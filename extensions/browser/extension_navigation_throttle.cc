// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_navigation_throttle.h"

#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/manifest_handlers/web_accessible_resources_info.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/permissions_data.h"

namespace extensions {

ExtensionNavigationThrottle::ExtensionNavigationThrottle(
    content::NavigationHandle* navigation_handle)
    : content::NavigationThrottle(navigation_handle) {}

ExtensionNavigationThrottle::~ExtensionNavigationThrottle() {}

content::NavigationThrottle::ThrottleCheckResult
ExtensionNavigationThrottle::WillStartRequest() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  GURL url(navigation_handle()->GetURL());
  ExtensionRegistry* registry = ExtensionRegistry::Get(
      navigation_handle()->GetWebContents()->GetBrowserContext());

  if (navigation_handle()->IsInMainFrame()) {
    // Block top-level navigations to blob: or filesystem: URLs with extension
    // origin from non-extension processes.  See https://crbug.com/645028.
    bool is_nested_url = url.SchemeIsFileSystem() || url.SchemeIsBlob();
    bool is_extension = false;
    if (registry) {
      is_extension = !!registry->enabled_extensions().GetExtensionOrAppByURL(
          navigation_handle()->GetStartingSiteInstance()->GetSiteURL());
    }

    url::Origin origin(url);
    if (is_nested_url && origin.scheme() == extensions::kExtensionScheme &&
        !is_extension) {
      // Relax this restriction for apps that use <webview>.  See
      // https://crbug.com/652077.
      const extensions::Extension* extension =
          registry->enabled_extensions().GetByID(origin.host());
      bool has_webview_permission =
          extension &&
          extension->permissions_data()->HasAPIPermission(
              extensions::APIPermission::kWebView);
      if (!has_webview_permission)
        return content::NavigationThrottle::CANCEL;
    }

    return content::NavigationThrottle::PROCEED;
  }

  // Now enforce web_accessible_resources for navigations. Top-level navigations
  // should always be allowed.

  // If the navigation is not to a chrome-extension:// URL, no need to perform
  // any more checks.
  if (!url.SchemeIs(extensions::kExtensionScheme))
    return content::NavigationThrottle::PROCEED;

  // The subframe which is navigated needs to have all of its ancestors be
  // at the same origin, otherwise the resource needs to be explicitly listed
  // in web_accessible_resources.
  // Since the RenderFrameHost is not known until navigation has committed,
  // we can't get it from NavigationHandle. However, this code only cares about
  // the ancestor chain, so find the current RenderFrameHost and use it to
  // traverse up to the main frame.
  content::RenderFrameHost* navigating_frame = nullptr;
  for (auto* frame : navigation_handle()->GetWebContents()->GetAllFrames()) {
    if (frame->GetFrameTreeNodeId() ==
        navigation_handle()->GetFrameTreeNodeId()) {
      navigating_frame = frame;
      break;
    }
  }
  DCHECK(navigating_frame);

  // Traverse the chain of parent frames, checking if they are the same origin
  // as the URL of this navigation.
  content::RenderFrameHost* ancestor = navigating_frame->GetParent();
  bool external_ancestor = false;
  while (ancestor) {
    if (ancestor->GetLastCommittedURL().GetOrigin() != url.GetOrigin()) {
      // Ignore DevTools, as it is allowed to embed extension pages.
      if (!ancestor->GetLastCommittedURL().SchemeIs(
              content::kChromeDevToolsScheme)) {
        external_ancestor = true;
        break;
      }
    }
    ancestor = ancestor->GetParent();
  }

  if (!external_ancestor)
    return content::NavigationThrottle::PROCEED;

  // Since there was at least one origin different than the navigation URL,
  // explicitly check for the resource in web_accessible_resources.
  std::string resource_path = url.path();
  if (!registry)
    return content::NavigationThrottle::BLOCK_REQUEST;

  const extensions::Extension* extension =
      registry->enabled_extensions().GetByID(url.host());
  if (!extension)
    return content::NavigationThrottle::BLOCK_REQUEST;

  if (WebAccessibleResourcesInfo::IsResourceWebAccessible(extension,
                                                          resource_path)) {
    return content::NavigationThrottle::PROCEED;
  }

  return content::NavigationThrottle::BLOCK_REQUEST;
}

}  // namespace extensions
