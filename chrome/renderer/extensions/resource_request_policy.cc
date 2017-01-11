// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/resource_request_policy.h"

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "chrome/common/extensions/chrome_manifest_url_handlers.h"
#include "chrome/common/url_constants.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "extensions/common/manifest_handlers/web_accessible_resources_info.h"
#include "extensions/common/manifest_handlers/webview_info.h"
#include "extensions/renderer/dispatcher.h"
#include "extensions/renderer/renderer_extension_registry.h"
#include "third_party/WebKit/public/platform/URLConversion.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/web/WebConsoleMessage.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace extensions {

ResourceRequestPolicy::ResourceRequestPolicy(Dispatcher* dispatcher)
    : dispatcher_(dispatcher) {}

// This method does a security check whether chrome-extension:// URLs can be
// requested by the renderer. Since this is in an untrusted process, the browser
// has a similar check to enforce the policy, in case this process is exploited.
// If you are changing this function, ensure equivalent checks are added to
// extension_protocols.cc's AllowExtensionResourceLoad.
bool ResourceRequestPolicy::CanRequestResource(
    const GURL& resource_url,
    blink::WebLocalFrame* frame,
    ui::PageTransition transition_type) {
  CHECK(resource_url.SchemeIs(kExtensionScheme));

  const Extension* extension =
      RendererExtensionRegistry::Get()->GetExtensionOrAppByURL(resource_url);
  if (!extension) {
    // Allow the load in the case of a non-existent extension. We'll just get a
    // 404 from the browser process.
    return true;
  }

  // Disallow loading of packaged resources for hosted apps. We don't allow
  // hybrid hosted/packaged apps. The one exception is access to icons, since
  // some extensions want to be able to do things like create their own
  // launchers.
  base::StringPiece resource_root_relative_path =
      resource_url.path_piece().empty() ? base::StringPiece()
                                        : resource_url.path_piece().substr(1);
  if (extension->is_hosted_app() &&
      !IconsInfo::GetIcons(extension)
          .ContainsPath(resource_root_relative_path)) {
    LOG(ERROR) << "Denying load of " << resource_url.spec() << " from "
               << "hosted app.";
    return false;
  }

  // Disallow loading of extension resources which are not explicitly listed
  // as web or WebView accessible if the manifest version is 2 or greater.
  if (!WebAccessibleResourcesInfo::IsResourceWebAccessible(
          extension, resource_url.path()) &&
      !WebviewInfo::IsResourceWebviewAccessible(
          extension, dispatcher_->webview_partition_id(),
          resource_url.path())) {
    GURL frame_url = frame->document().url();

    // The page_origin may be GURL("null") for unique origins like data URLs,
    // but this is ok for the checks below.  We only care if it matches the
    // current extension or has a devtools scheme.
    GURL page_origin = url::Origin(frame->top()->getSecurityOrigin()).GetURL();

    // Exceptions are:
    // - empty origin (needed for some edge cases when we have empty origins)
    bool is_empty_origin = frame_url.is_empty();
    // - extensions requesting their own resources (frame_url check is for
    //     images, page_url check is for iframes)
    bool is_own_resource = frame_url.GetOrigin() == extension->url() ||
                           page_origin == extension->url();
    // - devtools (chrome-extension:// URLs are loaded into frames of devtools
    //     to support the devtools extension APIs)
    bool is_dev_tools =
        page_origin.SchemeIs(content::kChromeDevToolsScheme) &&
        !chrome_manifest_urls::GetDevToolsPage(extension).is_empty();
    bool transition_allowed =
        !ui::PageTransitionIsWebTriggerable(transition_type);
    // - unreachable web page error page (to allow showing the icon of the
    //   unreachable app on this page)
    bool is_error_page = frame_url == content::kUnreachableWebDataURL;

    if (!is_empty_origin && !is_own_resource &&
        !is_dev_tools && !transition_allowed && !is_error_page) {
      std::string message = base::StringPrintf(
          "Denying load of %s. Resources must be listed in the "
          "web_accessible_resources manifest key in order to be loaded by "
          "pages outside the extension.",
          resource_url.spec().c_str());
      frame->addMessageToConsole(
          blink::WebConsoleMessage(blink::WebConsoleMessage::LevelError,
                                    blink::WebString::fromUTF8(message)));
      return false;
    }
  }

  return true;
}

}  // namespace extensions
