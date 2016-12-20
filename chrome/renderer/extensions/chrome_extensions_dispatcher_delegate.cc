// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/chrome_extensions_dispatcher_delegate.h"

#include <memory>

#include "base/command_line.h"
#include "base/sha1.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/crash_keys.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/renderer_resources.h"
#include "chrome/renderer/extensions/app_bindings.h"
#include "chrome/renderer/extensions/automation_internal_custom_bindings.h"
#include "chrome/renderer/extensions/media_galleries_custom_bindings.h"
#include "chrome/renderer/extensions/notifications_native_handler.h"
#include "chrome/renderer/extensions/page_capture_custom_bindings.h"
#include "chrome/renderer/extensions/sync_file_system_custom_bindings.h"
#include "chrome/renderer/extensions/webstore_bindings.h"
#include "components/version_info/version_info.h"
#include "content/public/common/content_switches.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/render_view.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/feature_switch.h"
#include "extensions/common/features/feature_channel.h"
#include "extensions/common/permissions/manifest_permission_set.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/switches.h"
#include "extensions/renderer/css_native_handler.h"
#include "extensions/renderer/dispatcher.h"
#include "extensions/renderer/i18n_custom_bindings.h"
#include "extensions/renderer/lazy_background_page_native_handler.h"
#include "extensions/renderer/native_handler.h"
#include "extensions/renderer/resource_bundle_source_map.h"
#include "extensions/renderer/script_context.h"
#include "media/media_features.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/web/WebSecurityPolicy.h"

#if BUILDFLAG(ENABLE_WEBRTC)
#include "chrome/renderer/extensions/cast_streaming_native_handler.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/renderer/extensions/file_browser_handler_custom_bindings.h"
#include "chrome/renderer/extensions/file_manager_private_custom_bindings.h"
#include "chrome/renderer/extensions/platform_keys_natives.h"
#endif

using extensions::NativeHandler;

ChromeExtensionsDispatcherDelegate::ChromeExtensionsDispatcherDelegate() {
}

ChromeExtensionsDispatcherDelegate::~ChromeExtensionsDispatcherDelegate() {
}

void ChromeExtensionsDispatcherDelegate::InitOriginPermissions(
    const extensions::Extension* extension,
    bool is_extension_active) {
  // Allow component extensions to access chrome://theme/.
  //
  // We don't want to grant these permissions to inactive component extensions,
  // to avoid granting them in "unblessed" (non-extension) processes.  If a
  // component extension somehow starts as inactive and becomes active later,
  // we'll re-init the origin permissions, so there's no danger in being
  // conservative.
  if (extensions::Manifest::IsComponentLocation(extension->location()) &&
      is_extension_active) {
    blink::WebSecurityPolicy::addOriginAccessWhitelistEntry(
        extension->url(), blink::WebString::fromUTF8(content::kChromeUIScheme),
        blink::WebString::fromUTF8(chrome::kChromeUIThemeHost), false);
  }

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
  module_system->RegisterNativeHandler(
      "app", std::unique_ptr<NativeHandler>(
                 new extensions::AppBindings(dispatcher, context)));
  module_system->RegisterNativeHandler(
      "sync_file_system",
      std::unique_ptr<NativeHandler>(
          new extensions::SyncFileSystemCustomBindings(context)));
#if defined(OS_CHROMEOS)
  module_system->RegisterNativeHandler(
      "file_browser_handler",
      std::unique_ptr<NativeHandler>(
          new extensions::FileBrowserHandlerCustomBindings(context)));
  module_system->RegisterNativeHandler(
      "file_manager_private",
      std::unique_ptr<NativeHandler>(
          new extensions::FileManagerPrivateCustomBindings(context)));
  module_system->RegisterNativeHandler(
      "platform_keys_natives",
      std::unique_ptr<NativeHandler>(
          new extensions::PlatformKeysNatives(context)));
#endif  // defined(OS_CHROMEOS)
  module_system->RegisterNativeHandler(
      "notifications_private",
      std::unique_ptr<NativeHandler>(
          new extensions::NotificationsNativeHandler(context)));
  module_system->RegisterNativeHandler(
      "mediaGalleries",
      std::unique_ptr<NativeHandler>(
          new extensions::MediaGalleriesCustomBindings(context)));
  module_system->RegisterNativeHandler(
      "page_capture", std::unique_ptr<NativeHandler>(
                          new extensions::PageCaptureCustomBindings(context)));
  module_system->RegisterNativeHandler(
      "webstore", std::unique_ptr<NativeHandler>(
                      new extensions::WebstoreBindings(context)));
#if BUILDFLAG(ENABLE_WEBRTC)
  module_system->RegisterNativeHandler(
      "cast_streaming_natives",
      std::unique_ptr<NativeHandler>(
          new extensions::CastStreamingNativeHandler(context)));
#endif
  module_system->RegisterNativeHandler(
      "automationInternal",
      std::unique_ptr<NativeHandler>(
          new extensions::AutomationInternalCustomBindings(context)));

  // The following are native handlers that are defined in //extensions, but
  // are only used for APIs defined in Chrome.
  // TODO(devlin): We should clean this up. If an API is defined in Chrome,
  // there's no reason to have its native handlers residing and being compiled
  // in //extensions.
  module_system->RegisterNativeHandler(
      "i18n", std::unique_ptr<NativeHandler>(
                  new extensions::I18NCustomBindings(context)));
  module_system->RegisterNativeHandler(
      "lazy_background_page",
      std::unique_ptr<NativeHandler>(
          new extensions::LazyBackgroundPageNativeHandler(context)));
  module_system->RegisterNativeHandler(
      "css_natives", std::unique_ptr<NativeHandler>(
                         new extensions::CssNativeHandler(context)));
}

void ChromeExtensionsDispatcherDelegate::PopulateSourceMap(
    extensions::ResourceBundleSourceMap* source_map) {
  // Custom bindings.
  source_map->RegisterSource("app", IDR_APP_CUSTOM_BINDINGS_JS);
  source_map->RegisterSource("automation", IDR_AUTOMATION_CUSTOM_BINDINGS_JS);
  source_map->RegisterSource("automationEvent", IDR_AUTOMATION_EVENT_JS);
  source_map->RegisterSource("automationNode", IDR_AUTOMATION_NODE_JS);
  source_map->RegisterSource("browserAction",
                             IDR_BROWSER_ACTION_CUSTOM_BINDINGS_JS);
  source_map->RegisterSource("declarativeContent",
                             IDR_DECLARATIVE_CONTENT_CUSTOM_BINDINGS_JS);
  source_map->RegisterSource("desktopCapture",
                             IDR_DESKTOP_CAPTURE_CUSTOM_BINDINGS_JS);
  source_map->RegisterSource("developerPrivate",
                             IDR_DEVELOPER_PRIVATE_CUSTOM_BINDINGS_JS);
  source_map->RegisterSource("downloads", IDR_DOWNLOADS_CUSTOM_BINDINGS_JS);
  source_map->RegisterSource("feedbackPrivate",
                             IDR_FEEDBACK_PRIVATE_CUSTOM_BINDINGS_JS);
  source_map->RegisterSource("fileSystem", IDR_FILE_SYSTEM_CUSTOM_BINDINGS_JS);
  source_map->RegisterSource("gcm", IDR_GCM_CUSTOM_BINDINGS_JS);
  source_map->RegisterSource("identity", IDR_IDENTITY_CUSTOM_BINDINGS_JS);
  source_map->RegisterSource("imageWriterPrivate",
                             IDR_IMAGE_WRITER_PRIVATE_CUSTOM_BINDINGS_JS);
  source_map->RegisterSource("input.ime", IDR_INPUT_IME_CUSTOM_BINDINGS_JS);
  source_map->RegisterSource("logPrivate", IDR_LOG_PRIVATE_CUSTOM_BINDINGS_JS);
  source_map->RegisterSource("mediaGalleries",
                             IDR_MEDIA_GALLERIES_CUSTOM_BINDINGS_JS);
  source_map->RegisterSource("notifications",
                             IDR_NOTIFICATIONS_CUSTOM_BINDINGS_JS);
  source_map->RegisterSource("omnibox", IDR_OMNIBOX_CUSTOM_BINDINGS_JS);
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

#if defined(OS_CHROMEOS)
  source_map->RegisterSource("certificateProvider",
                             IDR_CERTIFICATE_PROVIDER_CUSTOM_BINDINGS_JS);
  source_map->RegisterSource("enterprise.platformKeys",
                             IDR_ENTERPRISE_PLATFORM_KEYS_CUSTOM_BINDINGS_JS);
  source_map->RegisterSource("enterprise.platformKeys.internalAPI",
                             IDR_ENTERPRISE_PLATFORM_KEYS_INTERNAL_API_JS);
  source_map->RegisterSource("enterprise.platformKeys.KeyPair",
                             IDR_ENTERPRISE_PLATFORM_KEYS_KEY_PAIR_JS);
  source_map->RegisterSource("enterprise.platformKeys.SubtleCrypto",
                             IDR_ENTERPRISE_PLATFORM_KEYS_SUBTLE_CRYPTO_JS);
  source_map->RegisterSource("enterprise.platformKeys.Token",
                             IDR_ENTERPRISE_PLATFORM_KEYS_TOKEN_JS);
  source_map->RegisterSource("fileBrowserHandler",
                             IDR_FILE_BROWSER_HANDLER_CUSTOM_BINDINGS_JS);
  source_map->RegisterSource("fileManagerPrivate",
                             IDR_FILE_MANAGER_PRIVATE_CUSTOM_BINDINGS_JS);
  source_map->RegisterSource("fileSystemProvider",
                             IDR_FILE_SYSTEM_PROVIDER_CUSTOM_BINDINGS_JS);
  source_map->RegisterSource("platformKeys",
                             IDR_PLATFORM_KEYS_CUSTOM_BINDINGS_JS);
  source_map->RegisterSource("platformKeys.getPublicKey",
                             IDR_PLATFORM_KEYS_GET_PUBLIC_KEY_JS);
  source_map->RegisterSource("platformKeys.internalAPI",
                             IDR_PLATFORM_KEYS_INTERNAL_API_JS);
  source_map->RegisterSource("platformKeys.Key", IDR_PLATFORM_KEYS_KEY_JS);
  source_map->RegisterSource("platformKeys.SubtleCrypto",
                             IDR_PLATFORM_KEYS_SUBTLE_CRYPTO_JS);
  source_map->RegisterSource("platformKeys.utils", IDR_PLATFORM_KEYS_UTILS_JS);
  source_map->RegisterSource("terminalPrivate",
                             IDR_TERMINAL_PRIVATE_CUSTOM_BINDINGS_JS);
#endif  // defined(OS_CHROMEOS)

#if BUILDFLAG(ENABLE_WEBRTC)
  source_map->RegisterSource("cast.streaming.rtpStream",
                             IDR_CAST_STREAMING_RTP_STREAM_CUSTOM_BINDINGS_JS);
  source_map->RegisterSource("cast.streaming.session",
                             IDR_CAST_STREAMING_SESSION_CUSTOM_BINDINGS_JS);
  source_map->RegisterSource(
      "cast.streaming.udpTransport",
      IDR_CAST_STREAMING_UDP_TRANSPORT_CUSTOM_BINDINGS_JS);
  source_map->RegisterSource(
      "cast.streaming.receiverSession",
      IDR_CAST_STREAMING_RECEIVER_SESSION_CUSTOM_BINDINGS_JS);
#endif
  source_map->RegisterSource(
      "webrtcDesktopCapturePrivate",
      IDR_WEBRTC_DESKTOP_CAPTURE_PRIVATE_CUSTOM_BINDINGS_JS);
  source_map->RegisterSource("webstore", IDR_WEBSTORE_CUSTOM_BINDINGS_JS);

  // Custom types sources.
  source_map->RegisterSource("ChromeSetting", IDR_CHROME_SETTING_JS);
  source_map->RegisterSource("ContentSetting", IDR_CONTENT_SETTING_JS);
  source_map->RegisterSource("ChromeDirectSetting",
                             IDR_CHROME_DIRECT_SETTING_JS);

  // Platform app sources that are not API-specific..
  source_map->RegisterSource("fileEntryBindingUtil",
                             IDR_FILE_ENTRY_BINDING_UTIL_JS);
  source_map->RegisterSource("chromeWebViewInternal",
                             IDR_CHROME_WEB_VIEW_INTERNAL_CUSTOM_BINDINGS_JS);
  source_map->RegisterSource("chromeWebView", IDR_CHROME_WEB_VIEW_JS);
}

void ChromeExtensionsDispatcherDelegate::RequireAdditionalModules(
    extensions::ScriptContext* context) {
  // Note: setting up the WebView class here, not the chrome.webview API.
  // The API will be automatically set up when first used.
  if (context->GetAvailability("webViewInternal").is_available()) {
    // TODO(fsamuel): Eagerly calling Require on context startup is expensive.
    // It would be better if there were a light way of detecting when a webview
    // or appview is created and only then set up the infrastructure.
    context->module_system()->Require("chromeWebView");
  }
}

void ChromeExtensionsDispatcherDelegate::OnActiveExtensionsUpdated(
    const std::set<std::string>& extension_ids) {
  // In single-process mode, the browser process reports the active extensions.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          ::switches::kSingleProcess))
    return;
  crash_keys::SetActiveExtensions(extension_ids);
}
