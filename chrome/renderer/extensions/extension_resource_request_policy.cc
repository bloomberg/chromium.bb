// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/extension_resource_request_policy.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/stringprintf.h"
#include "base/string_util.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_set.h"
#include "googleurl/src/gurl.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebConsoleMessage.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebString.h"

using extensions::Extension;

// static
bool ExtensionResourceRequestPolicy::CanRequestResource(
    const GURL& resource_url,
    WebKit::WebFrame* frame,
    const ExtensionSet* loaded_extensions) {
  CHECK(resource_url.SchemeIs(chrome::kExtensionScheme));

  const Extension* extension =
      loaded_extensions->GetExtensionOrAppByURL(ExtensionURLInfo(resource_url));
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
      resource_url.path().empty() ? "" : resource_url.path().substr(1);
  if (extension->is_hosted_app() &&
      !extension->icons().ContainsPath(resource_root_relative_path)) {
    LOG(ERROR) << "Denying load of " << resource_url.spec() << " from "
               << "hosted app.";
    return false;
  }

  GURL frame_url = frame->document().url();

  // In the case of loading a frame, frame* points to the frame being loaded,
  // not the containing frame. This means that frame->document().url() ends up
  // not being useful to us.
  //
  // WebKit doesn't currently pass us enough information to know when we're a
  // frame, so we hack it by checking for 'about:blank', which should only
  // happen in this situation.
  //
  // TODO(aa): Fix WebKit to pass the context of the load: crbug.com/139788.
  if (frame_url == GURL(chrome::kAboutBlankURL) && frame->parent())
      frame_url = frame->parent()->document().url();

  bool extension_has_access_to_frame =
      extension->GetEffectiveHostPermissions().MatchesURL(frame_url);
  bool frame_has_empty_origin = frame_url.is_empty();
  bool frame_is_data_url = frame_url.SchemeIs(chrome::kDataScheme);
  bool frame_is_devtools = frame_url.SchemeIs(chrome::kChromeDevToolsScheme) &&
      !extension->devtools_url().is_empty();
  bool frame_is_extension = frame_url.SchemeIs(chrome::kExtensionScheme);
  bool is_own_resource = frame_url.GetOrigin() == extension->url();
  bool is_resource_nacl_manifest =
      extension->IsResourceNaClManifest(resource_url.path());
  bool is_resource_web_accessible =
      extension->IsResourceWebAccessible(resource_url.path()) ||
      CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableExtensionsResourceWhitelist);

  // Given that the goal here is to prevent malicious injection of a benign
  // extension's content into a context where it might be damaging, allowing
  // unvalidated "nexe" resources is low-risk. If a mechanism for synchronously
  // validating that the "nexe" is a NaCl executable appears, we should use it.
  bool is_resource_nexe = extension->HasNaClModules() &&
      EndsWith(resource_url.path(), ".nexe", true);

  if (!frame_has_empty_origin && !frame_is_devtools && !is_own_resource) {
    if (!is_resource_web_accessible) {
      std::string message = base::StringPrintf(
          "Denying load of %s. Resources must be listed in the "
          "web_accessible_resources manifest key in order to be loaded by "
          "pages outside the extension.",
          resource_url.spec().c_str());
      frame->addMessageToConsole(
          WebKit::WebConsoleMessage(WebKit::WebConsoleMessage::LevelError,
                                    WebKit::WebString::fromUTF8(message)));
      return false;
    }

    if (!extension_has_access_to_frame && !frame_is_extension &&
        !frame_is_data_url && !is_resource_nacl_manifest && !is_resource_nexe) {
      std::string message = base::StringPrintf(
          "Denying load of %s. An extension's resources can only be loaded "
          "into a page for which the extension has explicit host permissions.",
          resource_url.spec().c_str());
      frame->addMessageToConsole(
          WebKit::WebConsoleMessage(WebKit::WebConsoleMessage::LevelError,
                                    WebKit::WebString::fromUTF8(message)));
      return false;
    }
  }

  return true;
}

// static
bool ExtensionResourceRequestPolicy::CanRequestExtensionResourceScheme(
    const GURL& resource_url,
    WebKit::WebFrame* frame) {
  CHECK(resource_url.SchemeIs(chrome::kExtensionResourceScheme));

  GURL frame_url = frame->document().url();
  if (!frame_url.is_empty() &&
      !frame_url.SchemeIs(chrome::kExtensionScheme)) {
    std::string message = base::StringPrintf(
        "Denying load of %s. chrome-extension-resources:// can only be "
        "loaded from extensions.",
      resource_url.spec().c_str());
    frame->addMessageToConsole(
        WebKit::WebConsoleMessage(WebKit::WebConsoleMessage::LevelError,
                                  WebKit::WebString::fromUTF8(message)));
    return false;
  }

  return true;
}

ExtensionResourceRequestPolicy::ExtensionResourceRequestPolicy() {
}
