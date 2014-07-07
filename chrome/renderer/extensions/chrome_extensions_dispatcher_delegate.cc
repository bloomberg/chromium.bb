// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/chrome_extensions_dispatcher_delegate.h"

#include "base/command_line.h"
#include "base/sha1.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/crash_keys.h"
#include "chrome/common/extensions/features/feature_channel.h"
#include "chrome/common/url_constants.h"
#include "chrome/renderer/extensions/app_bindings.h"
#include "chrome/renderer/extensions/app_window_custom_bindings.h"
#include "chrome/renderer/extensions/automation_internal_custom_bindings.h"
#include "chrome/renderer/extensions/chrome_v8_context.h"
#include "chrome/renderer/extensions/enterprise_platform_keys_natives.h"
#include "chrome/renderer/extensions/file_browser_handler_custom_bindings.h"
#include "chrome/renderer/extensions/file_browser_private_custom_bindings.h"
#include "chrome/renderer/extensions/media_galleries_custom_bindings.h"
#include "chrome/renderer/extensions/notifications_native_handler.h"
#include "chrome/renderer/extensions/page_actions_custom_bindings.h"
#include "chrome/renderer/extensions/page_capture_custom_bindings.h"
#include "chrome/renderer/extensions/sync_file_system_custom_bindings.h"
#include "chrome/renderer/extensions/tab_finder.h"
#include "chrome/renderer/extensions/tabs_custom_bindings.h"
#include "chrome/renderer/extensions/webstore_bindings.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/render_view.h"
#include "extensions/common/extension.h"
#include "extensions/common/permissions/api_permission_set.h"
#include "extensions/common/permissions/manifest_permission_set.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/url_pattern_set.h"
#include "extensions/renderer/dispatcher.h"
#include "extensions/renderer/native_handler.h"
#include "extensions/renderer/resource_bundle_source_map.h"
#include "extensions/renderer/script_context.h"
#include "grit/renderer_resources.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebSecurityPolicy.h"
#include "third_party/WebKit/public/web/WebView.h"

#if defined(ENABLE_WEBRTC)
#include "chrome/renderer/extensions/cast_streaming_native_handler.h"
#endif

using extensions::NativeHandler;

ChromeExtensionsDispatcherDelegate::ChromeExtensionsDispatcherDelegate()
    : webrequest_used_(false) {
}

ChromeExtensionsDispatcherDelegate::~ChromeExtensionsDispatcherDelegate() {
}

scoped_ptr<extensions::ScriptContext>
ChromeExtensionsDispatcherDelegate::CreateScriptContext(
    const v8::Handle<v8::Context>& v8_context,
    blink::WebFrame* frame,
    const extensions::Extension* extension,
    extensions::Feature::Context context_type) {
  return scoped_ptr<extensions::ScriptContext>(new extensions::ChromeV8Context(
      v8_context, frame, extension, context_type));
}

void ChromeExtensionsDispatcherDelegate::InitOriginPermissions(
    const extensions::Extension* extension,
    bool is_extension_active) {
  // TODO(jstritar): We should try to remove this special case. Also, these
  // whitelist entries need to be updated when the kManagement permission
  // changes.
  if (is_extension_active &&
      extension->permissions_data()->HasAPIPermission(
          extensions::APIPermission::kManagement)) {
    blink::WebSecurityPolicy::addOriginAccessWhitelistEntry(
        extension->url(),
        blink::WebString::fromUTF8(content::kChromeUIScheme),
        blink::WebString::fromUTF8(chrome::kChromeUIExtensionIconHost),
        false);
  }
}

void ChromeExtensionsDispatcherDelegate::RegisterNativeHandlers(
    extensions::Dispatcher* dispatcher,
    extensions::ModuleSystem* module_system,
    extensions::ScriptContext* context) {
#if !defined(ENABLE_EXTENSIONS)
  return;
#endif
  module_system->RegisterNativeHandler(
      "app",
      scoped_ptr<NativeHandler>(
          new extensions::AppBindings(dispatcher, context)));
  module_system->RegisterNativeHandler(
      "app_window_natives",
      scoped_ptr<NativeHandler>(
          new extensions::AppWindowCustomBindings(dispatcher, context)));
  module_system->RegisterNativeHandler(
      "sync_file_system",
      scoped_ptr<NativeHandler>(
          new extensions::SyncFileSystemCustomBindings(context)));
  module_system->RegisterNativeHandler(
      "enterprise_platform_keys_natives",
      scoped_ptr<NativeHandler>(
          new extensions::EnterprisePlatformKeysNatives(context)));
  module_system->RegisterNativeHandler(
      "file_browser_handler",
      scoped_ptr<NativeHandler>(
          new extensions::FileBrowserHandlerCustomBindings(context)));
  module_system->RegisterNativeHandler(
      "file_browser_private",
      scoped_ptr<NativeHandler>(
          new extensions::FileBrowserPrivateCustomBindings(context)));
  module_system->RegisterNativeHandler(
      "notifications_private",
      scoped_ptr<NativeHandler>(
          new extensions::NotificationsNativeHandler(context)));
  module_system->RegisterNativeHandler(
      "mediaGalleries",
      scoped_ptr<NativeHandler>(
          new extensions::MediaGalleriesCustomBindings(context)));
  module_system->RegisterNativeHandler(
      "page_actions",
      scoped_ptr<NativeHandler>(
          new extensions::PageActionsCustomBindings(dispatcher, context)));
  module_system->RegisterNativeHandler(
      "page_capture",
      scoped_ptr<NativeHandler>(
          new extensions::PageCaptureCustomBindings(context)));
  module_system->RegisterNativeHandler(
      "tabs",
      scoped_ptr<NativeHandler>(new extensions::TabsCustomBindings(context)));
  module_system->RegisterNativeHandler(
      "webstore",
      scoped_ptr<NativeHandler>(new extensions::WebstoreBindings(context)));
#if defined(ENABLE_WEBRTC)
  module_system->RegisterNativeHandler(
      "cast_streaming_natives",
      scoped_ptr<NativeHandler>(
          new extensions::CastStreamingNativeHandler(context)));
#endif
  module_system->RegisterNativeHandler(
      "automationInternal",
      scoped_ptr<NativeHandler>(
          new extensions::AutomationInternalCustomBindings(context)));
}

void ChromeExtensionsDispatcherDelegate::PopulateSourceMap(
    extensions::ResourceBundleSourceMap* source_map) {
  // Custom bindings.
  source_map->RegisterSource("app", IDR_APP_CUSTOM_BINDINGS_JS);
  source_map->RegisterSource("app.window", IDR_APP_WINDOW_CUSTOM_BINDINGS_JS);
  source_map->RegisterSource("automation", IDR_AUTOMATION_CUSTOM_BINDINGS_JS);
  source_map->RegisterSource("automationEvent", IDR_AUTOMATION_EVENT_JS);
  source_map->RegisterSource("automationNode", IDR_AUTOMATION_NODE_JS);
  source_map->RegisterSource("browserAction",
                             IDR_BROWSER_ACTION_CUSTOM_BINDINGS_JS);
  source_map->RegisterSource("declarativeContent",
                             IDR_DECLARATIVE_CONTENT_CUSTOM_BINDINGS_JS);
  source_map->RegisterSource("declarativeWebRequest",
                             IDR_DECLARATIVE_WEBREQUEST_CUSTOM_BINDINGS_JS);
  source_map->RegisterSource("desktopCapture",
                             IDR_DESKTOP_CAPTURE_CUSTOM_BINDINGS_JS);
  source_map->RegisterSource("developerPrivate",
                             IDR_DEVELOPER_PRIVATE_CUSTOM_BINDINGS_JS);
  source_map->RegisterSource("downloads", IDR_DOWNLOADS_CUSTOM_BINDINGS_JS);
  source_map->RegisterSource("enterprise.platformKeys",
                             IDR_ENTERPRISE_PLATFORM_KEYS_CUSTOM_BINDINGS_JS);
  source_map->RegisterSource("enterprise.platformKeys.internalAPI",
                             IDR_ENTERPRISE_PLATFORM_KEYS_INTERNAL_API_JS);
  source_map->RegisterSource("enterprise.platformKeys.Key",
                             IDR_ENTERPRISE_PLATFORM_KEYS_KEY_JS);
  source_map->RegisterSource("enterprise.platformKeys.KeyPair",
                             IDR_ENTERPRISE_PLATFORM_KEYS_KEY_PAIR_JS);
  source_map->RegisterSource("enterprise.platformKeys.SubtleCrypto",
                             IDR_ENTERPRISE_PLATFORM_KEYS_SUBTLE_CRYPTO_JS);
  source_map->RegisterSource("enterprise.platformKeys.Token",
                             IDR_ENTERPRISE_PLATFORM_KEYS_TOKEN_JS);
  source_map->RegisterSource("enterprise.platformKeys.utils",
                             IDR_ENTERPRISE_PLATFORM_KEYS_UTILS_JS);
  source_map->RegisterSource("feedbackPrivate",
                             IDR_FEEDBACK_PRIVATE_CUSTOM_BINDINGS_JS);
  source_map->RegisterSource("fileBrowserHandler",
                             IDR_FILE_BROWSER_HANDLER_CUSTOM_BINDINGS_JS);
  source_map->RegisterSource("fileBrowserPrivate",
                             IDR_FILE_BROWSER_PRIVATE_CUSTOM_BINDINGS_JS);
  source_map->RegisterSource("fileSystem", IDR_FILE_SYSTEM_CUSTOM_BINDINGS_JS);
  source_map->RegisterSource("fileSystemProvider",
                             IDR_FILE_SYSTEM_PROVIDER_CUSTOM_BINDINGS_JS);
  source_map->RegisterSource("gcm", IDR_GCM_CUSTOM_BINDINGS_JS);
  source_map->RegisterSource("identity", IDR_IDENTITY_CUSTOM_BINDINGS_JS);
  source_map->RegisterSource("imageWriterPrivate",
                             IDR_IMAGE_WRITER_PRIVATE_CUSTOM_BINDINGS_JS);
  source_map->RegisterSource("input.ime", IDR_INPUT_IME_CUSTOM_BINDINGS_JS);
  source_map->RegisterSource("mediaGalleries",
                             IDR_MEDIA_GALLERIES_CUSTOM_BINDINGS_JS);
  source_map->RegisterSource("notifications",
                             IDR_NOTIFICATIONS_CUSTOM_BINDINGS_JS);
  source_map->RegisterSource("omnibox", IDR_OMNIBOX_CUSTOM_BINDINGS_JS);
  source_map->RegisterSource("pageActions",
                             IDR_PAGE_ACTIONS_CUSTOM_BINDINGS_JS);
  source_map->RegisterSource("pageAction", IDR_PAGE_ACTION_CUSTOM_BINDINGS_JS);
  source_map->RegisterSource("pageCapture",
                             IDR_PAGE_CAPTURE_CUSTOM_BINDINGS_JS);
  source_map->RegisterSource("syncFileSystem",
                             IDR_SYNC_FILE_SYSTEM_CUSTOM_BINDINGS_JS);
  source_map->RegisterSource("systemIndicator",
                             IDR_SYSTEM_INDICATOR_CUSTOM_BINDINGS_JS);
  source_map->RegisterSource("tabCapture", IDR_TAB_CAPTURE_CUSTOM_BINDINGS_JS);
  source_map->RegisterSource("tabs", IDR_TABS_CUSTOM_BINDINGS_JS);
  source_map->RegisterSource("tts", IDR_TTS_CUSTOM_BINDINGS_JS);
  source_map->RegisterSource("ttsEngine", IDR_TTS_ENGINE_CUSTOM_BINDINGS_JS);
  source_map->RegisterSource("webRequest", IDR_WEB_REQUEST_CUSTOM_BINDINGS_JS);
  source_map->RegisterSource("webRequestInternal",
                             IDR_WEB_REQUEST_INTERNAL_CUSTOM_BINDINGS_JS);
#if defined(ENABLE_WEBRTC)
  source_map->RegisterSource("cast.streaming.rtpStream",
                             IDR_CAST_STREAMING_RTP_STREAM_CUSTOM_BINDINGS_JS);
  source_map->RegisterSource("cast.streaming.session",
                             IDR_CAST_STREAMING_SESSION_CUSTOM_BINDINGS_JS);
  source_map->RegisterSource(
      "cast.streaming.udpTransport",
      IDR_CAST_STREAMING_UDP_TRANSPORT_CUSTOM_BINDINGS_JS);
#endif
  source_map->RegisterSource("webstore", IDR_WEBSTORE_CUSTOM_BINDINGS_JS);
  source_map->RegisterSource("windowControls", IDR_WINDOW_CONTROLS_JS);

  // Custom types sources.
  source_map->RegisterSource("ChromeSetting", IDR_CHROME_SETTING_JS);
  source_map->RegisterSource("ContentSetting", IDR_CONTENT_SETTING_JS);
  source_map->RegisterSource("ChromeDirectSetting",
                             IDR_CHROME_DIRECT_SETTING_JS);

  // Platform app sources that are not API-specific..
  source_map->RegisterSource("appView", IDR_APP_VIEW_JS);
  source_map->RegisterSource("tagWatcher", IDR_TAG_WATCHER_JS);
  source_map->RegisterSource("webViewInternal",
                             IDR_WEB_VIEW_INTERNAL_CUSTOM_BINDINGS_JS);
  // Note: webView not webview so that this doesn't interfere with the
  // chrome.webview API bindings.
  source_map->RegisterSource("webView", IDR_WEB_VIEW_JS);
  source_map->RegisterSource("webViewEvents", IDR_WEB_VIEW_EVENTS_JS);
  source_map->RegisterSource("webViewExperimental",
                             IDR_WEB_VIEW_EXPERIMENTAL_JS);
  source_map->RegisterSource("webViewRequest",
                             IDR_WEB_VIEW_REQUEST_CUSTOM_BINDINGS_JS);
  source_map->RegisterSource("denyAppView", IDR_APP_VIEW_DENY_JS);
  source_map->RegisterSource("denyWebView", IDR_WEB_VIEW_DENY_JS);
  source_map->RegisterSource("injectAppTitlebar", IDR_INJECT_APP_TITLEBAR_JS);
}

void ChromeExtensionsDispatcherDelegate::RequireAdditionalModules(
    extensions::ModuleSystem* module_system,
    const extensions::Extension* extension,
    extensions::Feature::Context context_type,
    bool is_within_platform_app) {
  // TODO(kalman, fsamuel): Eagerly calling Require on context startup is
  // expensive. It would be better if there were a light way of detecting when
  // a webview or appview is created and only then set up the infrastructure.
  if (context_type == extensions::Feature::BLESSED_EXTENSION_CONTEXT &&
      is_within_platform_app &&
      extensions::GetCurrentChannel() <= chrome::VersionInfo::CHANNEL_DEV &&
      CommandLine::ForCurrentProcess()->HasSwitch(
          ::switches::kEnableAppWindowControls)) {
    module_system->Require("windowControls");
  }

  // We used to limit WebView to |BLESSED_EXTENSION_CONTEXT| within platform
  // apps. An ext/app runs in a blessed extension context, if it is the active
  // extension in the current process, in other words, if it is loaded in a top
  // frame. To support webview in a non-frame extension, we have to allow
  // unblessed extension context as well.
  // Note: setting up the WebView class here, not the chrome.webview API.
  // The API will be automatically set up when first used.
  if (context_type == extensions::Feature::BLESSED_EXTENSION_CONTEXT ||
      context_type == extensions::Feature::UNBLESSED_EXTENSION_CONTEXT) {
    if (extension->permissions_data()->HasAPIPermission(
            extensions::APIPermission::kWebView)) {
      module_system->Require("webView");
      if (extensions::GetCurrentChannel() <= chrome::VersionInfo::CHANNEL_DEV) {
        module_system->Require("webViewExperimental");
      } else {
        // TODO(asargent) We need a whitelist for webview experimental.
        // crbug.com/264852
        std::string id_hash = base::SHA1HashString(extension->id());
        std::string hexencoded_id_hash =
            base::HexEncode(id_hash.c_str(), id_hash.length());
        if (hexencoded_id_hash == "8C3741E3AF0B93B6E8E0DDD499BB0B74839EA578" ||
            hexencoded_id_hash == "E703483CEF33DEC18B4B6DD84B5C776FB9182BDB" ||
            hexencoded_id_hash == "1A26E32DE447A17CBE5E9750CDBA78F58539B39C" ||
            hexencoded_id_hash == "59048028102D7B4C681DBC7BC6CD980C3DC66DA3") {
          module_system->Require("webViewExperimental");
        }
      }
    } else {
      module_system->Require("denyWebView");
    }
  }

  if (context_type == extensions::Feature::BLESSED_EXTENSION_CONTEXT) {
    if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kEnableAppView) &&
        extension->permissions_data()->HasAPIPermission(
            extensions::APIPermission::kAppView)) {
      module_system->Require("appView");
    } else {
      module_system->Require("denyAppView");
    }
  }
}

void ChromeExtensionsDispatcherDelegate::OnActiveExtensionsUpdated(
    const std::set<std::string>& extension_ids) {
  // In single-process mode, the browser process reports the active extensions.
  if (CommandLine::ForCurrentProcess()->HasSwitch(::switches::kSingleProcess))
    return;
  crash_keys::SetActiveExtensions(extension_ids);
}

void ChromeExtensionsDispatcherDelegate::SetChannel(int channel) {
  extensions::SetCurrentChannel(
      static_cast<chrome::VersionInfo::Channel>(channel));
}

void ChromeExtensionsDispatcherDelegate::ClearTabSpecificPermissions(
    const extensions::Dispatcher* dispatcher,
    int tab_id,
    const std::vector<std::string>& extension_ids) {
  for (std::vector<std::string>::const_iterator it = extension_ids.begin();
       it != extension_ids.end();
       ++it) {
    const extensions::Extension* extension =
        dispatcher->extensions()->GetByID(*it);
    if (extension)
      extension->permissions_data()->ClearTabSpecificPermissions(tab_id);
  }
}

void ChromeExtensionsDispatcherDelegate::UpdateTabSpecificPermissions(
    const extensions::Dispatcher* dispatcher,
    const GURL& url,
    int tab_id,
    const std::string& extension_id,
    const extensions::URLPatternSet& origin_set) {
  content::RenderView* view = extensions::TabFinder::Find(tab_id);

  // For now, the message should only be sent to the render view that contains
  // the target tab. This may change. Either way, if this is the target tab it
  // gives us the chance to check against the URL to avoid races.
  DCHECK(view);
  GURL active_url(view->GetWebView()->mainFrame()->document().url());
  if (active_url != url)
    return;

  const extensions::Extension* extension =
      dispatcher->extensions()->GetByID(extension_id);
  if (!extension)
    return;

  extension->permissions_data()->UpdateTabSpecificPermissions(
      tab_id,
      new extensions::PermissionSet(extensions::APIPermissionSet(),
                                    extensions::ManifestPermissionSet(),
                                    origin_set,
                                    extensions::URLPatternSet()));
}

void ChromeExtensionsDispatcherDelegate::HandleWebRequestAPIUsage(
    bool webrequest_used) {
  webrequest_used_ = webrequest_used;
}
