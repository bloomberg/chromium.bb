// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/resource_request_policy.h"

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "chrome/common/extensions/manifest_url_handler.h"
#include "chrome/common/url_constants.h"
#include "content/public/common/page_transition_types.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "extensions/common/manifest_handlers/web_accessible_resources_info.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/web/WebConsoleMessage.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "url/gurl.h"

namespace extensions {

// This method does a security check whether chrome-extension:// URLs can be
// requested by the renderer. Since this is in an untrusted process, the browser
// has a similar check to enforce the policy, in case this process is exploited.
// If you are changing this function, ensure equivalent checks are added to
// extension_protocols.cc's AllowExtensionResourceLoad.

// static
bool ResourceRequestPolicy::CanRequestResource(
    const GURL& resource_url,
    blink::WebFrame* frame,
    content::PageTransition transition_type,
    const ExtensionSet* loaded_extensions) {
  CHECK(resource_url.SchemeIs(extensions::kExtensionScheme));

  const Extension* extension =
      loaded_extensions->GetExtensionOrAppByURL(resource_url);
  if (!extension) {
    // Allow the load in the case of a non-existent extension. We'll just get a
    // 404 from the browser process.
    return true;
  }

  // Disallow loading of packaged resources for hosted apps. We don't allow
  // hybrid hosted/packaged apps. The one exception is access to icons, since
  // some extensions want to be able to do things like create their own
  // launchers.
  std::string resource_root_relative_path =
      resource_url.path().empty() ? std::string()
                                  : resource_url.path().substr(1);
  if (extension->is_hosted_app() &&
      !IconsInfo::GetIcons(extension)
          .ContainsPath(resource_root_relative_path)) {
    LOG(ERROR) << "Denying load of " << resource_url.spec() << " from "
               << "hosted app.";
    return false;
  }

  // Disallow loading of extension resources which are not explicitly listed
  // as web accessible if the manifest version is 2 or greater.
  if (!WebAccessibleResourcesInfo::IsResourceWebAccessible(
          extension, resource_url.path())) {
    GURL frame_url = frame->document().url();
    GURL page_url = frame->top()->document().url();

    // Exceptions are:
    // - empty origin (needed for some edge cases when we have empty origins)
    bool is_empty_origin = frame_url.is_empty();
    // - extensions requesting their own resources (frame_url check is for
    //     images, page_url check is for iframes)
    bool is_own_resource = frame_url.GetOrigin() == extension->url() ||
        page_url.GetOrigin() == extension->url();
    // - devtools (chrome-extension:// URLs are loaded into frames of devtools
    //     to support the devtools extension APIs)
    bool is_dev_tools = page_url.SchemeIs(content::kChromeDevToolsScheme) &&
                        !ManifestURL::GetDevToolsPage(extension).is_empty();
    bool transition_allowed =
        !content::PageTransitionIsWebTriggerable(transition_type);
    // - unreachable web page error page (to allow showing the icon of the
    //   unreachable app on this page)
    bool is_error_page = frame_url == GURL(content::kUnreachableWebDataURL);

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

// static
bool ResourceRequestPolicy::CanRequestExtensionResourceScheme(
    const GURL& resource_url,
    blink::WebFrame* frame) {
  CHECK(resource_url.SchemeIs(extensions::kExtensionResourceScheme));

  GURL frame_url = frame->document().url();
  if (!frame_url.is_empty() &&
      !frame_url.SchemeIs(extensions::kExtensionScheme)) {
    std::string message = base::StringPrintf(
        "Denying load of %s. chrome-extension-resources:// can only be "
        "loaded from extensions.",
      resource_url.spec().c_str());
    frame->addMessageToConsole(
        blink::WebConsoleMessage(blink::WebConsoleMessage::LevelError,
                                  blink::WebString::fromUTF8(message)));
    return false;
  }

  return true;
}

ResourceRequestPolicy::ResourceRequestPolicy() {
}

}  // namespace extensions
