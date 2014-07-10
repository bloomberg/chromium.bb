// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/browser_context_keyed_service_factories.h"

#include "chrome/browser/extensions/activity_log/activity_log.h"
#include "chrome/browser/extensions/api/activity_log_private/activity_log_private_api.h"
#include "chrome/browser/extensions/api/alarms/alarm_manager.h"
#include "chrome/browser/extensions/api/audio/audio_api.h"
#include "chrome/browser/extensions/api/bluetooth/bluetooth_api.h"
#include "chrome/browser/extensions/api/bluetooth/bluetooth_private_api.h"
#include "chrome/browser/extensions/api/bluetooth_low_energy/bluetooth_low_energy_api.h"
#include "chrome/browser/extensions/api/bluetooth_socket/bluetooth_socket_event_dispatcher.h"
#include "chrome/browser/extensions/api/bookmark_manager_private/bookmark_manager_private_api.h"
#include "chrome/browser/extensions/api/bookmarks/bookmarks_api.h"
#include "chrome/browser/extensions/api/braille_display_private/braille_display_private_api.h"
#include "chrome/browser/extensions/api/commands/command_service.h"
#include "chrome/browser/extensions/api/content_settings/content_settings_service.h"
#include "chrome/browser/extensions/api/cookies/cookies_api.h"
#include "chrome/browser/extensions/api/developer_private/developer_private_api.h"
#include "chrome/browser/extensions/api/dial/dial_api_factory.h"
#include "chrome/browser/extensions/api/extension_action/extension_action_api.h"
#include "chrome/browser/extensions/api/feedback_private/feedback_private_api.h"
#include "chrome/browser/extensions/api/font_settings/font_settings_api.h"
#include "chrome/browser/extensions/api/gcd_private/gcd_private_api.h"
#include "chrome/browser/extensions/api/history/history_api.h"
#include "chrome/browser/extensions/api/hotword_private/hotword_private_api.h"
#include "chrome/browser/extensions/api/identity/identity_api.h"
#include "chrome/browser/extensions/api/idle/idle_manager_factory.h"
#include "chrome/browser/extensions/api/input/input.h"
#include "chrome/browser/extensions/api/location/location_manager.h"
#include "chrome/browser/extensions/api/management/management_api.h"
#include "chrome/browser/extensions/api/mdns/mdns_api.h"
#include "chrome/browser/extensions/api/media_galleries_private/media_galleries_private_api.h"
#include "chrome/browser/extensions/api/networking_private/networking_private_event_router_factory.h"
#include "chrome/browser/extensions/api/omnibox/omnibox_api.h"
#include "chrome/browser/extensions/api/preference/chrome_direct_setting_api.h"
#include "chrome/browser/extensions/api/preference/preference_api.h"
#include "chrome/browser/extensions/api/processes/processes_api.h"
#include "chrome/browser/extensions/api/push_messaging/push_messaging_api.h"
#include "chrome/browser/extensions/api/screenlock_private/screenlock_private_api.h"
#include "chrome/browser/extensions/api/sessions/sessions_api.h"
#include "chrome/browser/extensions/api/settings_overrides/settings_overrides_api.h"
#include "chrome/browser/extensions/api/signed_in_devices/signed_in_devices_manager.h"
#include "chrome/browser/extensions/api/streams_private/streams_private_api.h"
#include "chrome/browser/extensions/api/system_info/system_info_api.h"
#include "chrome/browser/extensions/api/tab_capture/tab_capture_registry.h"
#include "chrome/browser/extensions/api/tabs/tabs_windows_api.h"
#include "chrome/browser/extensions/api/web_navigation/web_navigation_api.h"
#include "chrome/browser/extensions/api/web_request/web_request_api.h"
#include "chrome/browser/extensions/api/webrtc_audio_private/webrtc_audio_private_api.h"
#include "chrome/browser/extensions/api/webstore/webstore_api.h"
#include "chrome/browser/extensions/extension_garbage_collector_factory.h"
#include "chrome/browser/extensions/extension_gcm_app_handler.h"
#include "chrome/browser/extensions/extension_storage_monitor_factory.h"
#include "chrome/browser/extensions/extension_system_factory.h"
#include "chrome/browser/extensions/extension_toolbar_model_factory.h"
#include "chrome/browser/extensions/extension_web_ui_override_registrar.h"
#include "chrome/browser/extensions/install_tracker_factory.h"
#include "chrome/browser/extensions/menu_manager_factory.h"
#include "chrome/browser/extensions/plugin_manager.h"
#include "chrome/browser/extensions/token_cache/token_cache_service_factory.h"
#include "chrome/browser/speech/extension_api/tts_extension_api.h"
#include "extensions/browser/api/api_resource_manager.h"
#include "extensions/browser/api/power/power_api_manager.h"
#include "extensions/browser/api/usb/usb_device_resource.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/extensions/file_manager/file_browser_private_api_factory.h"
#include "chrome/browser/chromeos/extensions/input_method_api.h"
#include "chrome/browser/chromeos/extensions/media_player_api.h"
#include "chrome/browser/extensions/api/input_ime/input_ime_api.h"
#include "chrome/browser/extensions/api/log_private/log_private_api.h"
#endif

#if defined(ENABLE_SPELLCHECK)
#include "chrome/browser/extensions/api/spellcheck/spellcheck_api.h"
#endif

namespace chrome_extensions {

void EnsureBrowserContextKeyedServiceFactoriesBuilt() {
  extensions::ActivityLog::GetFactoryInstance();
  extensions::ActivityLogAPI::GetFactoryInstance();
  extensions::AlarmManager::GetFactoryInstance();
  extensions::ApiResourceManager<
      extensions::UsbDeviceResource>::GetFactoryInstance();
  extensions::AudioAPI::GetFactoryInstance();
  extensions::BookmarksAPI::GetFactoryInstance();
  extensions::BookmarkManagerPrivateAPI::GetFactoryInstance();
  extensions::BluetoothAPI::GetFactoryInstance();
  extensions::BluetoothLowEnergyAPI::GetFactoryInstance();
  extensions::BluetoothPrivateAPI::GetFactoryInstance();
  extensions::BrailleDisplayPrivateAPI::GetFactoryInstance();
  extensions::chromedirectsetting::ChromeDirectSettingAPI::GetFactoryInstance();
  extensions::CommandService::GetFactoryInstance();
  extensions::ContentSettingsService::GetFactoryInstance();
  extensions::CookiesAPI::GetFactoryInstance();
  extensions::DeveloperPrivateAPI::GetFactoryInstance();
  extensions::DialAPIFactory::GetInstance();
  extensions::ExtensionActionAPI::GetFactoryInstance();
  extensions::ExtensionGarbageCollectorFactory::GetInstance();
  extensions::ExtensionStorageMonitorFactory::GetInstance();
  extensions::ExtensionSystemFactory::GetInstance();
  extensions::ExtensionToolbarModelFactory::GetInstance();
  extensions::ExtensionWebUIOverrideRegistrar::GetFactoryInstance();
  extensions::FeedbackPrivateAPI::GetFactoryInstance();
  extensions::FontSettingsAPI::GetFactoryInstance();
  extensions::GcdPrivateAPI::GetFactoryInstance();
  extensions::HistoryAPI::GetFactoryInstance();
  extensions::HotwordPrivateEventService::GetFactoryInstance();
  extensions::IdentityAPI::GetFactoryInstance();
  extensions::IdleManagerFactory::GetInstance();
  extensions::InstallTrackerFactory::GetInstance();
#if defined(TOOLKIT_VIEWS)
  extensions::InputAPI::GetFactoryInstance();
#endif
#if defined(OS_CHROMEOS)
  extensions::InputImeAPI::GetFactoryInstance();
  extensions::InputMethodAPI::GetFactoryInstance();
#endif
  extensions::LocationManager::GetFactoryInstance();
#if defined(OS_CHROMEOS)
  extensions::LogPrivateAPI::GetFactoryInstance();
#endif
  extensions::ManagementAPI::GetFactoryInstance();
  extensions::MDnsAPI::GetFactoryInstance();
  extensions::MediaGalleriesPrivateAPI::GetFactoryInstance();
#if defined(OS_CHROMEOS)
  extensions::MediaPlayerAPI::GetFactoryInstance();
#endif
  extensions::MenuManagerFactory::GetInstance();
#if defined(OS_CHROMEOS) || defined(OS_WIN) || defined(OS_MACOSX)
  extensions::NetworkingPrivateEventRouterFactory::GetInstance();
#endif
  extensions::OmniboxAPI::GetFactoryInstance();
#if defined(ENABLE_PLUGINS)
  extensions::PluginManager::GetFactoryInstance();
#endif  // defined(ENABLE_PLUGINS)
  extensions::PowerApiManager::GetFactoryInstance();
  extensions::PreferenceAPI::GetFactoryInstance();
  extensions::ProcessesAPI::GetFactoryInstance();
  extensions::PushMessagingAPI::GetFactoryInstance();
  extensions::ScreenlockPrivateEventRouter::GetFactoryInstance();
  extensions::SessionsAPI::GetFactoryInstance();
  extensions::SettingsOverridesAPI::GetFactoryInstance();
  extensions::SignedInDevicesManager::GetFactoryInstance();
#if defined(ENABLE_SPELLCHECK)
  extensions::SpellcheckAPI::GetFactoryInstance();
#endif
  extensions::StreamsPrivateAPI::GetFactoryInstance();
  extensions::SystemInfoAPI::GetFactoryInstance();
  extensions::TabCaptureRegistry::GetFactoryInstance();
  extensions::TabsWindowsAPI::GetFactoryInstance();
  extensions::TtsAPI::GetFactoryInstance();
  extensions::WebNavigationAPI::GetFactoryInstance();
  extensions::WebRequestAPI::GetFactoryInstance();
  extensions::WebrtcAudioPrivateEventService::GetFactoryInstance();
  extensions::WebstoreAPI::GetFactoryInstance();
#if defined(OS_CHROMEOS)
  file_manager::FileBrowserPrivateAPIFactory::GetInstance();
#endif
  TokenCacheServiceFactory::GetInstance();
  extensions::ExtensionGCMAppHandler::GetFactoryInstance();
  extensions::api::BluetoothSocketEventDispatcher::GetFactoryInstance();
}

}  // namespace chrome_extensions
