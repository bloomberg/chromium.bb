// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_content_browser_client.h"

#include <map>
#include <set>
#include <utility>
#include <vector>

#include "base/base_switches.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/files/scoped_file.h"
#include "base/json/json_reader.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "chrome/browser/after_startup_task_utils.h"
#include "chrome/browser/apps/app_url_redirector.h"
#include "chrome/browser/browser_about_handler.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_shutdown.h"
#include "chrome/browser/browsing_data/browsing_data_helper.h"
#include "chrome/browser/browsing_data/browsing_data_remover.h"
#include "chrome/browser/browsing_data/browsing_data_remover_factory.h"
#include "chrome/browser/browsing_data/origin_filter_builder.h"
#include "chrome/browser/browsing_data/registrable_domain_filter_builder.h"
#include "chrome/browser/budget_service/budget_service_impl.h"
#include "chrome/browser/character_encoding.h"
#include "chrome/browser/chrome_content_browser_client_parts.h"
#include "chrome/browser/chrome_net_benchmarking_message_filter.h"
#include "chrome/browser/chrome_quota_permission_context.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/engagement/site_engagement_eviction_policy.h"
#include "chrome/browser/font_family_cache.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/memory/chrome_memory_coordinator_delegate.h"
#include "chrome/browser/metrics/chrome_browser_main_extra_parts_metrics.h"
#include "chrome/browser/nacl_host/nacl_browser_delegate_impl.h"
#include "chrome/browser/notifications/platform_notification_service_impl.h"
#include "chrome/browser/page_load_metrics/metrics_navigation_throttle.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/payments/payment_request_impl.h"
#include "chrome/browser/permissions/permission_context_base.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/prerender/prerender_final_status.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/prerender/prerender_manager_factory.h"
#include "chrome/browser/prerender/prerender_message_filter.h"
#include "chrome/browser/printing/printing_message_filter.h"
#include "chrome/browser/profiles/chrome_browser_main_extra_parts_profiles.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_io_data.h"
#include "chrome/browser/renderer_host/chrome_navigation_ui_data.h"
#include "chrome/browser/renderer_host/chrome_render_message_filter.h"
#include "chrome/browser/renderer_host/pepper/chrome_browser_pepper_host_factory.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/safe_browsing/ui_manager.h"
#include "chrome/browser/search/instant_service.h"
#include "chrome/browser/search/instant_service_factory.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/speech/chrome_speech_recognition_manager_delegate.h"
#include "chrome/browser/speech/tts_controller.h"
#include "chrome/browser/speech/tts_message_filter.h"
#include "chrome/browser/ssl/ssl_blocking_page.h"
#include "chrome/browser/ssl/ssl_cert_reporter.h"
#include "chrome/browser/ssl/ssl_client_certificate_selector.h"
#include "chrome/browser/ssl/ssl_error_handler.h"
#include "chrome/browser/sync_file_system/local/sync_file_system_backend.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/tracing/chrome_tracing_delegate.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/ui/blocked_content/blocked_window_params.h"
#include "chrome/browser/ui/blocked_content/popup_blocker_tab_helper.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/browser/ui/sync/sync_promo_ui.h"
#include "chrome/browser/ui/tab_contents/chrome_web_contents_view_delegate.h"
#include "chrome/browser/ui/webui/chrome_web_ui_controller_factory.h"
#include "chrome/browser/ui/webui/log_web_ui_url.h"
#include "chrome/browser/usb/usb_tab_helper.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/env_vars.h"
#include "chrome/common/features.h"
#include "chrome/common/logging_chrome.h"
#include "chrome/common/origin_trials/chrome_origin_trial_policy.h"
#include "chrome/common/pepper_permission_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/secure_origin_whitelist.h"
#include "chrome/common/stack_sampling_configuration.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/installer/util/google_update_settings.h"
#include "chromeos/chromeos_constants.h"
#include "components/autofill/content/browser/content_autofill_driver_factory.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/cdm/browser/cdm_message_filter_android.h"
#include "components/cloud_devices/common/cloud_devices_switches.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/dom_distiller/core/dom_distiller_switches.h"
#include "components/dom_distiller/core/url_constants.h"
#include "components/error_page/common/error_page_switches.h"
#include "components/google/core/browser/google_util.h"
#include "components/metrics/call_stack_profile_collector.h"
#include "components/metrics/client_info.h"
#include "components/net_log/chrome_net_log.h"
#include "components/password_manager/content/browser/content_password_manager_driver_factory.h"
#include "components/payments/payment_request.mojom.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/rappor/rappor_recorder_impl.h"
#include "components/rappor/rappor_utils.h"
#include "components/security_interstitials/core/ssl_error_ui.h"
#include "components/signin/core/common/profile_management_switches.h"
#include "components/spellcheck/spellcheck_build_features.h"
#include "components/startup_metric_utils/browser/startup_metric_host_impl.h"
#include "components/translate/core/common/translate_switches.h"
#include "components/url_formatter/url_fixer.h"
#include "components/variations/variations_associated_data.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_child_process_host.h"
#include "content/public/browser/browser_main_parts.h"
#include "content/public/browser/browser_ppapi_host.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/browser_url_handler.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/client_certificate_delegate.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/resource_context.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/vpn_service_proxy.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/child_process_host.h"
#include "content/public/common/content_descriptors.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/sandbox_type.h"
#include "content/public/common/service_manager_connection.h"
#include "content/public/common/service_names.mojom.h"
#include "content/public/common/url_utils.h"
#include "content/public/common/web_preferences.h"
#include "device/bluetooth/adapter_factory.h"
#include "device/bluetooth/public/interfaces/adapter.mojom.h"
#include "device/usb/public/interfaces/chooser_service.mojom.h"
#include "device/usb/public/interfaces/device_manager.mojom.h"
#include "extensions/features/features.h"
#include "gin/v8_initializer.h"
#include "media/media_features.h"
#include "net/base/mime_util.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_options.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "ppapi/features/features.h"
#include "ppapi/host/ppapi_host.h"
#include "printing/features/features.h"
#include "services/image_decoder/public/interfaces/constants.mojom.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "services/service_manager/public/cpp/interface_registry.h"
#include "services/service_manager/public/cpp/service.h"
#include "storage/browser/fileapi/external_mount_points.h"
#include "third_party/WebKit/public/platform/modules/webshare/webshare.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/resources/grit/ui_resources.h"
#include "url/gurl.h"
#include "url/origin.h"

#if defined(OS_WIN)
#include "base/strings/string_tokenizer.h"
#include "chrome/browser/chrome_browser_main_win.h"
#include "sandbox/win/src/sandbox_policy.h"
#elif defined(OS_MACOSX)
#include "chrome/browser/chrome_browser_main_mac.h"
#elif defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/arc/arc_session_manager.h"
#include "chrome/browser/chromeos/arc/fileapi/arc_content_file_system_backend_delegate.h"
#include "chrome/browser/chromeos/arc/intent_helper/arc_navigation_throttle.h"
#include "chrome/browser/chromeos/attestation/platform_verification_impl.h"
#include "chrome/browser/chromeos/chrome_browser_main_chromeos.h"
#include "chrome/browser/chromeos/chrome_interface_factory.h"
#include "chrome/browser/chromeos/drive/fileapi/file_system_backend_delegate.h"
#include "chrome/browser/chromeos/file_manager/app_id.h"
#include "chrome/browser/chromeos/file_system_provider/fileapi/backend_delegate.h"
#include "chrome/browser/chromeos/fileapi/file_system_backend.h"
#include "chrome/browser/chromeos/fileapi/mtp_file_system_backend_delegate.h"
#include "chrome/browser/chromeos/login/signin/merge_session_navigation_throttle.h"
#include "chrome/browser/chromeos/login/signin/merge_session_throttling_utils.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/system/input_device_settings.h"
#include "chrome/browser/ui/ash/ash_util.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chromeos/chromeos_switches.h"
#include "components/user_manager/user_manager.h"
#elif defined(OS_LINUX)
#include "chrome/browser/chrome_browser_main_linux.h"
#elif defined(OS_ANDROID)
#include "chrome/browser/chrome_browser_main_android.h"
#include "chrome/common/descriptors_android.h"
#include "components/crash/content/browser/crash_dump_manager_android.h"
#include "components/navigation_interception/intercept_navigation_delegate.h"
#include "ui/base/resource/resource_bundle_android.h"
#elif defined(OS_POSIX)
#include "chrome/browser/chrome_browser_main_posix.h"
#endif

#if defined(OS_POSIX) && !defined(OS_MACOSX)
#include "base/debug/leak_annotations.h"
#include "components/crash/content/app/breakpad_linux.h"
#include "components/crash/content/browser/crash_handler_host_linux.h"
#endif

#if BUILDFLAG(ANDROID_JAVA_UI)
#include "chrome/browser/android/ntp/new_tab_page_url_handler.h"
#include "chrome/browser/android/service_tab_launcher.h"
#include "chrome/browser/android/webapps/single_tab_mode_tab_helper.h"
#include "content/public/browser/android/java_interfaces.h"
#endif

#if defined(OS_ANDROID)
#include "chrome/browser/android/devtools_manager_delegate_android.h"
#include "ui/base/ui_base_paths.h"
#include "ui/gfx/android/device_display_info.h"
#else
#include "chrome/browser/devtools/chrome_devtools_manager_delegate.h"
#endif

#if defined(TOOLKIT_VIEWS)
#include "chrome/browser/ui/views/chrome_browser_main_extra_parts_views.h"
#endif

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
#include "chrome/browser/ui/views/chrome_browser_main_extra_parts_views_linux.h"
#endif

#if defined(USE_AURA)
#include "services/service_manager/runner/common/client_util.h"
#include "services/ui/public/cpp/gpu/gpu_service.h"
#include "ui/views/mus/window_manager_connection.h"
#endif

#if defined(USE_ASH)
#include "chrome/browser/ui/views/ash/chrome_browser_main_extra_parts_ash.h"
#endif

#if defined(USE_X11)
#include "chrome/browser/chrome_browser_main_extra_parts_x11.h"
#endif

#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
#include "chrome/browser/captive_portal/captive_portal_tab_helper.h"
#endif

#if !defined(DISABLE_NACL)
#include "components/nacl/browser/nacl_browser.h"
#include "components/nacl/browser/nacl_host_message_filter.h"
#include "components/nacl/browser/nacl_process_host.h"
#include "components/nacl/common/nacl_process_type.h"
#include "components/nacl/common/nacl_switches.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/accessibility/animation_policy_prefs.h"
#include "chrome/browser/extensions/chrome_content_browser_client_extensions_part.h"
#include "chrome/browser/media/cast_transport_host_filter.h"
#include "chrome/browser/speech/extension_api/tts_engine_extension_api.h"
#include "components/guest_view/browser/guest_view_base.h"
#include "components/guest_view/browser/guest_view_manager.h"
#include "extensions/browser/extension_navigation_throttle.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#include "extensions/browser/guest_view/web_view/web_view_permission_helper.h"
#include "extensions/browser/guest_view/web_view/web_view_renderer_state.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "extensions/common/manifest_handlers/shared_module_info.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/permissions/socket_permission.h"
#include "extensions/common/switches.h"
#endif

#if BUILDFLAG(ENABLE_PLUGINS)
#include "chrome/browser/plugins/chrome_content_browser_client_plugins_part.h"
#include "chrome/browser/plugins/flash_download_interception.h"
#endif

#if BUILDFLAG(ENABLE_SPELLCHECK)
#include "chrome/browser/spellchecker/spellcheck_message_filter.h"
#endif

#if BUILDFLAG(USE_BROWSER_SPELLCHECKER)
#include "components/spellcheck/browser/spellcheck_message_filter_platform.h"
#endif

#if BUILDFLAG(ENABLE_WEBRTC)
#include "chrome/browser/media/audio_debug_recordings_handler.h"
#include "chrome/browser/media/webrtc/webrtc_logging_handler_host.h"
#endif

#if defined(ENABLE_MEDIA_ROUTER)
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/media/router/presentation_service_delegate_impl.h"
#endif  // defined(ENABLE_MEDIA_ROUTER)

#if BUILDFLAG(ENABLE_MEDIA_REMOTING) && defined(ENABLE_MEDIA_ROUTER)
#include "chrome/browser/media/cast_remoting_connector.h"
#endif


#if defined(ENABLE_WAYLAND_SERVER)
#include "chrome/browser/chrome_browser_main_extra_parts_exo.h"
#endif

#if defined(ENABLE_MOJO_MEDIA)
#include "chrome/browser/media/output_protection_impl.h"
#endif

#if defined(ENABLE_MOJO_MEDIA_IN_BROWSER_PROCESS)
#include "media/mojo/services/media_service_factory.h"  // nogncheck
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/metrics/leak_detector/leak_detector_remote_controller.h"
#endif

using base::FileDescriptor;
using blink::WebWindowFeatures;
using content::BrowserThread;
using content::BrowserURLHandler;
using content::ChildProcessSecurityPolicy;
using content::QuotaPermissionContext;
using content::RenderFrameHost;
using content::RenderViewHost;
using content::ResourceType;
using content::SiteInstance;
using content::WebContents;
using content::WebPreferences;
using message_center::NotifierId;
using security_interstitials::SSLErrorUI;

#if defined(OS_POSIX)
using content::FileDescriptorInfo;
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
using extensions::APIPermission;
using extensions::ChromeContentBrowserClientExtensionsPart;
using extensions::Extension;
using extensions::InfoMap;
using extensions::Manifest;
#endif

#if BUILDFLAG(ENABLE_PLUGINS)
using plugins::ChromeContentBrowserClientPluginsPart;
#endif

namespace {

// Cached version of the locale so we can return the locale on the I/O
// thread.
base::LazyInstance<std::string> g_io_thread_application_locale;

#if BUILDFLAG(ENABLE_PLUGINS)
// TODO(teravest): Add renderer-side API-specific checking for these APIs so
// that blanket permission isn't granted to all dev channel APIs for these.
// http://crbug.com/386743
const char* const kPredefinedAllowedDevChannelOrigins[] = {
  "6EAED1924DB611B6EEF2A664BD077BE7EAD33B8F",  // see crbug.com/383937
  "4EB74897CB187C7633357C2FE832E0AD6A44883A"   // see crbug.com/383937
};

const char* const kPredefinedAllowedFileHandleOrigins[] = {
  "6EAED1924DB611B6EEF2A664BD077BE7EAD33B8F",  // see crbug.com/234789
  "4EB74897CB187C7633357C2FE832E0AD6A44883A"   // see crbug.com/234789
};

const char* const kPredefinedAllowedSocketOrigins[] = {
  "okddffdblfhhnmhodogpojmfkjmhinfp",  // Test SSH Client
  "pnhechapfaindjhompbnflcldabbghjo",  // HTerm App (SSH Client)
  "bglhmjfplikpjnfoegeomebmfnkjomhe",  // see crbug.com/122126
  "gbchcmhmhahfdphkhkmpfmihenigjmpp",  // Chrome Remote Desktop
  "kgngmbheleoaphbjbaiobfdepmghbfah",  // Pre-release Chrome Remote Desktop
  "odkaodonbgfohohmklejpjiejmcipmib",  // Dogfood Chrome Remote Desktop
  "ojoimpklfciegopdfgeenehpalipignm",  // Chromoting canary
  "cbkkbcmdlboombapidmoeolnmdacpkch",  // see crbug.com/129089
  "hhnbmknkdabfoieppbbljkhkfjcmcbjh",  // see crbug.com/134099
  "mablfbjkhmhkmefkjjacnbaikjkipphg",  // see crbug.com/134099
  "pdeelgamlgannhelgoegilelnnojegoh",  // see crbug.com/134099
  "cabapfdbkniadpollkckdnedaanlciaj",  // see crbug.com/134099
  "mapljbgnjledlpdmlchihnmeclmefbba",  // see crbug.com/134099
  "ghbfeebgmiidnnmeobbbaiamklmpbpii",  // see crbug.com/134099
  "jdfhpkjeckflbbleddjlpimecpbjdeep",  // see crbug.com/142514
  "iabmpiboiopbgfabjmgeedhcmjenhbla",  // see crbug.com/165080
  "B7CF8A292249681AF81771650BA4CEEAF19A4560",  // see crbug.com/165080
  "7525AF4F66763A70A883C4700529F647B470E4D2",  // see crbug.com/238084
  "0B549507088E1564D672F7942EB87CA4DAD73972",  // see crbug.com/238084
  "864288364E239573E777D3E0E36864E590E95C74"   // see crbug.com/238084
};
#endif

enum AppLoadedInTabSource {
  APP_LOADED_IN_TAB_SOURCE_APP = 0,
  APP_LOADED_IN_TAB_SOURCE_BACKGROUND_PAGE,
  APP_LOADED_IN_TAB_SOURCE_MAX
};

// Returns a copy of the given url with its host set to given host and path set
// to given path. Other parts of the url will be the same.
GURL ReplaceURLHostAndPath(const GURL& url,
                           const std::string& host,
                           const std::string& path) {
  url::Replacements<char> replacements;
  replacements.SetHost(host.c_str(), url::Component(0, host.length()));
  replacements.SetPath(path.c_str(), url::Component(0, path.length()));
  return url.ReplaceComponents(replacements);
}

// Maps "foo://bar/baz/" to "foo://chrome/bar/baz/".
GURL AddUberHost(const GURL& url) {
  const std::string uber_host = chrome::kChromeUIUberHost;
  std::string new_path;
  url.host_piece().AppendToString(&new_path);
  url.path_piece().AppendToString(&new_path);

  return ReplaceURLHostAndPath(url, uber_host, new_path);
}

// If url->host() is "chrome" and url->path() has characters other than the
// first slash, changes the url from "foo://chrome/bar/" to "foo://bar/" and
// returns true. Otherwise returns false.
bool RemoveUberHost(GURL* url) {
  if (url->host() != chrome::kChromeUIUberHost)
    return false;

  if (url->path().empty() || url->path() == "/")
    return false;

  const std::string old_path = url->path();

  const std::string::size_type separator = old_path.find('/', 1);
  std::string new_host;
  std::string new_path;
  if (separator == std::string::npos) {
    new_host = old_path.substr(1);
  } else {
    new_host = old_path.substr(1, separator - 1);
    new_path = old_path.substr(separator);
  }

  // Do not allow URLs with paths empty before the first slash since we can't
  // have an empty host. (e.g "foo://chrome//")
  if (new_host.empty())
    return false;

  *url = ReplaceURLHostAndPath(*url, new_host, new_path);

  DCHECK(url->is_valid());

  return true;
}

// Handles rewriting Web UI URLs.
bool HandleWebUI(GURL* url, content::BrowserContext* browser_context) {
  // Do not handle special URLs such as "about:foo"
  if (!url->host().empty()) {
    const GURL chrome_url = AddUberHost(*url);

    // Handle valid "chrome://chrome/foo" URLs so the reverse handler will
    // be called.
    if (ChromeWebUIControllerFactory::GetInstance()->UseWebUIForURL(
            browser_context, chrome_url))
      return true;
  }

  if (!ChromeWebUIControllerFactory::GetInstance()->UseWebUIForURL(
          browser_context, *url))
    return false;

#if defined(OS_CHROMEOS)
  // Special case : in ChromeOS in Guest mode bookmarks and history are
  // disabled for security reasons. New tab page explains the reasons, so
  // we redirect user to new tab page.
  if (user_manager::UserManager::Get()->IsLoggedInAsGuest()) {
    if (url->SchemeIs(content::kChromeUIScheme) &&
        (url->DomainIs(chrome::kChromeUIBookmarksHost) ||
         url->DomainIs(chrome::kChromeUIHistoryHost))) {
      // Rewrite with new tab URL
      *url = GURL(chrome::kChromeUINewTabURL);
    }
  }
#endif

  return true;
}

// Reverse URL handler for Web UI. Maps "chrome://chrome/foo/" to
// "chrome://foo/".
bool HandleWebUIReverse(GURL* url, content::BrowserContext* browser_context) {
  if (!url->is_valid() || !url->SchemeIs(content::kChromeUIScheme))
    return false;

  return RemoveUberHost(url);
}

bool CertMatchesFilter(const net::X509Certificate& cert,
                       const base::DictionaryValue& filter) {
  // TODO(markusheintz): This is the minimal required filter implementation.
  // Implement a better matcher.

  // An empty filter matches any client certificate since no requirements are
  // specified at all.
  if (filter.empty())
    return true;

  std::string common_name;
  if (filter.GetString("ISSUER.CN", &common_name) &&
      (cert.issuer().common_name == common_name)) {
    return true;
  }
  return false;
}

#if defined(OS_POSIX) && !defined(OS_ANDROID) && !defined(OS_MACOSX)
breakpad::CrashHandlerHostLinux* CreateCrashHandlerHost(
    const std::string& process_type) {
  base::FilePath dumps_path;
  PathService::Get(chrome::DIR_CRASH_DUMPS, &dumps_path);
  {
    ANNOTATE_SCOPED_MEMORY_LEAK;
    bool upload = (getenv(env_vars::kHeadless) == NULL);
    breakpad::CrashHandlerHostLinux* crash_handler =
        new breakpad::CrashHandlerHostLinux(process_type, dumps_path, upload);
    crash_handler->StartUploaderThread();
    return crash_handler;
  }
}

int GetCrashSignalFD(const base::CommandLine& command_line) {
  // Extensions have the same process type as renderers.
  if (command_line.HasSwitch(extensions::switches::kExtensionProcess)) {
    static breakpad::CrashHandlerHostLinux* crash_handler = NULL;
    if (!crash_handler)
      crash_handler = CreateCrashHandlerHost("extension");
    return crash_handler->GetDeathSignalSocket();
  }

  std::string process_type =
      command_line.GetSwitchValueASCII(switches::kProcessType);

  if (process_type == switches::kRendererProcess) {
    static breakpad::CrashHandlerHostLinux* crash_handler = NULL;
    if (!crash_handler)
      crash_handler = CreateCrashHandlerHost(process_type);
    return crash_handler->GetDeathSignalSocket();
  }

  if (process_type == switches::kPpapiPluginProcess) {
    static breakpad::CrashHandlerHostLinux* crash_handler = NULL;
    if (!crash_handler)
      crash_handler = CreateCrashHandlerHost(process_type);
    return crash_handler->GetDeathSignalSocket();
  }

  if (process_type == switches::kGpuProcess) {
    static breakpad::CrashHandlerHostLinux* crash_handler = NULL;
    if (!crash_handler)
      crash_handler = CreateCrashHandlerHost(process_type);
    return crash_handler->GetDeathSignalSocket();
  }

  return -1;
}
#endif  // defined(OS_POSIX) && !defined(OS_ANDROID) && !defined(OS_MACOSX)

void SetApplicationLocaleOnIOThread(const std::string& locale) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  g_io_thread_application_locale.Get() = locale;
}

void HandleBlockedPopupOnUIThread(const BlockedWindowParams& params) {
  RenderFrameHost* render_frame_host = RenderFrameHost::FromID(
      params.render_process_id(), params.opener_render_frame_id());
  if (!render_frame_host)
    return;
  WebContents* tab = WebContents::FromRenderFrameHost(render_frame_host);
  // The tab might already have navigated away.  We only need to do this check
  // for main frames, since the RenderFrameHost for a subframe opener will have
  // already been deleted if the main frame navigates away.
  if (!tab ||
      (!render_frame_host->GetParent() &&
       tab->GetMainFrame() != render_frame_host))
    return;

  prerender::PrerenderContents* prerender_contents =
      prerender::PrerenderContents::FromWebContents(tab);
  if (prerender_contents) {
    prerender_contents->Destroy(prerender::FINAL_STATUS_CREATE_NEW_WINDOW);
    return;
  }

  PopupBlockerTabHelper* popup_helper =
      PopupBlockerTabHelper::FromWebContents(tab);
  if (!popup_helper)
    return;
  popup_helper->AddBlockedPopup(params);
}

#if BUILDFLAG(ENABLE_PLUGINS)
void HandleFlashDownloadActionOnUIThread(int render_process_id,
                                         int render_frame_id,
                                         const GURL& source_url) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RenderFrameHost* render_frame_host =
      RenderFrameHost::FromID(render_process_id, render_frame_id);
  if (!render_frame_host)
    return;
  WebContents* web_contents =
      WebContents::FromRenderFrameHost(render_frame_host);
  FlashDownloadInterception::InterceptFlashDownloadNavigation(web_contents,
                                                              source_url);
}
#endif  // BUILDFLAG(ENABLE_PLUGINS)

// An implementation of the SSLCertReporter interface used by
// SSLErrorHandler. Uses the SafeBrowsing UI manager to send invalid
// certificate reports.
class SafeBrowsingSSLCertReporter : public SSLCertReporter {
 public:
  explicit SafeBrowsingSSLCertReporter(
      const scoped_refptr<safe_browsing::SafeBrowsingUIManager>&
          safe_browsing_ui_manager)
      : safe_browsing_ui_manager_(safe_browsing_ui_manager) {}
  ~SafeBrowsingSSLCertReporter() override {}

  // SSLCertReporter implementation
  void ReportInvalidCertificateChain(
      const std::string& serialized_report) override {
    if (safe_browsing_ui_manager_) {
      safe_browsing_ui_manager_->ReportInvalidCertificateChain(
          serialized_report, base::Bind(&base::DoNothing));
    }
  }

 private:
  const scoped_refptr<safe_browsing::SafeBrowsingUIManager>
      safe_browsing_ui_manager_;
};

#if BUILDFLAG(ANDROID_JAVA_UI)
void HandleSingleTabModeBlockOnUIThread(const BlockedWindowParams& params) {
  WebContents* web_contents = tab_util::GetWebContentsByFrameID(
      params.render_process_id(), params.opener_render_frame_id());
  if (!web_contents)
    return;

  SingleTabModeTabHelper::FromWebContents(web_contents)->HandleOpenUrl(params);
}
#endif  // BUILDFLAG(ANDROID_JAVA_UI)

#if defined(OS_ANDROID)
float GetDeviceScaleAdjustment() {
  static const float kMinFSM = 1.05f;
  static const int kWidthForMinFSM = 320;
  static const float kMaxFSM = 1.3f;
  static const int kWidthForMaxFSM = 800;

  gfx::DeviceDisplayInfo info;
  int minWidth = info.GetSmallestDIPWidth();

  if (minWidth <= kWidthForMinFSM)
    return kMinFSM;
  if (minWidth >= kWidthForMaxFSM)
    return kMaxFSM;

  // The font scale multiplier varies linearly between kMinFSM and kMaxFSM.
  float ratio = static_cast<float>(minWidth - kWidthForMinFSM) /
      (kWidthForMaxFSM - kWidthForMinFSM);
  return ratio * (kMaxFSM - kMinFSM) + kMinFSM;
}
#endif  // defined(OS_ANDROID)

#if BUILDFLAG(ENABLE_EXTENSIONS)
// By default, JavaScript, images and autoplay are enabled in guest content.
void GetGuestViewDefaultContentSettingRules(
    bool incognito,
    RendererContentSettingRules* rules) {
  rules->image_rules.push_back(
      ContentSettingPatternSource(ContentSettingsPattern::Wildcard(),
                                  ContentSettingsPattern::Wildcard(),
                                  CONTENT_SETTING_ALLOW,
                                  std::string(),
                                  incognito));

  rules->script_rules.push_back(
      ContentSettingPatternSource(ContentSettingsPattern::Wildcard(),
                                  ContentSettingsPattern::Wildcard(),
                                  CONTENT_SETTING_ALLOW,
                                  std::string(),
                                  incognito));
  rules->autoplay_rules.push_back(
      ContentSettingPatternSource(ContentSettingsPattern::Wildcard(),
                                  ContentSettingsPattern::Wildcard(),
                                  CONTENT_SETTING_ALLOW,
                                  std::string(),
                                  incognito));
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

void CreateUsbDeviceManager(
    RenderFrameHost* render_frame_host,
    mojo::InterfaceRequest<device::usb::DeviceManager> request) {
  WebContents* web_contents =
      WebContents::FromRenderFrameHost(render_frame_host);
  if (!web_contents) {
    NOTREACHED();
    return;
  }

  UsbTabHelper* tab_helper =
      UsbTabHelper::GetOrCreateForWebContents(web_contents);
  tab_helper->CreateDeviceManager(render_frame_host, std::move(request));
}

void CreateWebUsbChooserService(
    RenderFrameHost* render_frame_host,
    mojo::InterfaceRequest<device::usb::ChooserService> request) {
  WebContents* web_contents =
      WebContents::FromRenderFrameHost(render_frame_host);
  if (!web_contents) {
    NOTREACHED();
    return;
  }

  UsbTabHelper* tab_helper =
      UsbTabHelper::GetOrCreateForWebContents(web_contents);
  tab_helper->CreateChooserService(render_frame_host, std::move(request));
}

bool GetDataSaverEnabledPref(const PrefService* prefs) {
  // Enable data saver only when data saver pref is enabled and not part of
  // "Disabled" group of "SaveDataHeader" experiment.
  return prefs->GetBoolean(prefs::kDataSaverEnabled) &&
         base::FieldTrialList::FindFullName("SaveDataHeader")
             .compare("Disabled");
}

// A BrowsingDataRemover::Observer that waits for |count|
// OnBrowsingDataRemoverDone() callbacks, translates them into
// one base::Closure, and then destroys itself.
class ClearSiteDataObserver : public BrowsingDataRemover::Observer {
 public:
  explicit ClearSiteDataObserver(BrowsingDataRemover* remover,
                                 const base::Closure& callback,
                                 int count)
      : remover_(remover), callback_(callback), count_(count) {
    remover_->AddObserver(this);
  }

  ~ClearSiteDataObserver() override { remover_->RemoveObserver(this); }

  // BrowsingDataRemover::Observer.
  void OnBrowsingDataRemoverDone() override {
    DCHECK(count_);
    if (--count_)
      return;

    callback_.Run();
    delete this;
  }

 private:
  BrowsingDataRemover* remover_;
  base::Closure callback_;
  int count_;
};

WebContents* GetWebContents(int render_process_id, int render_frame_id) {
  RenderFrameHost* rfh =
      RenderFrameHost::FromID(render_process_id, render_frame_id);
  return WebContents::FromRenderFrameHost(rfh);
}

}  // namespace

ChromeContentBrowserClient::ChromeContentBrowserClient()
    : weak_factory_(this) {
#if BUILDFLAG(ENABLE_PLUGINS)
  for (size_t i = 0; i < arraysize(kPredefinedAllowedDevChannelOrigins); ++i)
    allowed_dev_channel_origins_.insert(kPredefinedAllowedDevChannelOrigins[i]);
  for (size_t i = 0; i < arraysize(kPredefinedAllowedFileHandleOrigins); ++i)
    allowed_file_handle_origins_.insert(kPredefinedAllowedFileHandleOrigins[i]);
  for (size_t i = 0; i < arraysize(kPredefinedAllowedSocketOrigins); ++i)
    allowed_socket_origins_.insert(kPredefinedAllowedSocketOrigins[i]);

  extra_parts_.push_back(new ChromeContentBrowserClientPluginsPart);
#endif

#if !defined(OS_ANDROID)
  TtsExtensionEngine* tts_extension_engine = TtsExtensionEngine::GetInstance();
  TtsController::GetInstance()->SetTtsEngineDelegate(tts_extension_engine);
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
  extra_parts_.push_back(new ChromeContentBrowserClientExtensionsPart);
#endif
}

ChromeContentBrowserClient::~ChromeContentBrowserClient() {
  for (int i = static_cast<int>(extra_parts_.size()) - 1; i >= 0; --i)
    delete extra_parts_[i];
  extra_parts_.clear();
}

// static
void ChromeContentBrowserClient::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(prefs::kDisable3DAPIs, false);
  registry->RegisterBooleanPref(prefs::kEnableHyperlinkAuditing, true);
  registry->RegisterListPref(prefs::kEnableDeprecatedWebPlatformFeatures);
}

// static
void ChromeContentBrowserClient::SetApplicationLocale(
    const std::string& locale) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // This object is guaranteed to outlive all threads so we don't have to
  // worry about the lack of refcounting and can just post as Unretained.
  //
  // The common case is that this function is called early in Chrome startup
  // before any threads are created (it will also be called later if the user
  // changes the pref). In this case, there will be no threads created and
  // posting will fail. When there are no threads, we can just set the string
  // without worrying about threadsafety.
  if (!BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
          base::Bind(&SetApplicationLocaleOnIOThread, locale))) {
    g_io_thread_application_locale.Get() = locale;
  }
}

content::BrowserMainParts* ChromeContentBrowserClient::CreateBrowserMainParts(
    const content::MainFunctionParams& parameters) {
  ChromeBrowserMainParts* main_parts;
  // Construct the Main browser parts based on the OS type.
#if defined(OS_WIN)
  main_parts = new ChromeBrowserMainPartsWin(parameters);
#elif defined(OS_MACOSX)
  main_parts = new ChromeBrowserMainPartsMac(parameters);
#elif defined(OS_CHROMEOS)
  main_parts = new chromeos::ChromeBrowserMainPartsChromeos(parameters);
#elif defined(OS_LINUX)
  main_parts = new ChromeBrowserMainPartsLinux(parameters);
#elif defined(OS_ANDROID)
  main_parts = new ChromeBrowserMainPartsAndroid(parameters);
#elif defined(OS_POSIX)
  main_parts = new ChromeBrowserMainPartsPosix(parameters);
#else
  NOTREACHED();
  main_parts = new ChromeBrowserMainParts(parameters);
#endif

  chrome::AddProfilesExtraParts(main_parts);

  // Construct additional browser parts. Stages are called in the order in
  // which they are added.
#if defined(TOOLKIT_VIEWS)
#if defined(OS_LINUX) && !defined(OS_CHROMEOS) && !defined(USE_OZONE)
  main_parts->AddParts(new ChromeBrowserMainExtraPartsViewsLinux());
#else
  main_parts->AddParts(new ChromeBrowserMainExtraPartsViews());
#endif
#endif

// TODO(oshima): Athena on chrome currently requires USE_ASH to build.
// We should reduce the dependency as much as possible.
#if defined(USE_ASH)
  main_parts->AddParts(new ChromeBrowserMainExtraPartsAsh());
#endif

#if defined(USE_X11)
  main_parts->AddParts(new ChromeBrowserMainExtraPartsX11());
#endif

#if defined(ENABLE_WAYLAND_SERVER)
  main_parts->AddParts(new ChromeBrowserMainExtraPartsExo());
#endif

  chrome::AddMetricsExtraParts(main_parts);

  return main_parts;
}

void ChromeContentBrowserClient::PostAfterStartupTask(
    const tracked_objects::Location& from_here,
    const scoped_refptr<base::TaskRunner>& task_runner,
    const base::Closure& task) {
  AfterStartupTaskUtils::PostTask(from_here, task_runner, task);
}

bool ChromeContentBrowserClient::IsBrowserStartupComplete() {
  return AfterStartupTaskUtils::IsBrowserStartupComplete();
}

std::string ChromeContentBrowserClient::GetStoragePartitionIdForSite(
    content::BrowserContext* browser_context,
    const GURL& site) {
  std::string partition_id;

  // The partition ID for webview guest processes is the string value of its
  // SiteInstance URL - "chrome-guest://app_id/persist?partition".
  if (site.SchemeIs(content::kGuestScheme))
    partition_id = site.spec();
#if BUILDFLAG(ENABLE_EXTENSIONS)
  // The partition ID for extensions with isolated storage is treated similarly
  // to the above.
  else if (site.SchemeIs(extensions::kExtensionScheme) &&
           extensions::util::SiteHasIsolatedStorage(site, browser_context))
    partition_id = site.spec();
#endif

  DCHECK(IsValidStoragePartitionId(browser_context, partition_id));
  return partition_id;
}

bool ChromeContentBrowserClient::IsValidStoragePartitionId(
    content::BrowserContext* browser_context,
    const std::string& partition_id) {
  // The default ID is empty and is always valid.
  if (partition_id.empty())
    return true;

  return GURL(partition_id).is_valid();
}

void ChromeContentBrowserClient::GetStoragePartitionConfigForSite(
    content::BrowserContext* browser_context,
    const GURL& site,
    bool can_be_default,
    std::string* partition_domain,
    std::string* partition_name,
    bool* in_memory) {
  // Default to the browser-wide storage partition and override based on |site|
  // below.
  partition_domain->clear();
  partition_name->clear();
  *in_memory = false;

#if BUILDFLAG(ENABLE_EXTENSIONS)
  bool success = extensions::WebViewGuest::GetGuestPartitionConfigForSite(
      site, partition_domain, partition_name, in_memory);

  if (!success && site.SchemeIs(extensions::kExtensionScheme)) {
    // If |can_be_default| is false, the caller is stating that the |site|
    // should be parsed as if it had isolated storage. In particular it is
    // important to NOT check ExtensionService for the is_storage_isolated()
    // attribute because this code path is run during Extension uninstall
    // to do cleanup after the Extension has already been unloaded from the
    // ExtensionService.
    bool is_isolated = !can_be_default;
    if (can_be_default) {
      if (extensions::util::SiteHasIsolatedStorage(site, browser_context))
        is_isolated = true;
    }

    if (is_isolated) {
      CHECK(site.has_host());
      // For extensions with isolated storage, the the host of the |site| is
      // the |partition_domain|. The |in_memory| and |partition_name| are only
      // used in guest schemes so they are cleared here.
      *partition_domain = site.host();
      *in_memory = false;
      partition_name->clear();
    }
    success = true;
  }
#endif

  // Assert that if |can_be_default| is false, the code above must have found a
  // non-default partition.  If this fails, the caller has a serious logic
  // error about which StoragePartition they expect to be in and it is not
  // safe to continue.
  CHECK(can_be_default || !partition_domain->empty());
}

content::WebContentsViewDelegate*
    ChromeContentBrowserClient::GetWebContentsViewDelegate(
        content::WebContents* web_contents) {
  return chrome::CreateWebContentsViewDelegate(web_contents);
}

void ChromeContentBrowserClient::RenderProcessWillLaunch(
    content::RenderProcessHost* host) {
  int id = host->GetID();
  Profile* profile = Profile::FromBrowserContext(host->GetBrowserContext());
  net::URLRequestContextGetter* context =
      host->GetStoragePartition()->GetURLRequestContext();

  host->AddFilter(new ChromeRenderMessageFilter(
      id, profile, host->GetStoragePartition()->GetServiceWorkerContext()));
#if BUILDFLAG(ENABLE_EXTENSIONS)
  host->AddFilter(new cast::CastTransportHostFilter);
#endif
#if BUILDFLAG(ENABLE_PRINTING)
  host->AddFilter(new printing::PrintingMessageFilter(id, profile));
#endif
#if BUILDFLAG(ENABLE_SPELLCHECK)
  host->AddFilter(new SpellCheckMessageFilter(id));
#endif
#if BUILDFLAG(USE_BROWSER_SPELLCHECKER)
  host->AddFilter(new SpellCheckMessageFilterPlatform(id));
#endif
  host->AddFilter(new ChromeNetBenchmarkingMessageFilter(profile, context));
  host->AddFilter(new prerender::PrerenderMessageFilter(id, profile));
  host->AddFilter(new TtsMessageFilter(host->GetBrowserContext()));
#if BUILDFLAG(ENABLE_WEBRTC)
  WebRtcLoggingHandlerHost* webrtc_logging_handler_host =
      new WebRtcLoggingHandlerHost(id, profile,
                                   g_browser_process->webrtc_log_uploader());
  host->AddFilter(webrtc_logging_handler_host);
  host->SetUserData(WebRtcLoggingHandlerHost::kWebRtcLoggingHandlerHostKey,
                    new base::UserDataAdapter<WebRtcLoggingHandlerHost>(
                        webrtc_logging_handler_host));

  AudioDebugRecordingsHandler* audio_debug_recordings_handler =
      new AudioDebugRecordingsHandler(profile);
  host->SetUserData(
      AudioDebugRecordingsHandler::kAudioDebugRecordingsHandlerKey,
      new base::UserDataAdapter<AudioDebugRecordingsHandler>(
          audio_debug_recordings_handler));

#endif
#if !defined(DISABLE_NACL)
  host->AddFilter(new nacl::NaClHostMessageFilter(
      id, profile->IsOffTheRecord(),
      profile->GetPath(),
      context));
#endif
#if defined(OS_ANDROID)
  host->AddFilter(new cdm::CdmMessageFilterAndroid());
#endif

  host->Send(new ChromeViewMsg_SetIsIncognitoProcess(
      profile->IsOffTheRecord()));

  for (size_t i = 0; i < extra_parts_.size(); ++i)
    extra_parts_[i]->RenderProcessWillLaunch(host);

  RendererContentSettingRules rules;
  if (host->IsForGuestsOnly()) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
    GetGuestViewDefaultContentSettingRules(profile->IsOffTheRecord(), &rules);
#else
    NOTREACHED();
#endif
  } else {
    GetRendererContentSettingRules(
        HostContentSettingsMapFactory::GetForProfile(profile), &rules);
  }
  host->Send(new ChromeViewMsg_SetContentSettingRules(rules));
}

GURL ChromeContentBrowserClient::GetEffectiveURL(
    content::BrowserContext* browser_context, const GURL& url) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  if (!profile)
    return url;

  // If the input |url| should be assigned to the Instant renderer, make its
  // effective URL distinct from other URLs on the search provider's domain.
  if (search::ShouldAssignURLToInstantRenderer(url, profile))
    return search::GetEffectiveURLForInstant(url, profile);

#if BUILDFLAG(ENABLE_EXTENSIONS)
  return ChromeContentBrowserClientExtensionsPart::GetEffectiveURL(
      profile, url);
#else
  return url;
#endif
}

bool ChromeContentBrowserClient::ShouldUseProcessPerSite(
    content::BrowserContext* browser_context, const GURL& effective_url) {
  // Non-extension, non-Instant URLs should generally use
  // process-per-site-instance.  Because we expect to use the effective URL,
  // URLs for hosted apps (apart from bookmark apps) should have an extension
  // scheme by now.

  Profile* profile = Profile::FromBrowserContext(browser_context);
  if (!profile)
    return false;

  if (search::ShouldUseProcessPerSiteForInstantURL(effective_url, profile))
    return true;

#if BUILDFLAG(ENABLE_EXTENSIONS)
  return ChromeContentBrowserClientExtensionsPart::ShouldUseProcessPerSite(
      profile, effective_url);
#else
  return false;
#endif
}

bool ChromeContentBrowserClient::DoesSiteRequireDedicatedProcess(
    content::BrowserContext* browser_context,
    const GURL& effective_site_url) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (ChromeContentBrowserClientExtensionsPart::DoesSiteRequireDedicatedProcess(
          browser_context, effective_site_url)) {
    return true;
  }
#endif
  return false;
}

// TODO(creis, nick): https://crbug.com/160576 describes a weakness in our
// origin-lock enforcement, where we don't have a way to efficiently know
// effective URLs on the IO thread, and wind up killing processes that e.g.
// request cookies for their actual URL. This whole function (and its
// ExtensionsPart) should be removed once we add that ability to the IO thread.
bool ChromeContentBrowserClient::ShouldLockToOrigin(
    content::BrowserContext* browser_context,
    const GURL& effective_site_url) {
  // Origin lock to the search scheme would kill processes upon legitimate
  // requests for cookies from the search engine's domain.
  if (effective_site_url.SchemeIs(chrome::kChromeSearchScheme))
    return false;

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Disable origin lock if this is an extension/app that applies effective URL
  // mappings.
  if (!ChromeContentBrowserClientExtensionsPart::ShouldLockToOrigin(
          browser_context, effective_site_url)) {
    return false;
  }
#endif
  return true;
}

// These are treated as WebUI schemes but do not get WebUI bindings. Also,
// view-source is allowed for these schemes.
void ChromeContentBrowserClient::GetAdditionalWebUISchemes(
    std::vector<std::string>* additional_schemes) {
  additional_schemes->push_back(chrome::kChromeSearchScheme);
  additional_schemes->push_back(dom_distiller::kDomDistillerScheme);
}

bool ChromeContentBrowserClient::LogWebUIUrl(const GURL& web_ui_url) const {
  return webui::LogWebUIUrl(web_ui_url);
}

bool ChromeContentBrowserClient::IsHandledURL(const GURL& url) {
  return ProfileIOData::IsHandledURL(url);
}

bool ChromeContentBrowserClient::CanCommitURL(
    content::RenderProcessHost* process_host,
    const GURL& url) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  return ChromeContentBrowserClientExtensionsPart::CanCommitURL(
      process_host, url);
#else
  return true;
#endif
}

bool ChromeContentBrowserClient::ShouldAllowOpenURL(
    content::SiteInstance* site_instance, const GURL& url) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  bool result;
  if (ChromeContentBrowserClientExtensionsPart::ShouldAllowOpenURL(
          site_instance, url, &result))
    return result;
#endif

  // Do not allow chrome://chrome-signin navigate to other chrome:// URLs, since
  // the signin page may host untrusted web content.
  GURL from_url = site_instance->GetSiteURL();
  if (from_url.GetOrigin().spec() == chrome::kChromeUIChromeSigninURL &&
      url.SchemeIs(content::kChromeUIScheme) &&
      url.host_piece() != chrome::kChromeUIChromeSigninHost) {
    VLOG(1) << "Blocked navigation to " << url.spec() << " from "
            << chrome::kChromeUIChromeSigninURL;
    return false;
  }

  return true;
}

namespace {

// Returns whether a SiteInstance holds a NTP. TODO(mastiz): This
// really really really needs to be moved to a shared place where all the code
// that needs to know this can access it. See http://crbug.com/624410.
bool IsNTPSiteInstance(SiteInstance* site_instance) {
  // While using SiteInstance::GetSiteURL() is unreliable and the wrong thing to
  // use for making security decisions 99.44% of the time, for detecting the NTP
  // it is reliable and the correct way. Again, see http://crbug.com/624410.
  return site_instance &&
         site_instance->GetSiteURL().SchemeIs(chrome::kChromeSearchScheme) &&
         (site_instance->GetSiteURL().host_piece() ==
              chrome::kChromeSearchRemoteNtpHost ||
          site_instance->GetSiteURL().host_piece() ==
              chrome::kChromeSearchLocalNtpHost);
}

}  // namespace

void ChromeContentBrowserClient::OverrideNavigationParams(
    SiteInstance* site_instance,
    ui::PageTransition* transition,
    bool* is_renderer_initiated,
    content::Referrer* referrer) {
  DCHECK(transition);
  DCHECK(is_renderer_initiated);
  DCHECK(referrer);
  // TODO(crbug.com/624410): Factor the predicate to identify a URL as an NTP
  // to a shared library.
  if (IsNTPSiteInstance(site_instance) &&
      ui::PageTransitionCoreTypeIs(*transition, ui::PAGE_TRANSITION_LINK)) {
    // Use AUTO_BOOKMARK for clicks on tiles of the new tab page, consistently
    // with native implementations like Android's.
    *transition = ui::PAGE_TRANSITION_AUTO_BOOKMARK;
    *is_renderer_initiated = false;
    *referrer = content::Referrer();
  }

#if BUILDFLAG(ENABLE_EXTENSIONS)
  ChromeContentBrowserClientExtensionsPart::OverrideNavigationParams(
      site_instance, transition, is_renderer_initiated, referrer);
#endif
}

bool ChromeContentBrowserClient::
    ShouldFrameShareParentSiteInstanceDespiteTopDocumentIsolation(
        const GURL& url,
        content::SiteInstance* parent_site_instance) {
  return IsNTPSiteInstance(parent_site_instance);
}

bool ChromeContentBrowserClient::IsSuitableHost(
    content::RenderProcessHost* process_host,
    const GURL& site_url) {
  Profile* profile =
      Profile::FromBrowserContext(process_host->GetBrowserContext());
  // This may be NULL during tests. In that case, just assume any site can
  // share any host.
  if (!profile)
    return true;

  // Instant URLs should only be in the instant process and instant process
  // should only have Instant URLs.
  InstantService* instant_service =
      InstantServiceFactory::GetForProfile(profile);
  if (instant_service) {
    bool is_instant_process = instant_service->IsInstantProcess(
        process_host->GetID());
    bool should_be_in_instant_process =
        search::ShouldAssignURLToInstantRenderer(site_url, profile);
    if (is_instant_process || should_be_in_instant_process)
      return is_instant_process && should_be_in_instant_process;
  }

#if BUILDFLAG(ENABLE_EXTENSIONS)
  return ChromeContentBrowserClientExtensionsPart::IsSuitableHost(
      profile, process_host, site_url);
#else
  return true;
#endif
}

bool ChromeContentBrowserClient::MayReuseHost(
    content::RenderProcessHost* process_host) {
  // If there is currently a prerender in progress for the host provided,
  // it may not be shared. We require prerenders to be by themselves in a
  // separate process so that we can monitor their resource usage.
  prerender::PrerenderManager* prerender_manager =
      prerender::PrerenderManagerFactory::GetForBrowserContext(
          process_host->GetBrowserContext());
  if (prerender_manager &&
      !prerender_manager->MayReuseProcessHost(process_host)) {
    return false;
  }

  return true;
}

bool ChromeContentBrowserClient::ShouldTryToUseExistingProcessHost(
    content::BrowserContext* browser_context, const GURL& url) {
  // It has to be a valid URL for us to check for an extension.
  if (!url.is_valid())
    return false;

#if BUILDFLAG(ENABLE_EXTENSIONS)
  Profile* profile = Profile::FromBrowserContext(browser_context);
  return ChromeContentBrowserClientExtensionsPart::
      ShouldTryToUseExistingProcessHost(
          profile, url);
#else
  return false;
#endif
}

void ChromeContentBrowserClient::SiteInstanceGotProcess(
    SiteInstance* site_instance) {
  CHECK(site_instance->HasProcess());

  Profile* profile = Profile::FromBrowserContext(
      site_instance->GetBrowserContext());
  if (!profile)
    return;

  // Remember the ID of the Instant process to signal the renderer process
  // on startup in |AppendExtraCommandLineSwitches| below.
  if (search::ShouldAssignURLToInstantRenderer(site_instance->GetSiteURL(),
                                               profile)) {
    InstantService* instant_service =
        InstantServiceFactory::GetForProfile(profile);
    if (instant_service)
      instant_service->AddInstantProcess(site_instance->GetProcess()->GetID());
  }

  for (size_t i = 0; i < extra_parts_.size(); ++i)
    extra_parts_[i]->SiteInstanceGotProcess(site_instance);
}

void ChromeContentBrowserClient::SiteInstanceDeleting(
    SiteInstance* site_instance) {
  if (!site_instance->HasProcess())
    return;

  for (size_t i = 0; i < extra_parts_.size(); ++i)
    extra_parts_[i]->SiteInstanceDeleting(site_instance);
}

bool ChromeContentBrowserClient::ShouldSwapBrowsingInstancesForNavigation(
    SiteInstance* site_instance,
    const GURL& current_url,
    const GURL& new_url) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  return ChromeContentBrowserClientExtensionsPart::
      ShouldSwapBrowsingInstancesForNavigation(
          site_instance, current_url, new_url);
#else
  return false;
#endif
}

bool ChromeContentBrowserClient::ShouldSwapProcessesForRedirect(
    content::BrowserContext* browser_context,
    const GURL& current_url,
    const GURL& new_url) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  return ChromeContentBrowserClientExtensionsPart::
      ShouldSwapProcessesForRedirect(browser_context, current_url, new_url);
#else
  return false;
#endif
}

bool ChromeContentBrowserClient::ShouldAssignSiteForURL(const GURL& url) {
  return !url.SchemeIs(chrome::kChromeNativeScheme);
}

std::string ChromeContentBrowserClient::GetCanonicalEncodingNameByAliasName(
    const std::string& alias_name) {
  return ::GetCanonicalEncodingNameByAliasName(alias_name);
}

namespace {

bool IsAutoReloadEnabled() {
  // Fetch the field trial, even though we don't use it. Calling FindFullName()
  // causes the field-trial mechanism to report which group we're in, which
  // might reflect a hard disable or hard enable via flag, both of which have
  // their own field trial groups. This lets us know what percentage of users
  // manually enable or disable auto-reload.
  std::string group = base::FieldTrialList::FindFullName(
      "AutoReloadExperiment");
  const base::CommandLine& browser_command_line =
      *base::CommandLine::ForCurrentProcess();
  if (browser_command_line.HasSwitch(switches::kEnableOfflineAutoReload))
    return true;
  if (browser_command_line.HasSwitch(switches::kDisableOfflineAutoReload))
    return false;
  return true;
}

bool IsAutoReloadVisibleOnlyEnabled() {
  // See the block comment in IsAutoReloadEnabled().
  std::string group = base::FieldTrialList::FindFullName(
      "AutoReloadVisibleOnlyExperiment");
  const base::CommandLine& browser_command_line =
      *base::CommandLine::ForCurrentProcess();
  if (browser_command_line.HasSwitch(
      switches::kEnableOfflineAutoReloadVisibleOnly)) {
    return true;
  }
  if (browser_command_line.HasSwitch(
      switches::kDisableOfflineAutoReloadVisibleOnly)) {
    return false;
  }
  return true;
}

#if defined(OS_LINUX) || defined(OS_CHROMEOS) || defined(OS_WIN)
bool AreExperimentalWebPlatformFeaturesEnabled() {
  const base::CommandLine& browser_command_line =
      *base::CommandLine::ForCurrentProcess();
  return browser_command_line.HasSwitch(
      switches::kEnableExperimentalWebPlatformFeatures);
}
#endif

void MaybeAppendBlinkSettingsSwitchForFieldTrial(
    const base::CommandLine& browser_command_line,
    base::CommandLine* command_line) {
  // List of field trials that modify the blink-settings command line flag. No
  // two field trials in the list should specify the same keys, otherwise one
  // field trial may overwrite another. See Source/core/frame/Settings.in in
  // Blink for the list of valid keys.
  static const char* const kBlinkSettingsFieldTrials[] = {
      // Keys: backgroundHtmlParserOutstandingTokenLimit
      //       backgroundHtmlParserPendingTokenLimit
      "BackgroundHtmlParserTokenLimits",

      // Keys: doHtmlPreloadScanning
      "HtmlPreloadScanning",

      // Keys: lowPriorityIframes
      "LowPriorityIFrames",

      // Keys: disallowFetchForDocWrittenScriptsInMainFrame
      //       disallowFetchForDocWrittenScriptsInMainFrameOnSlowConnections
      //       disallowFetchForDocWrittenScriptsInMainFrameIfEffectively2G
      "DisallowFetchForDocWrittenScriptsInMainFrame",

      // Keys: parseHTMLOnMainThreadSyncTokenize
      //       parseHTMLOnMainThreadCoalesceChunks
      "ParseHTMLOnMainThread",

      // Keys: cssExternalScannerNoPreload
      //       cssExternalScannerPreload
      "CSSExternalScanner",
  };

  std::vector<std::string> blink_settings;
  for (const char* field_trial_name : kBlinkSettingsFieldTrials) {
    // Each blink-settings field trial should include a forcing_flag group,
    // to make sure that clients that specify the blink-settings flag on the
    // command line are excluded from the experiment groups. To make
    // sure we assign clients that specify this flag to the forcing_flag
    // group, we must call GetVariationParams for each field trial first
    // (for example, before checking HasSwitch() and returning), since
    // GetVariationParams has the side-effect of assigning the client to
    // a field trial group.
    std::map<std::string, std::string> params;
    if (variations::GetVariationParams(field_trial_name, &params)) {
      for (const auto& param : params) {
        blink_settings.push_back(base::StringPrintf(
            "%s=%s", param.first.c_str(), param.second.c_str()));
      }
    }
  }

  if (blink_settings.empty()) {
    return;
  }

  if (browser_command_line.HasSwitch(switches::kBlinkSettings) ||
      command_line->HasSwitch(switches::kBlinkSettings)) {
    // The field trials should be configured to force users that specify the
    // blink-settings flag into a group with no params, and we return
    // above if no params were specified, so it's an error if we reach
    // this point.
    LOG(WARNING) << "Received field trial params, "
                    "but blink-settings switch already specified.";
    return;
  }

  command_line->AppendSwitchASCII(switches::kBlinkSettings,
                                  base::JoinString(blink_settings, ","));
}

#if BUILDFLAG(ANDROID_JAVA_UI)
void ForwardShareServiceRequest(
    base::WeakPtr<service_manager::InterfaceProvider> interface_provider,
    blink::mojom::ShareServiceRequest request) {
  if (!interface_provider ||
      ChromeOriginTrialPolicy().IsFeatureDisabled("WebShare")) {
    return;
  }
  interface_provider->GetInterface(std::move(request));
}
#endif

}  // namespace

void ChromeContentBrowserClient::AppendExtraCommandLineSwitches(
    base::CommandLine* command_line,
    int child_process_id) {
#if defined(OS_MACOSX)
  std::unique_ptr<metrics::ClientInfo> client_info =
      GoogleUpdateSettings::LoadMetricsClientInfo();
  if (client_info) {
    command_line->AppendSwitchASCII(switches::kMetricsClientID,
                                    client_info->client_id);
  }
#elif defined(OS_POSIX)
  if (breakpad::IsCrashReporterEnabled()) {
    std::string switch_value;
    std::unique_ptr<metrics::ClientInfo> client_info =
        GoogleUpdateSettings::LoadMetricsClientInfo();
    if (client_info)
      switch_value = client_info->client_id;
    switch_value.push_back(',');
    switch_value.append(chrome::GetChannelString());
    command_line->AppendSwitchASCII(switches::kEnableCrashReporter,
                                    switch_value);
  }
#endif

  if (logging::DialogsAreSuppressed())
    command_line->AppendSwitch(switches::kNoErrorDialogs);

  std::string process_type =
      command_line->GetSwitchValueASCII(switches::kProcessType);
  const base::CommandLine& browser_command_line =
      *base::CommandLine::ForCurrentProcess();

  static const char* const kCommonSwitchNames[] = {
    switches::kUserAgent,
    switches::kUserDataDir,  // Make logs go to the right file.
  };
  command_line->CopySwitchesFrom(browser_command_line, kCommonSwitchNames,
                                 arraysize(kCommonSwitchNames));

  static const char* const kDinosaurEasterEggSwitches[] = {
      error_page::switches::kDisableDinosaurEasterEgg,
  };
  command_line->CopySwitchesFrom(browser_command_line,
                                 kDinosaurEasterEggSwitches,
                                 arraysize(kDinosaurEasterEggSwitches));

#if defined(OS_CHROMEOS)
  // On Chrome OS need to pass primary user homedir (in multi-profiles session).
  base::FilePath homedir;
  PathService::Get(base::DIR_HOME, &homedir);
  command_line->AppendSwitchASCII(chromeos::switches::kHomedir,
                                  homedir.value().c_str());
#endif

  if (process_type == switches::kRendererProcess) {
    content::RenderProcessHost* process =
        content::RenderProcessHost::FromID(child_process_id);
    Profile* profile =
        process ? Profile::FromBrowserContext(process->GetBrowserContext())
                : NULL;
    for (size_t i = 0; i < extra_parts_.size(); ++i) {
      extra_parts_[i]->AppendExtraRendererCommandLineSwitches(
          command_line, process, profile);
    }

#if defined(OS_CHROMEOS)
    const std::string& login_profile =
        browser_command_line.GetSwitchValueASCII(
            chromeos::switches::kLoginProfile);
    if (!login_profile.empty())
      command_line->AppendSwitchASCII(
          chromeos::switches::kLoginProfile, login_profile);
#endif

#if BUILDFLAG(ENABLE_WEBRTC)
    MaybeCopyDisableWebRtcEncryptionSwitch(command_line,
                                           browser_command_line,
                                           chrome::GetChannel());
#endif

    if (process) {
      PrefService* prefs = profile->GetPrefs();
      // Currently this pref is only registered if applied via a policy.
      if (prefs->HasPrefPath(prefs::kDisable3DAPIs) &&
          prefs->GetBoolean(prefs::kDisable3DAPIs)) {
        // Turn this policy into a command line switch.
        command_line->AppendSwitch(switches::kDisable3DAPIs);
      }

      const base::ListValue* switches =
          prefs->GetList(prefs::kEnableDeprecatedWebPlatformFeatures);
      if (switches) {
        // Enable any deprecated features that have been re-enabled by policy.
        for (base::ListValue::const_iterator it = switches->begin();
             it != switches->end(); ++it) {
          std::string switch_to_enable;
          if ((*it)->GetAsString(&switch_to_enable))
            command_line->AppendSwitch(switch_to_enable);
        }
      }

      // Disable client-side phishing detection in the renderer if it is
      // disabled in the Profile preferences or the browser process.
      if (!prefs->GetBoolean(prefs::kSafeBrowsingEnabled) ||
          !g_browser_process->safe_browsing_detection_service()) {
        command_line->AppendSwitch(
            switches::kDisableClientSidePhishingDetection);
      }

      if (prefs->GetBoolean(prefs::kPrintPreviewDisabled))
        command_line->AppendSwitch(switches::kDisablePrintPreview);

      InstantService* instant_service =
          InstantServiceFactory::GetForProfile(profile);
      if (instant_service &&
          instant_service->IsInstantProcess(process->GetID()))
        command_line->AppendSwitch(switches::kInstantProcess);

      if (prefs->HasPrefPath(prefs::kAllowDinosaurEasterEgg) &&
          !prefs->GetBoolean(prefs::kAllowDinosaurEasterEgg))
        command_line->AppendSwitch(
            error_page::switches::kDisableDinosaurEasterEgg);
    }

    if (IsAutoReloadEnabled())
      command_line->AppendSwitch(switches::kEnableOfflineAutoReload);
    if (IsAutoReloadVisibleOnlyEnabled()) {
      command_line->AppendSwitch(
          switches::kEnableOfflineAutoReloadVisibleOnly);
    }

    {
      // Enable showing a saved copy if this session is in the field trial
      // or the user explicitly enabled it.  Note that as far as the
      // renderer is concerned, the feature is enabled if-and-only-if
      // one of the kEnableShowSavedCopy* switches is on the command
      // line; the yes/no/default behavior is only at the browser
      // command line level.

      // Command line switches override
      const std::string& show_saved_copy_value =
          browser_command_line.GetSwitchValueASCII(
              error_page::switches::kShowSavedCopy);
      if (show_saved_copy_value ==
              error_page::switches::kEnableShowSavedCopyPrimary ||
          show_saved_copy_value ==
              error_page::switches::kEnableShowSavedCopySecondary ||
          show_saved_copy_value ==
              error_page::switches::kDisableShowSavedCopy) {
        command_line->AppendSwitchASCII(error_page::switches::kShowSavedCopy,
                                        show_saved_copy_value);
      } else {
        std::string group =
            base::FieldTrialList::FindFullName("LoadStaleCacheExperiment");

        if (group == "Primary") {
          command_line->AppendSwitchASCII(
              error_page::switches::kShowSavedCopy,
              error_page::switches::kEnableShowSavedCopyPrimary);
        } else if (group == "Secondary") {
          command_line->AppendSwitchASCII(
              error_page::switches::kShowSavedCopy,
              error_page::switches::kEnableShowSavedCopySecondary);
        }
      }
    }

    MaybeAppendBlinkSettingsSwitchForFieldTrial(
        browser_command_line, command_line);

#if defined(OS_ANDROID)
    // If the platform is Android, force the distillability service on.
    command_line->AppendSwitch(switches::kEnableDistillabilityService);
#endif

    // Please keep this in alphabetical order.
    static const char* const kSwitchNames[] = {
#if defined(OS_ANDROID)
      autofill::switches::kDisableAccessorySuggestionView,
      autofill::switches::kEnableAccessorySuggestionView,
#endif
      autofill::switches::kDisablePasswordGeneration,
      autofill::switches::kEnablePasswordGeneration,
      autofill::switches::kEnableSingleClickAutofill,
      autofill::switches::kEnableSuggestionsWithSubstringMatch,
      autofill::switches::kIgnoreAutocompleteOffForAutofill,
      autofill::switches::kLocalHeuristicsOnlyForPasswordGeneration,
#if BUILDFLAG(ENABLE_EXTENSIONS)
      extensions::switches::kAllowHTTPBackgroundPage,
      extensions::switches::kAllowLegacyExtensionManifests,
      extensions::switches::kEnableAppWindowControls,
      extensions::switches::kEnableEmbeddedExtensionOptions,
      extensions::switches::kEnableExperimentalExtensionApis,
      extensions::switches::kExtensionsOnChromeURLs,
      extensions::switches::kIsolateExtensions,
      extensions::switches::kNativeCrxBindings,
      extensions::switches::kWhitelistedExtensionID,
#endif
      switches::kAllowInsecureLocalhost,
      switches::kAppsGalleryURL,
      switches::kCloudPrintURL,
      switches::kCloudPrintXmppEndpoint,
      switches::kDisableBundledPpapiFlash,
      switches::kDisableCastStreamingHWEncoding,
      switches::kDisableJavaScriptHarmonyShipping,
      switches::kDisableNewBookmarkApps,
      switches::kEnableBenchmarking,
      switches::kEnableDistillabilityService,
      switches::kEnableNaCl,
#if !defined(DISABLE_NACL)
      switches::kEnableNaClDebug,
      switches::kEnableNaClNonSfiMode,
#endif
      switches::kEnableNetBenchmarking,
      switches::kEnableNewBookmarkApps,
#if !defined(DISABLE_NACL)
      switches::kForcePNaClSubzero,
#endif
      switches::kJavaScriptHarmony,
      switches::kOriginTrialDisabledFeatures,
      switches::kOriginTrialPublicKey,
      switches::kPpapiFlashArgs,
      switches::kPpapiFlashPath,
      switches::kPpapiFlashVersion,
      switches::kProfilingAtStart,
      switches::kProfilingFile,
      switches::kProfilingFlush,
      switches::kReaderModeHeuristics,
      switches::kUnsafelyTreatInsecureOriginAsSecure,
      translate::switches::kTranslateSecurityOrigin,
    };

    command_line->CopySwitchesFrom(browser_command_line, kSwitchNames,
                                   arraysize(kSwitchNames));
  } else if (process_type == switches::kUtilityProcess) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
    static const char* const kSwitchNames[] = {
      extensions::switches::kAllowHTTPBackgroundPage,
      extensions::switches::kEnableExperimentalExtensionApis,
      extensions::switches::kExtensionsOnChromeURLs,
      extensions::switches::kWhitelistedExtensionID,
    };

    command_line->CopySwitchesFrom(browser_command_line, kSwitchNames,
                                   arraysize(kSwitchNames));
#endif
  } else if (process_type == switches::kZygoteProcess) {
    static const char* const kSwitchNames[] = {
      // Load (in-process) Pepper plugins in-process in the zygote pre-sandbox.
      switches::kDisableBundledPpapiFlash,
#if !defined(DISABLE_NACL)
      switches::kEnableNaClDebug,
      switches::kEnableNaClNonSfiMode,
      switches::kForcePNaClSubzero,
      switches::kNaClDangerousNoSandboxNonSfi,
#endif
      switches::kPpapiFlashPath,
      switches::kPpapiFlashVersion,
    };

    command_line->CopySwitchesFrom(browser_command_line, kSwitchNames,
                                   arraysize(kSwitchNames));
  } else if (process_type == switches::kGpuProcess) {
    // If --ignore-gpu-blacklist is passed in, don't send in crash reports
    // because GPU is expected to be unreliable.
    if (browser_command_line.HasSwitch(switches::kIgnoreGpuBlacklist) &&
        !command_line->HasSwitch(switches::kDisableBreakpad))
      command_line->AppendSwitch(switches::kDisableBreakpad);
  }

  // The command line switch kEnableBenchmarking needs to be specified along
  // with the kEnableStatsTable switch to ensure that the stats table global
  // is initialized correctly.
  if (command_line->HasSwitch(switches::kEnableBenchmarking))
    DCHECK(command_line->HasSwitch(switches::kEnableStatsTable));

  StackSamplingConfiguration::Get()->AppendCommandLineSwitchForChildProcess(
      process_type,
      command_line);
}

std::string ChromeContentBrowserClient::GetApplicationLocale() {
  if (BrowserThread::CurrentlyOn(BrowserThread::IO))
    return g_io_thread_application_locale.Get();
  return g_browser_process->GetApplicationLocale();
}

std::string ChromeContentBrowserClient::GetAcceptLangs(
    content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
  return profile->GetPrefs()->GetString(prefs::kAcceptLanguages);
}

const gfx::ImageSkia* ChromeContentBrowserClient::GetDefaultFavicon() {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  return rb.GetNativeImageNamed(IDR_DEFAULT_FAVICON).ToImageSkia();
}

bool ChromeContentBrowserClient::IsDataSaverEnabled(
    content::BrowserContext* browser_context) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  if (!profile)
    return false;
  PrefService* prefs = profile->GetPrefs();
  return prefs && prefs->GetBoolean(prefs::kDataSaverEnabled);
}

bool ChromeContentBrowserClient::AllowAppCache(
    const GURL& manifest_url,
    const GURL& first_party,
    content::ResourceContext* context) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  ProfileIOData* io_data = ProfileIOData::FromResourceContext(context);
  return io_data->GetCookieSettings()->
      IsSettingCookieAllowed(manifest_url, first_party);
}

bool ChromeContentBrowserClient::AllowServiceWorker(
    const GURL& scope,
    const GURL& first_party_url,
    content::ResourceContext* context,
    int render_process_id,
    int render_frame_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Check if this is an extension-related service worker, and, if so, if it's
  // allowed (this can return false if, e.g., the extension is disabled).
  // If it's not allowed, return immediately. We deliberately do *not* report
  // to the TabSpecificContentSettings, since the service worker is blocked
  // because of the extension, rather than because of the user's content
  // settings.
  if (!ChromeContentBrowserClientExtensionsPart::AllowServiceWorker(
          scope, first_party_url, context, render_process_id,
          render_frame_id)) {
    return false;
  }
#endif

  ProfileIOData* io_data = ProfileIOData::FromResourceContext(context);

  // Check if JavaScript is allowed.
  content_settings::SettingInfo info;
  std::unique_ptr<base::Value> value =
      io_data->GetHostContentSettingsMap()->GetWebsiteSetting(
          first_party_url, first_party_url, CONTENT_SETTINGS_TYPE_JAVASCRIPT,
          std::string(), &info);
  ContentSetting setting = content_settings::ValueToContentSetting(value.get());
  bool allow_javascript = (setting == CONTENT_SETTING_ALLOW);

  // Check if cookies are allowed.
  bool allow_serviceworker =
      io_data->GetCookieSettings()->IsSettingCookieAllowed(scope,
                                                           first_party_url);
  // Record access to database for potential display in UI.
  // Only post the task if this is for a specific frame.
  if (render_process_id != -1 && render_frame_id != -1) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&TabSpecificContentSettings::ServiceWorkerAccessed,
                   render_process_id, render_frame_id, scope,
                   !allow_javascript, !allow_serviceworker));
  }
  return allow_javascript && allow_serviceworker;
}

bool ChromeContentBrowserClient::AllowGetCookie(
    const GURL& url,
    const GURL& first_party,
    const net::CookieList& cookie_list,
    content::ResourceContext* context,
    int render_process_id,
    int render_frame_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  ProfileIOData* io_data = ProfileIOData::FromResourceContext(context);
  bool allow = io_data->GetCookieSettings()->
      IsReadingCookieAllowed(url, first_party);

  base::Callback<content::WebContents*(void)> wc_getter =
      base::Bind(&GetWebContents, render_process_id, render_frame_id);
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&TabSpecificContentSettings::CookiesRead, wc_getter, url,
                 first_party, cookie_list, !allow));
  return allow;
}

bool ChromeContentBrowserClient::AllowSetCookie(
    const GURL& url,
    const GURL& first_party,
    const std::string& cookie_line,
    content::ResourceContext* context,
    int render_process_id,
    int render_frame_id,
    const net::CookieOptions& options) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  ProfileIOData* io_data = ProfileIOData::FromResourceContext(context);
  content_settings::CookieSettings* cookie_settings =
      io_data->GetCookieSettings();
  bool allow = cookie_settings->IsSettingCookieAllowed(url, first_party);

  base::Callback<content::WebContents*(void)> wc_getter =
      base::Bind(&GetWebContents, render_process_id, render_frame_id);
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&TabSpecificContentSettings::CookieChanged, wc_getter, url,
                 first_party, cookie_line, options, !allow));
  return allow;
}

bool ChromeContentBrowserClient::AllowSaveLocalState(
    content::ResourceContext* context) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  ProfileIOData* io_data = ProfileIOData::FromResourceContext(context);
  content_settings::CookieSettings* cookie_settings =
      io_data->GetCookieSettings();
  ContentSetting setting = cookie_settings->GetDefaultCookieSetting(NULL);

  // TODO(bauerb): Should we also disallow local state if the default is BLOCK?
  // Could we even support per-origin settings?
  return setting != CONTENT_SETTING_SESSION_ONLY;
}

void ChromeContentBrowserClient::AllowWorkerFileSystem(
    const GURL& url,
    content::ResourceContext* context,
    const std::vector<std::pair<int, int> >& render_frames,
    base::Callback<void(bool)> callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  ProfileIOData* io_data = ProfileIOData::FromResourceContext(context);
  content_settings::CookieSettings* cookie_settings =
      io_data->GetCookieSettings();
  bool allow = cookie_settings->IsSettingCookieAllowed(url, url);

#if BUILDFLAG(ENABLE_EXTENSIONS)
  GuestPermissionRequestHelper(url, render_frames, callback, allow);
#else
  FileSystemAccessed(url, render_frames, callback, allow);
#endif
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
void ChromeContentBrowserClient::GuestPermissionRequestHelper(
    const GURL& url,
    const std::vector<std::pair<int, int> >& render_frames,
    base::Callback<void(bool)> callback,
    bool allow) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  std::vector<std::pair<int, int> >::const_iterator i;
  std::map<int, int> process_map;
  std::map<int, int>::const_iterator it;
  bool has_web_view_guest = false;
  // Record access to file system for potential display in UI.
  for (i = render_frames.begin(); i != render_frames.end(); ++i) {
    if (process_map.find(i->first) != process_map.end())
      continue;

    process_map.insert(std::pair<int, int>(i->first, i->second));

    if (extensions::WebViewRendererState::GetInstance()->IsGuest(i->first))
      has_web_view_guest = true;
  }
  if (!has_web_view_guest) {
    FileSystemAccessed(url, render_frames, callback, allow);
    return;
  }
  DCHECK_EQ(1U, process_map.size());
  it = process_map.begin();
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&ChromeContentBrowserClient::
                  RequestFileSystemPermissionOnUIThread,
                  it->first,
                  it->second,
                  url,
                  allow,
                  base::Bind(&ChromeContentBrowserClient::FileSystemAccessed,
                            weak_factory_.GetWeakPtr(),
                            url,
                            render_frames,
                            callback)));
}

void ChromeContentBrowserClient::RequestFileSystemPermissionOnUIThread(
    int render_process_id,
    int render_frame_id,
    const GURL& url,
    bool allowed_by_default,
    const base::Callback<void(bool)>& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  extensions::WebViewPermissionHelper* web_view_permission_helper =
      extensions::WebViewPermissionHelper::FromFrameID(
          render_process_id, render_frame_id);
  web_view_permission_helper->RequestFileSystemPermission(url,
                                                          allowed_by_default,
                                                          callback);
}
#endif

void ChromeContentBrowserClient::FileSystemAccessed(
    const GURL& url,
    const std::vector<std::pair<int, int> >& render_frames,
    base::Callback<void(bool)> callback,
    bool allow) {
  // Record access to file system for potential display in UI.
  std::vector<std::pair<int, int> >::const_iterator i;
  for (i = render_frames.begin(); i != render_frames.end(); ++i) {
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&TabSpecificContentSettings::FileSystemAccessed,
                   i->first, i->second, url, !allow));
  }
  callback.Run(allow);
}

bool ChromeContentBrowserClient::AllowWorkerIndexedDB(
    const GURL& url,
    const base::string16& name,
    content::ResourceContext* context,
    const std::vector<std::pair<int, int> >& render_frames) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  ProfileIOData* io_data = ProfileIOData::FromResourceContext(context);
  content_settings::CookieSettings* cookie_settings =
      io_data->GetCookieSettings();
  bool allow = cookie_settings->IsSettingCookieAllowed(url, url);

  // Record access to IndexedDB for potential display in UI.
  std::vector<std::pair<int, int> >::const_iterator i;
  for (i = render_frames.begin(); i != render_frames.end(); ++i) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&TabSpecificContentSettings::IndexedDBAccessed,
                   i->first, i->second, url, name, !allow));
  }

  return allow;
}

#if BUILDFLAG(ENABLE_WEBRTC)
bool ChromeContentBrowserClient::AllowWebRTCIdentityCache(
    const GURL& url,
    const GURL& first_party_url,
    content::ResourceContext* context) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  ProfileIOData* io_data = ProfileIOData::FromResourceContext(context);
  content_settings::CookieSettings* cookie_settings =
      io_data->GetCookieSettings();
  return cookie_settings->IsReadingCookieAllowed(url, first_party_url) &&
         cookie_settings->IsSettingCookieAllowed(url, first_party_url);
}
#endif  // BUILDFLAG(ENABLE_WEBRTC)

bool ChromeContentBrowserClient::AllowKeygen(
    const GURL& url,
    content::ResourceContext* context) {
  HostContentSettingsMap* content_settings =
      ProfileIOData::FromResourceContext(context)->GetHostContentSettingsMap();

  return content_settings->GetContentSetting(
             url, url, CONTENT_SETTINGS_TYPE_KEYGEN, std::string()) ==
         CONTENT_SETTING_ALLOW;
}

ChromeContentBrowserClient::AllowWebBluetoothResult
ChromeContentBrowserClient::AllowWebBluetooth(
    content::BrowserContext* browser_context,
    const url::Origin& requesting_origin,
    const url::Origin& embedding_origin) {
  // TODO(crbug.com/598890): Don't disable if
  // base::CommandLine::ForCurrentProcess()->
  // HasSwitch(switches::kEnableWebBluetooth) is true.
  if (variations::GetVariationParamValue(
          PermissionContextBase::kPermissionsKillSwitchFieldStudy,
          "Bluetooth") ==
      PermissionContextBase::kPermissionsKillSwitchBlockedValue) {
    // The kill switch is enabled for this permission. Block requests.
    return AllowWebBluetoothResult::BLOCK_GLOBALLY_DISABLED;
  }

  const HostContentSettingsMap* const content_settings =
      HostContentSettingsMapFactory::GetForProfile(
          Profile::FromBrowserContext(browser_context));

  if (content_settings->GetContentSetting(
          requesting_origin.GetURL(), embedding_origin.GetURL(),
          CONTENT_SETTINGS_TYPE_BLUETOOTH_GUARD,
          std::string()) == CONTENT_SETTING_BLOCK) {
    return AllowWebBluetoothResult::BLOCK_POLICY;
  }
  return AllowWebBluetoothResult::ALLOW;
}

std::string ChromeContentBrowserClient::GetWebBluetoothBlocklist() {
  return variations::GetVariationParamValue("WebBluetoothBlocklist",
                                            "blocklist_additions");
}

net::URLRequestContext*
ChromeContentBrowserClient::OverrideRequestContextForURL(
    const GURL& url, content::ResourceContext* context) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (url.SchemeIs(extensions::kExtensionScheme)) {
    ProfileIOData* io_data = ProfileIOData::FromResourceContext(context);
    return io_data->extensions_request_context();
  }
#endif

  return NULL;
}

QuotaPermissionContext*
ChromeContentBrowserClient::CreateQuotaPermissionContext() {
  return new ChromeQuotaPermissionContext();
}

std::unique_ptr<storage::QuotaEvictionPolicy>
ChromeContentBrowserClient::GetTemporaryStorageEvictionPolicy(
    content::BrowserContext* context) {
  return SiteEngagementEvictionPolicy::IsEnabled()
             ? base::MakeUnique<SiteEngagementEvictionPolicy>(context)
             : nullptr;
}

void ChromeContentBrowserClient::AllowCertificateError(
    content::WebContents* web_contents,
    int cert_error,
    const net::SSLInfo& ssl_info,
    const GURL& request_url,
    ResourceType resource_type,
    bool overridable,
    bool strict_enforcement,
    bool expired_previous_decision,
    const base::Callback<void(content::CertificateRequestResultType)>&
        callback) {
  DCHECK(web_contents);
  if (resource_type != content::RESOURCE_TYPE_MAIN_FRAME) {
    // A sub-resource has a certificate error.  The user doesn't really
    // have a context for making the right decision, so block the
    // request hard, without an info bar to allow showing the insecure
    // content.
    if (!callback.is_null())
      callback.Run(content::CERTIFICATE_REQUEST_RESULT_TYPE_DENY);
    return;
  }

  // If the tab is being prerendered, cancel the prerender and the request.
  prerender::PrerenderContents* prerender_contents =
      prerender::PrerenderContents::FromWebContents(web_contents);
  if (prerender_contents) {
    prerender_contents->Destroy(prerender::FINAL_STATUS_SSL_ERROR);
    if (!callback.is_null()) {
      callback.Run(content::CERTIFICATE_REQUEST_RESULT_TYPE_CANCEL);
    }
    return;
  }

  // Otherwise, display an SSL blocking page. The interstitial page takes
  // ownership of ssl_blocking_page.
  int options_mask = 0;
  if (overridable)
    options_mask |= SSLErrorUI::SOFT_OVERRIDE_ENABLED;
  if (strict_enforcement)
    options_mask |= SSLErrorUI::STRICT_ENFORCEMENT;
  if (expired_previous_decision)
    options_mask |= SSLErrorUI::EXPIRED_BUT_PREVIOUSLY_ALLOWED;

  safe_browsing::SafeBrowsingService* safe_browsing_service =
      g_browser_process->safe_browsing_service();
  std::unique_ptr<SafeBrowsingSSLCertReporter> cert_reporter(
      new SafeBrowsingSSLCertReporter(safe_browsing_service
                                          ? safe_browsing_service->ui_manager()
                                          : nullptr));
  SSLErrorHandler::HandleSSLError(web_contents, cert_error, ssl_info,
                                  request_url, options_mask,
                                  std::move(cert_reporter), callback);
}

void ChromeContentBrowserClient::SelectClientCertificate(
    content::WebContents* web_contents,
    net::SSLCertRequestInfo* cert_request_info,
    std::unique_ptr<content::ClientCertificateDelegate> delegate) {
  prerender::PrerenderContents* prerender_contents =
      prerender::PrerenderContents::FromWebContents(web_contents);
  if (prerender_contents) {
    prerender_contents->Destroy(
        prerender::FINAL_STATUS_SSL_CLIENT_CERTIFICATE_REQUESTED);
    return;
  }

  GURL requesting_url("https://" + cert_request_info->host_and_port.ToString());
  DCHECK(requesting_url.is_valid())
      << "Invalid URL string: https://"
      << cert_request_info->host_and_port.ToString();

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  std::unique_ptr<base::Value> filter =
      HostContentSettingsMapFactory::GetForProfile(profile)->GetWebsiteSetting(
          requesting_url, requesting_url,
          CONTENT_SETTINGS_TYPE_AUTO_SELECT_CERTIFICATE, std::string(), NULL);

  if (filter.get()) {
    // Try to automatically select a client certificate.
    if (filter->IsType(base::Value::TYPE_DICTIONARY)) {
      base::DictionaryValue* filter_dict =
          static_cast<base::DictionaryValue*>(filter.get());

      const std::vector<scoped_refptr<net::X509Certificate> >&
          all_client_certs = cert_request_info->client_certs;
      for (size_t i = 0; i < all_client_certs.size(); ++i) {
        if (CertMatchesFilter(*all_client_certs[i].get(), *filter_dict)) {
          // Use the first certificate that is matched by the filter.
          delegate->ContinueWithCertificate(all_client_certs[i].get());
          return;
        }
      }
    } else {
      NOTREACHED();
    }
  }

  chrome::ShowSSLClientCertificateSelector(web_contents, cert_request_info,
                                           std::move(delegate));
}

content::MediaObserver* ChromeContentBrowserClient::GetMediaObserver() {
  return MediaCaptureDevicesDispatcher::GetInstance();
}

content::PlatformNotificationService*
ChromeContentBrowserClient::GetPlatformNotificationService() {
#if defined(ENABLE_NOTIFICATIONS)
  return PlatformNotificationServiceImpl::GetInstance();
#else
  NOTIMPLEMENTED();
  return NULL;
#endif
}

bool ChromeContentBrowserClient::CanCreateWindow(
    const GURL& opener_url,
    const GURL& opener_top_level_frame_url,
    const GURL& source_origin,
    WindowContainerType container_type,
    const GURL& target_url,
    const content::Referrer& referrer,
    const std::string& frame_name,
    WindowOpenDisposition disposition,
    const WebWindowFeatures& features,
    bool user_gesture,
    bool opener_suppressed,
    content::ResourceContext* context,
    int render_process_id,
    int opener_render_view_id,
    int opener_render_frame_id,
    bool* no_javascript_access) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  *no_javascript_access = false;

  // If the opener is trying to create a background window but doesn't have
  // the appropriate permission, fail the attempt.
  if (container_type == WINDOW_CONTAINER_TYPE_BACKGROUND) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
    ProfileIOData* io_data = ProfileIOData::FromResourceContext(context);
    InfoMap* map = io_data->GetExtensionInfoMap();
    if (!map->SecurityOriginHasAPIPermission(
            source_origin,
            render_process_id,
            APIPermission::kBackground)) {
      return false;
    }

    // Note: this use of GetExtensionOrAppByURL is safe but imperfect.  It may
    // return a recently installed Extension even if this CanCreateWindow call
    // was made by an old copy of the page in a normal web process.  That's ok,
    // because the permission check above would have caused an early return
    // already. We must use the full URL to find hosted apps, though, and not
    // just the origin.
    const Extension* extension =
        map->extensions().GetExtensionOrAppByURL(opener_url);
    if (extension && !extensions::BackgroundInfo::AllowJSAccess(extension))
      *no_javascript_access = true;
#endif

    return true;
  }

#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (extensions::WebViewRendererState::GetInstance()->IsGuest(
      render_process_id)) {
    return true;
  }

  if (target_url.SchemeIs(extensions::kExtensionScheme) ||
      target_url.SchemeIs(extensions::kExtensionResourceScheme)) {
    // Intentionally duplicating |io_data| and |map| code from above because we
    // want to reduce calls to retrieve them as this function is a SYNC IPC
    // handler.
    ProfileIOData* io_data = ProfileIOData::FromResourceContext(context);
    InfoMap* map = io_data->GetExtensionInfoMap();
    const Extension* extension =
        map->extensions().GetExtensionOrAppByURL(opener_url);
    // TODO(lazyboy): http://crbug.com/585570, if |extension| is a platform app,
    // disallow loading it in a tab via window.open(). Currently there are apps
    // that rely on this to be allowed to get Cast API to work, so we are
    // allowing this temporarily. Once Media Router is available on stable, this
    // exception should not be required.
    if (extension && extension->is_platform_app()) {
      AppLoadedInTabSource source =
          opener_top_level_frame_url ==
                  extensions::BackgroundInfo::GetBackgroundURL(extension)
              ? APP_LOADED_IN_TAB_SOURCE_BACKGROUND_PAGE
              : APP_LOADED_IN_TAB_SOURCE_APP;
      UMA_HISTOGRAM_ENUMERATION("Extensions.AppLoadedInTab", source,
                                APP_LOADED_IN_TAB_SOURCE_MAX);
    }
  }
#endif

  HostContentSettingsMap* content_settings =
      ProfileIOData::FromResourceContext(context)->GetHostContentSettingsMap();

#if BUILDFLAG(ENABLE_PLUGINS)
  if (FlashDownloadInterception::ShouldStopFlashDownloadAction(
          content_settings, opener_top_level_frame_url, target_url,
          user_gesture)) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&HandleFlashDownloadActionOnUIThread, render_process_id,
                   opener_render_frame_id, opener_top_level_frame_url));
    return false;
  }
#endif

  BlockedWindowParams blocked_params(target_url,
                                     referrer,
                                     frame_name,
                                     disposition,
                                     features,
                                     user_gesture,
                                     opener_suppressed,
                                     render_process_id,
                                     opener_render_frame_id);

  if (!user_gesture &&
      !base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisablePopupBlocking)) {
    if (content_settings->GetContentSetting(opener_top_level_frame_url,
                                            opener_top_level_frame_url,
                                            CONTENT_SETTINGS_TYPE_POPUPS,
                                            std::string()) !=
        CONTENT_SETTING_ALLOW) {
      BrowserThread::PostTask(BrowserThread::UI,
                              FROM_HERE,
                              base::Bind(&HandleBlockedPopupOnUIThread,
                                         blocked_params));
      return false;
    }
  }

#if BUILDFLAG(ANDROID_JAVA_UI)
  if (SingleTabModeTabHelper::IsRegistered(render_process_id,
                                           opener_render_view_id)) {
    BrowserThread::PostTask(BrowserThread::UI,
                            FROM_HERE,
                            base::Bind(&HandleSingleTabModeBlockOnUIThread,
                                       blocked_params));
    return false;
  }
#endif

  return true;
}

void ChromeContentBrowserClient::ResourceDispatcherHostCreated() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  for (size_t i = 0; i < extra_parts_.size(); ++i)
    extra_parts_[i]->ResourceDispatcherHostCreated();

  return g_browser_process->ResourceDispatcherHostCreated();
}

content::SpeechRecognitionManagerDelegate*
    ChromeContentBrowserClient::CreateSpeechRecognitionManagerDelegate() {
  return new speech::ChromeSpeechRecognitionManagerDelegate();
}

net::NetLog* ChromeContentBrowserClient::GetNetLog() {
  return g_browser_process->net_log();
}

void ChromeContentBrowserClient::OverrideWebkitPrefs(
    RenderViewHost* rvh, WebPreferences* web_prefs) {
  Profile* profile = Profile::FromBrowserContext(
      rvh->GetProcess()->GetBrowserContext());
  PrefService* prefs = profile->GetPrefs();

  // Fill per-script font preferences. These are not registered on Android
  // - http://crbug.com/308033.
#if !defined(OS_ANDROID)
  FontFamilyCache::FillFontFamilyMap(profile,
                                     prefs::kWebKitStandardFontFamilyMap,
                                     &web_prefs->standard_font_family_map);
  FontFamilyCache::FillFontFamilyMap(profile,
                                     prefs::kWebKitFixedFontFamilyMap,
                                     &web_prefs->fixed_font_family_map);
  FontFamilyCache::FillFontFamilyMap(profile,
                                     prefs::kWebKitSerifFontFamilyMap,
                                     &web_prefs->serif_font_family_map);
  FontFamilyCache::FillFontFamilyMap(profile,
                                     prefs::kWebKitSansSerifFontFamilyMap,
                                     &web_prefs->sans_serif_font_family_map);
  FontFamilyCache::FillFontFamilyMap(profile,
                                     prefs::kWebKitCursiveFontFamilyMap,
                                     &web_prefs->cursive_font_family_map);
  FontFamilyCache::FillFontFamilyMap(profile,
                                     prefs::kWebKitFantasyFontFamilyMap,
                                     &web_prefs->fantasy_font_family_map);
  FontFamilyCache::FillFontFamilyMap(profile,
                                     prefs::kWebKitPictographFontFamilyMap,
                                     &web_prefs->pictograph_font_family_map);
#endif

  web_prefs->default_font_size =
      prefs->GetInteger(prefs::kWebKitDefaultFontSize);
  web_prefs->default_fixed_font_size =
      prefs->GetInteger(prefs::kWebKitDefaultFixedFontSize);
  web_prefs->minimum_font_size =
      prefs->GetInteger(prefs::kWebKitMinimumFontSize);
  web_prefs->minimum_logical_font_size =
      prefs->GetInteger(prefs::kWebKitMinimumLogicalFontSize);

  web_prefs->default_encoding = prefs->GetString(prefs::kDefaultCharset);

  web_prefs->javascript_can_open_windows_automatically =
      prefs->GetBoolean(prefs::kWebKitJavascriptCanOpenWindowsAutomatically);
  web_prefs->dom_paste_enabled =
      prefs->GetBoolean(prefs::kWebKitDomPasteEnabled);
  web_prefs->tabs_to_links = prefs->GetBoolean(prefs::kWebkitTabsToLinks);

  if (!prefs->GetBoolean(prefs::kWebKitJavascriptEnabled))
    web_prefs->javascript_enabled = false;

  // Only allow disabling web security via the command-line flag if the user
  // has specified a distinct profile directory. This still enables tests to
  // disable web security by setting the pref directly.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (!prefs->GetBoolean(prefs::kWebKitWebSecurityEnabled)) {
    web_prefs->web_security_enabled = false;
  } else if (!web_prefs->web_security_enabled &&
             command_line->HasSwitch(switches::kDisableWebSecurity) &&
             !command_line->HasSwitch(switches::kUserDataDir)) {
    LOG(ERROR) << "Web security may only be disabled if '--user-data-dir' is "
               "also specified.";
    web_prefs->web_security_enabled = true;
  }

  if (!prefs->GetBoolean(prefs::kWebKitPluginsEnabled))
    web_prefs->plugins_enabled = false;
  web_prefs->loads_images_automatically =
      prefs->GetBoolean(prefs::kWebKitLoadsImagesAutomatically);

  if (prefs->GetBoolean(prefs::kDisable3DAPIs))
    web_prefs->experimental_webgl_enabled = false;

  web_prefs->allow_running_insecure_content =
      prefs->GetBoolean(prefs::kWebKitAllowRunningInsecureContent);
#if defined(OS_ANDROID)
  web_prefs->font_scale_factor =
      static_cast<float>(prefs->GetDouble(prefs::kWebKitFontScaleFactor));
  web_prefs->device_scale_adjustment = GetDeviceScaleAdjustment();
  web_prefs->force_enable_zoom =
      prefs->GetBoolean(prefs::kWebKitForceEnableZoom);
#endif

#if defined(OS_ANDROID)
  web_prefs->password_echo_enabled =
      prefs->GetBoolean(prefs::kWebKitPasswordEchoEnabled);
#else
  web_prefs->password_echo_enabled = browser_defaults::kPasswordEchoEnabled;
#endif

  web_prefs->text_areas_are_resizable =
      prefs->GetBoolean(prefs::kWebKitTextAreasAreResizable);
  web_prefs->hyperlink_auditing_enabled =
      prefs->GetBoolean(prefs::kEnableHyperlinkAuditing);

#if BUILDFLAG(ENABLE_EXTENSIONS)
  std::string image_animation_policy =
      prefs->GetString(prefs::kAnimationPolicy);
  if (image_animation_policy == kAnimationPolicyOnce)
    web_prefs->animation_policy =
        content::IMAGE_ANIMATION_POLICY_ANIMATION_ONCE;
  else if (image_animation_policy == kAnimationPolicyNone)
    web_prefs->animation_policy = content::IMAGE_ANIMATION_POLICY_NO_ANIMATION;
  else
    web_prefs->animation_policy = content::IMAGE_ANIMATION_POLICY_ALLOWED;
#endif

  // Make sure we will set the default_encoding with canonical encoding name.
  web_prefs->default_encoding = GetCanonicalEncodingNameByAliasName(
          web_prefs->default_encoding);
  if (web_prefs->default_encoding.empty()) {
    prefs->ClearPref(prefs::kDefaultCharset);
    web_prefs->default_encoding = prefs->GetString(prefs::kDefaultCharset);
  }
  DCHECK(!web_prefs->default_encoding.empty());

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnablePotentiallyAnnoyingSecurityFeatures)) {
    web_prefs->disable_reading_from_canvas = true;
    web_prefs->strict_mixed_content_checking = true;
    web_prefs->strict_powerful_feature_restrictions = true;
  }

  web_prefs->data_saver_enabled = GetDataSaverEnabledPref(prefs);

  for (size_t i = 0; i < extra_parts_.size(); ++i)
    extra_parts_[i]->OverrideWebkitPrefs(rvh, web_prefs);
}

void ChromeContentBrowserClient::BrowserURLHandlerCreated(
    BrowserURLHandler* handler) {
  for (size_t i = 0; i < extra_parts_.size(); ++i)
    extra_parts_[i]->BrowserURLHandlerCreated(handler);

  // about: handler. Must come before chrome: handler, since it will
  // rewrite about: urls to chrome: URLs and then expect chrome: to
  // actually handle them.  Also relies on a preliminary fixup phase.
  handler->SetFixupHandler(&FixupBrowserAboutURL);
  handler->AddHandlerPair(&WillHandleBrowserAboutURL,
                          BrowserURLHandler::null_handler());

#if BUILDFLAG(ANDROID_JAVA_UI)
  // Handler to rewrite chrome://newtab on Android.
  handler->AddHandlerPair(&chrome::android::HandleAndroidNativePageURL,
                          BrowserURLHandler::null_handler());
#else
  // Handler to rewrite chrome://newtab for InstantExtended.
  handler->AddHandlerPair(&search::HandleNewTabURLRewrite,
                          &search::HandleNewTabURLReverseRewrite);
#endif

  // chrome: & friends.
  handler->AddHandlerPair(&HandleWebUI, &HandleWebUIReverse);
}

void ChromeContentBrowserClient::ClearCache(RenderFrameHost* rfh) {
  Profile* profile = Profile::FromBrowserContext(
      rfh->GetSiteInstance()->GetProcess()->GetBrowserContext());
  BrowsingDataRemover* remover =
      BrowsingDataRemoverFactory::GetForBrowserContext(profile);
  remover->Remove(BrowsingDataRemover::Unbounded(),
                  BrowsingDataRemover::REMOVE_CACHE,
                  BrowsingDataHelper::UNPROTECTED_WEB);
}

void ChromeContentBrowserClient::ClearCookies(RenderFrameHost* rfh) {
  Profile* profile = Profile::FromBrowserContext(
      rfh->GetSiteInstance()->GetProcess()->GetBrowserContext());
  BrowsingDataRemover* remover =
      BrowsingDataRemoverFactory::GetForBrowserContext(profile);
  int remove_mask = BrowsingDataRemover::REMOVE_SITE_DATA;
  remover->Remove(BrowsingDataRemover::Unbounded(), remove_mask,
                  BrowsingDataHelper::UNPROTECTED_WEB);
}

void ChromeContentBrowserClient::ClearSiteData(
    content::BrowserContext* browser_context,
    const url::Origin& origin,
    bool remove_cookies,
    bool remove_storage,
    bool remove_cache,
    const base::Closure& callback) {
  BrowsingDataRemover* remover =
      BrowsingDataRemoverFactory::GetForBrowserContext(browser_context);

  // ClearSiteDataObserver deletes itself when callbacks from both removal
  // tasks are received.
  ClearSiteDataObserver* observer =
      new ClearSiteDataObserver(remover, callback, 2 /* number of tasks */);

  // Cookies and channel IDs are scoped to
  // a) eTLD+1 of |origin|'s host if |origin|'s host is a registrable domain
  //    or a subdomain thereof
  // b) |origin|'s host exactly if it is an IP address or an internal hostname
  //    (e.g. "localhost" or "fileserver").
  if (remove_cookies) {
    std::string domain = GetDomainAndRegistry(
        origin.host(),
        net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);

    if (domain.empty())
      domain = origin.host();  // IP address or internal hostname.

    std::unique_ptr<RegistrableDomainFilterBuilder> domain_filter_builder(
        new RegistrableDomainFilterBuilder(
            BrowsingDataFilterBuilder::WHITELIST));
    domain_filter_builder->AddRegisterableDomain(domain);

    remover->RemoveWithFilterAndReply(
        BrowsingDataRemover::Period(browsing_data::TimePeriod::ALL_TIME),
        BrowsingDataRemover::REMOVE_COOKIES |
            BrowsingDataRemover::REMOVE_CHANNEL_IDS |
            BrowsingDataRemover::REMOVE_PLUGIN_DATA,
        BrowsingDataHelper::ALL, std::move(domain_filter_builder), observer);
  } else {
    // The first removal task is a no-op.
    observer->OnBrowsingDataRemoverDone();
  }

  // Delete origin-scoped data.
  int remove_mask = 0;
  if (remove_storage) {
    remove_mask |= BrowsingDataRemover::REMOVE_SITE_DATA &
                   ~BrowsingDataRemover::REMOVE_COOKIES &
                   ~BrowsingDataRemover::REMOVE_CHANNEL_IDS &
                   ~BrowsingDataRemover::REMOVE_PLUGIN_DATA;
  }
  if (remove_cache)
    remove_mask |= BrowsingDataRemover::REMOVE_CACHE;

  if (remove_mask) {
    std::unique_ptr<OriginFilterBuilder> origin_filter_builder(
        new OriginFilterBuilder(BrowsingDataFilterBuilder::WHITELIST));
    origin_filter_builder->AddOrigin(origin);

    remover->RemoveWithFilterAndReply(
        BrowsingDataRemover::Period(browsing_data::TimePeriod::ALL_TIME),
        remove_mask, BrowsingDataHelper::ALL, std::move(origin_filter_builder),
        observer);
  } else {
    // The second removal task is a no-op.
    observer->OnBrowsingDataRemoverDone();
  }
}

base::FilePath ChromeContentBrowserClient::GetDefaultDownloadDirectory() {
  return DownloadPrefs::GetDefaultDownloadDirectory();
}

std::string ChromeContentBrowserClient::GetDefaultDownloadName() {
  return l10n_util::GetStringUTF8(IDS_DEFAULT_DOWNLOAD_FILENAME);
}

base::FilePath ChromeContentBrowserClient::GetShaderDiskCacheDirectory() {
  base::FilePath user_data_dir;
  PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
  DCHECK(!user_data_dir.empty());
  return user_data_dir.Append(FILE_PATH_LITERAL("ShaderCache"));
}

void ChromeContentBrowserClient::DidCreatePpapiPlugin(
    content::BrowserPpapiHost* browser_host) {
#if BUILDFLAG(ENABLE_PLUGINS)
  ChromeContentBrowserClientPluginsPart::DidCreatePpapiPlugin(browser_host);
#endif
}

content::BrowserPpapiHost*
    ChromeContentBrowserClient::GetExternalBrowserPpapiHost(
        int plugin_process_id) {
#if !defined(DISABLE_NACL)
  content::BrowserChildProcessHostIterator iter(PROCESS_TYPE_NACL_LOADER);
  while (!iter.Done()) {
    nacl::NaClProcessHost* host = static_cast<nacl::NaClProcessHost*>(
        iter.GetDelegate());
    if (host->process() &&
        host->process()->GetData().id == plugin_process_id) {
      // Found the plugin.
      return host->browser_ppapi_host();
    }
    ++iter;
  }
#endif
  return NULL;
}

gpu::GpuChannelEstablishFactory*
ChromeContentBrowserClient::GetGpuChannelEstablishFactory() {
#if defined(USE_AURA)
  if (views::WindowManagerConnection::Exists())
    return views::WindowManagerConnection::Get()->gpu_service();
#endif
  return nullptr;
}

bool ChromeContentBrowserClient::AllowPepperSocketAPI(
    content::BrowserContext* browser_context,
    const GURL& url,
    bool private_api,
    const content::SocketPermissionRequest* params) {
#if BUILDFLAG(ENABLE_PLUGINS) && BUILDFLAG(ENABLE_EXTENSIONS)
  return ChromeContentBrowserClientPluginsPart::AllowPepperSocketAPI(
      browser_context, url, private_api, params, allowed_socket_origins_);
#else
  return false;
#endif
}

bool ChromeContentBrowserClient::IsPepperVpnProviderAPIAllowed(
    content::BrowserContext* browser_context,
    const GURL& url) {
#if BUILDFLAG(ENABLE_PLUGINS) && BUILDFLAG(ENABLE_EXTENSIONS)
  return ChromeContentBrowserClientPluginsPart::IsPepperVpnProviderAPIAllowed(
      browser_context, url);
#else
  return false;
#endif
}

std::unique_ptr<content::VpnServiceProxy>
ChromeContentBrowserClient::GetVpnServiceProxy(
    content::BrowserContext* browser_context) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  return ChromeContentBrowserClientExtensionsPart::GetVpnServiceProxy(
      browser_context);
#else
  return nullptr;
#endif
}

ui::SelectFilePolicy* ChromeContentBrowserClient::CreateSelectFilePolicy(
    WebContents* web_contents) {
  return new ChromeSelectFilePolicy(web_contents);
}

void ChromeContentBrowserClient::GetAdditionalAllowedSchemesForFileSystem(
    std::vector<std::string>* additional_allowed_schemes) {
  ContentBrowserClient::GetAdditionalAllowedSchemesForFileSystem(
      additional_allowed_schemes);
  additional_allowed_schemes->push_back(content::kChromeDevToolsScheme);
  additional_allowed_schemes->push_back(content::kChromeUIScheme);
  for (size_t i = 0; i < extra_parts_.size(); ++i) {
    extra_parts_[i]->GetAdditionalAllowedSchemesForFileSystem(
        additional_allowed_schemes);
  }
}

void ChromeContentBrowserClient::GetSchemesBypassingSecureContextCheckWhitelist(
    std::set<std::string>* schemes) {
  return ::GetSchemesBypassingSecureContextCheckWhitelist(schemes);
}

void ChromeContentBrowserClient::GetURLRequestAutoMountHandlers(
    std::vector<storage::URLRequestAutoMountHandler>* handlers) {
  for (size_t i = 0; i < extra_parts_.size(); ++i)
    extra_parts_[i]->GetURLRequestAutoMountHandlers(handlers);
}

void ChromeContentBrowserClient::GetAdditionalFileSystemBackends(
    content::BrowserContext* browser_context,
    const base::FilePath& storage_partition_path,
    ScopedVector<storage::FileSystemBackend>* additional_backends) {
#if defined(OS_CHROMEOS)
  storage::ExternalMountPoints* external_mount_points =
      content::BrowserContext::GetMountPoints(browser_context);
  DCHECK(external_mount_points);
  chromeos::FileSystemBackend* backend = new chromeos::FileSystemBackend(
      base::MakeUnique<drive::FileSystemBackendDelegate>(),
      base::MakeUnique<chromeos::file_system_provider::BackendDelegate>(),
      base::MakeUnique<chromeos::MTPFileSystemBackendDelegate>(
          storage_partition_path),
      base::MakeUnique<arc::ArcContentFileSystemBackendDelegate>(),
      external_mount_points, storage::ExternalMountPoints::GetSystemInstance());
  backend->AddSystemMountPoints();
  DCHECK(backend->CanHandleType(storage::kFileSystemTypeExternal));
  additional_backends->push_back(backend);
#endif

  for (size_t i = 0; i < extra_parts_.size(); ++i) {
    extra_parts_[i]->GetAdditionalFileSystemBackends(
        browser_context, storage_partition_path, additional_backends);
  }
}

#if defined(OS_ANDROID)
void ChromeContentBrowserClient::GetAdditionalMappedFilesForChildProcess(
    const base::CommandLine& command_line,
    int child_process_id,
    FileDescriptorInfo* mappings,
    std::map<int, base::MemoryMappedFile::Region>* regions) {
  int fd = ui::GetMainAndroidPackFd(
      &(*regions)[kAndroidUIResourcesPakDescriptor]);
  mappings->Share(kAndroidUIResourcesPakDescriptor, fd);

  fd = ui::GetCommonResourcesPackFd(
      &(*regions)[kAndroidChrome100PercentPakDescriptor]);
  mappings->Share(kAndroidChrome100PercentPakDescriptor, fd);

  fd = ui::GetLocalePackFd(&(*regions)[kAndroidLocalePakDescriptor]);
  mappings->Share(kAndroidLocalePakDescriptor, fd);

  if (breakpad::IsCrashReporterEnabled()) {
    base::File file =
        breakpad::CrashDumpManager::GetInstance()->CreateMinidumpFile(
            child_process_id);
    if (file.IsValid()) {
      mappings->Transfer(kAndroidMinidumpDescriptor,
                         base::ScopedFD(file.TakePlatformFile()));
    } else {
      LOG(ERROR) << "Failed to create file for minidump, crash reporting will "
                 "be disabled for this process.";
    }
  }

  base::FilePath app_data_path;
  PathService::Get(base::DIR_ANDROID_APP_DATA, &app_data_path);
  DCHECK(!app_data_path.empty());
}
#elif defined(OS_POSIX) && !defined(OS_MACOSX)
void ChromeContentBrowserClient::GetAdditionalMappedFilesForChildProcess(
    const base::CommandLine& command_line,
    int child_process_id,
    FileDescriptorInfo* mappings) {
  int crash_signal_fd = GetCrashSignalFD(command_line);
  if (crash_signal_fd >= 0) {
    mappings->Share(kCrashDumpSignal, crash_signal_fd);
  }
}
#endif  // defined(OS_ANDROID)

#if defined(OS_WIN)
base::string16 ChromeContentBrowserClient::GetAppContainerSidForSandboxType(
    int sandbox_type) const {
  base::string16 sid;

#if defined(GOOGLE_CHROME_BUILD)
  const version_info::Channel channel = chrome::GetChannel();

  // It's possible to have a SxS installation running at the same time as a
  // non-SxS so isolate them from each other.
  if (channel == version_info::Channel::CANARY) {
    sid.assign(
        L"S-1-15-2-3251537155-1984446955-2931258699-841473695-1938553385-"
        L"924012150-");
  } else {
    sid.assign(
        L"S-1-15-2-3251537155-1984446955-2931258699-841473695-1938553385-"
        L"924012149-");
  }
#else
  sid.assign(
      L"S-1-15-2-3251537155-1984446955-2931258699-841473695-1938553385-"
      L"924012148-");
#endif

  // TODO(wfh): Add support for more process types here. crbug.com/499523
  switch (sandbox_type) {
    case content::SANDBOX_TYPE_RENDERER:
      return sid + L"129201922";
    case content::SANDBOX_TYPE_UTILITY:
      return base::string16();
    case content::SANDBOX_TYPE_GPU:
      return base::string16();
    case content::SANDBOX_TYPE_PPAPI:
      return sid + L"129201925";
#if !defined(DISABLE_NACL)
    case PROCESS_TYPE_NACL_LOADER:
      return base::string16();
    case PROCESS_TYPE_NACL_BROKER:
      return base::string16();
#endif
  }

  // Should never reach here.
  CHECK(0);
  return base::string16();
}

bool ChromeContentBrowserClient::PreSpawnRenderer(
    sandbox::TargetPolicy* policy) {
  // This code is duplicated in nacl_exe_win_64.cc.
  // Allow the server side of a pipe restricted to the "chrome.nacl."
  // namespace so that it cannot impersonate other system or other chrome
  // service pipes.
  sandbox::ResultCode result = policy->AddRule(
      sandbox::TargetPolicy::SUBSYS_NAMED_PIPES,
      sandbox::TargetPolicy::NAMEDPIPES_ALLOW_ANY,
      L"\\\\.\\pipe\\chrome.nacl.*");
  if (result != sandbox::SBOX_ALL_OK)
    return false;
  return result == sandbox::SBOX_ALL_OK;
}
#endif  // defined(OS_WIN)

void ChromeContentBrowserClient::ExposeInterfacesToRenderer(
    service_manager::InterfaceRegistry* registry,
    content::RenderProcessHost* render_process_host) {
  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner =
      content::BrowserThread::GetTaskRunnerForThread(
          content::BrowserThread::UI);
  registry->AddInterface(
      base::Bind(&startup_metric_utils::StartupMetricHostImpl::Create),
      ui_task_runner);
  registry->AddInterface(
      base::Bind(&BudgetServiceImpl::Create, render_process_host->GetID()),
      ui_task_runner);
  registry->AddInterface(
      base::Bind(&rappor::RapporRecorderImpl::Create,
                 g_browser_process->rappor_service()),
      ui_task_runner);

#if defined(OS_CHROMEOS)
  registry->AddInterface<metrics::mojom::LeakDetector>(
      base::Bind(&metrics::LeakDetectorRemoteController::Create),
      ui_task_runner);
#endif
}

void ChromeContentBrowserClient::ExposeInterfacesToMediaService(
    service_manager::InterfaceRegistry* registry,
    content::RenderFrameHost* render_frame_host) {
// TODO(xhwang): Only register this when ENABLE_MOJO_MEDIA.
#if defined(OS_CHROMEOS)
  registry->AddInterface(
      base::Bind(&chromeos::attestation::PlatformVerificationImpl::Create,
                 render_frame_host));
#endif  // defined(OS_CHROMEOS)

#if defined(ENABLE_MOJO_MEDIA)
  registry->AddInterface(
      base::Bind(&OutputProtectionImpl::Create, render_frame_host));
#endif  // defined(ENABLE_MOJO_MEDIA)
}

void ChromeContentBrowserClient::RegisterRenderFrameMojoInterfaces(
    service_manager::InterfaceRegistry* registry,
    content::RenderFrameHost* render_frame_host) {
  if (base::FeatureList::IsEnabled(features::kWebUsb)
#if BUILDFLAG(ENABLE_EXTENSIONS)
      &&
      !render_frame_host->GetSiteInstance()->GetSiteURL().SchemeIs(
          extensions::kExtensionScheme)
#endif
          ) {
    registry->AddInterface(
        base::Bind(&CreateUsbDeviceManager, render_frame_host));
    registry->AddInterface(
        base::Bind(&CreateWebUsbChooserService, render_frame_host));
  }

  registry->AddInterface<bluetooth::mojom::AdapterFactory>(
      base::Bind(&bluetooth::AdapterFactory::Create));

  if (!render_frame_host->GetParent()) {
    // Register mojo CredentialManager interface only for main frame.
    registry->AddInterface(
        base::Bind(&ChromePasswordManagerClient::BindCredentialManager,
                   render_frame_host));
    // Register mojo ContentTranslateDriver interface only for main frame.
    registry->AddInterface(base::Bind(
        &ChromeTranslateClient::BindContentTranslateDriver, render_frame_host));
  }

  registry->AddInterface(
      base::Bind(&autofill::ContentAutofillDriverFactory::BindAutofillDriver,
                 render_frame_host));

  registry->AddInterface(
      base::Bind(&password_manager::ContentPasswordManagerDriverFactory::
                     BindPasswordManagerDriver,
                 render_frame_host));

  registry->AddInterface(
      base::Bind(&password_manager::ContentPasswordManagerDriverFactory::
                     BindSensitiveInputVisibilityService,
                 render_frame_host));

#if BUILDFLAG(ANDROID_JAVA_UI)
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);
  if (web_contents) {
    registry->AddInterface(
        web_contents->GetJavaInterfaces()
            ->CreateInterfaceFactory<payments::mojom::PaymentRequest>());
    registry->AddInterface(
        base::Bind(&ForwardShareServiceRequest,
                   web_contents->GetJavaInterfaces()->GetWeakPtr()));
  }
#elif defined(OS_LINUX) || defined(OS_CHROMEOS) || defined(OS_WIN)
  if (AreExperimentalWebPlatformFeaturesEnabled()) {
    content::WebContents* web_contents =
        content::WebContents::FromRenderFrameHost(render_frame_host);
    if (web_contents) {
      registry->AddInterface(
          base::Bind(CreatePaymentRequestHandler, web_contents));
    }
  }
#endif
}

void ChromeContentBrowserClient::ExposeInterfacesToGpuProcess(
    service_manager::InterfaceRegistry* registry,
    content::GpuProcessHost* render_process_host) {
  registry->AddInterface(
      base::Bind(&metrics::CallStackProfileCollector::Create,
                 metrics::CallStackProfileParams::GPU_PROCESS));
}

void ChromeContentBrowserClient::RegisterInProcessServices(
    StaticServiceMap* services) {
#if (ENABLE_MOJO_MEDIA_IN_BROWSER_PROCESS)
  content::ServiceInfo info;
  info.factory = base::Bind(&media::CreateMediaService);
  services->insert(std::make_pair("media", info));
#endif
#if defined(OS_CHROMEOS)
  content::ServiceManagerConnection::GetForProcess()->AddConnectionFilter(
      base::MakeUnique<chromeos::ChromeInterfaceFactory>());
#endif  // OS_CHROMEOS
}

void ChromeContentBrowserClient::RegisterOutOfProcessServices(
      OutOfProcessServiceMap* services) {
#if defined(ENABLE_MOJO_MEDIA_IN_UTILITY_PROCESS)
  services->insert(std::make_pair("media",
                                  base::ASCIIToUTF16("Media Service")));
#endif
  services->insert(std::make_pair(image_decoder::mojom::kServiceName,
                                  base::ASCIIToUTF16("Image Decoder Service")));
}

std::unique_ptr<base::Value>
ChromeContentBrowserClient::GetServiceManifestOverlay(
    const std::string& name) {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  int id = -1;
  if (name == content::mojom::kBrowserServiceName)
    id = IDR_CHROME_CONTENT_BROWSER_MANIFEST_OVERLAY;
  else if (name == content::mojom::kGpuServiceName)
    id = IDR_CHROME_CONTENT_GPU_MANIFEST_OVERLAY;
  else if (name == content::mojom::kPluginServiceName)
    id = IDR_CHROME_CONTENT_PLUGIN_MANIFEST_OVERLAY;
  else if (name == content::mojom::kRendererServiceName)
    id = IDR_CHROME_CONTENT_RENDERER_MANIFEST_OVERLAY;
  else if (name == content::mojom::kUtilityServiceName)
    id = IDR_CHROME_CONTENT_UTILITY_MANIFEST_OVERLAY;
  if (id == -1)
    return nullptr;

  base::StringPiece manifest_contents =
      rb.GetRawDataResourceForScale(id, ui::ScaleFactor::SCALE_FACTOR_NONE);
  return base::JSONReader::Read(manifest_contents);
}

void ChromeContentBrowserClient::OpenURL(
    content::BrowserContext* browser_context,
    const content::OpenURLParams& params,
    const base::Callback<void(content::WebContents*)>& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

#if BUILDFLAG(ANDROID_JAVA_UI)
  ServiceTabLauncher::GetInstance()->LaunchTab(browser_context, params,
                                               callback);
#else
  chrome::NavigateParams nav_params(
      Profile::FromBrowserContext(browser_context),
      params.url,
      params.transition);
  FillNavigateParamsFromOpenURLParams(&nav_params, params);
  nav_params.user_gesture = params.user_gesture;

  Navigate(&nav_params);
  callback.Run(nav_params.target_contents);
#endif
}

content::PresentationServiceDelegate*
ChromeContentBrowserClient::GetPresentationServiceDelegate(
      content::WebContents* web_contents) {
#if defined(ENABLE_MEDIA_ROUTER)
  if (media_router::MediaRouterEnabled(web_contents->GetBrowserContext())) {
    return media_router::PresentationServiceDelegateImpl::
        GetOrCreateForWebContents(web_contents);
  }
#endif  // defined(ENABLE_MEDIA_ROUTER)
  return nullptr;
}

void ChromeContentBrowserClient::RecordURLMetric(const std::string& metric,
                                                 const GURL& url) {
  if (url.is_valid()) {
    rappor::SampleDomainAndRegistryFromGURL(g_browser_process->rappor_service(),
                                            metric, url);
  }
}

ScopedVector<content::NavigationThrottle>
ChromeContentBrowserClient::CreateThrottlesForNavigation(
    content::NavigationHandle* handle) {
  ScopedVector<content::NavigationThrottle> throttles;

#if BUILDFLAG(ENABLE_PLUGINS)
  std::unique_ptr<content::NavigationThrottle> flash_url_throttle =
      FlashDownloadInterception::MaybeCreateThrottleFor(handle);
  if (flash_url_throttle)
    throttles.push_back(std::move(flash_url_throttle));
#endif

  if (handle->IsInMainFrame()) {
    throttles.push_back(
        page_load_metrics::MetricsNavigationThrottle::Create(handle));
  }

#if defined(OS_ANDROID)
  // TODO(davidben): This is insufficient to integrate with prerender properly.
  // https://crbug.com/370595
  prerender::PrerenderContents* prerender_contents =
      prerender::PrerenderContents::FromWebContents(handle->GetWebContents());
  if (!prerender_contents && handle->IsInMainFrame()) {
    throttles.push_back(
        navigation_interception::InterceptNavigationDelegate::CreateThrottleFor(
            handle));
  }
#else
  if (handle->IsInMainFrame()) {
    // Redirect some navigations to apps that have registered matching URL
    // handlers ('url_handlers' in the manifest).
    std::unique_ptr<content::NavigationThrottle> url_to_app_throttle =
        AppUrlRedirector::MaybeCreateThrottleFor(handle);
    if (url_to_app_throttle)
      throttles.push_back(std::move(url_to_app_throttle));
  }
#endif

#if defined(OS_CHROMEOS)
  // Check if we need to add merge session throttle. This throttle will postpone
  // loading of main frames.
  if (handle->IsInMainFrame()) {
    // Add interstitial page while merge session process (cookie reconstruction
    // from OAuth2 refresh token in ChromeOS login) is still in progress while
    // we are attempting to load a google property.
    if (merge_session_throttling_utils::ShouldAttachNavigationThrottle() &&
        !merge_session_throttling_utils::AreAllSessionMergedAlready() &&
        handle->GetURL().SchemeIsHTTPOrHTTPS()) {
      throttles.push_back(MergeSessionNavigationThrottle::Create(handle));
    }

    const arc::ArcSessionManager* arc_session_manager =
        arc::ArcSessionManager::Get();
    if (arc_session_manager && arc_session_manager->IsArcEnabled() &&
        !handle->GetWebContents()->GetBrowserContext()->IsOffTheRecord()) {
      prerender::PrerenderContents* prerender_contents =
          prerender::PrerenderContents::FromWebContents(
              handle->GetWebContents());
      if (!prerender_contents) {
        auto intent_picker_cb = base::Bind(ShowIntentPickerBubble());
        auto url_to_arc_throttle = base::MakeUnique<arc::ArcNavigationThrottle>(
            handle, intent_picker_cb);
        throttles.push_back(std::move(url_to_arc_throttle));
      }
    }
  }
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
  throttles.push_back(new extensions::ExtensionNavigationThrottle(handle));
#endif

  return throttles;
}

std::unique_ptr<content::NavigationUIData>
ChromeContentBrowserClient::GetNavigationUIData(
    content::NavigationHandle* navigation_handle) {
  return base::MakeUnique<ChromeNavigationUIData>(navigation_handle);
}

content::DevToolsManagerDelegate*
ChromeContentBrowserClient::GetDevToolsManagerDelegate() {
#if defined(OS_ANDROID)
  return new DevToolsManagerDelegateAndroid();
#else
  return new ChromeDevToolsManagerDelegate();
#endif
}

content::TracingDelegate* ChromeContentBrowserClient::GetTracingDelegate() {
  return new ChromeTracingDelegate();
}

bool ChromeContentBrowserClient::IsPluginAllowedToCallRequestOSFileHandle(
    content::BrowserContext* browser_context,
    const GURL& url) {
#if BUILDFLAG(ENABLE_PLUGINS) && BUILDFLAG(ENABLE_EXTENSIONS)
  return ChromeContentBrowserClientPluginsPart::
      IsPluginAllowedToCallRequestOSFileHandle(browser_context, url,
                                               allowed_file_handle_origins_);
#else
  return false;
#endif
}

bool ChromeContentBrowserClient::IsPluginAllowedToUseDevChannelAPIs(
    content::BrowserContext* browser_context,
    const GURL& url) {
#if BUILDFLAG(ENABLE_PLUGINS) && BUILDFLAG(ENABLE_EXTENSIONS)
  return ChromeContentBrowserClientPluginsPart::
      IsPluginAllowedToUseDevChannelAPIs(browser_context, url,
                                         allowed_dev_channel_origins_);
#else
  return false;
#endif
}

void ChromeContentBrowserClient::OverridePageVisibilityState(
      RenderFrameHost* render_frame_host,
      blink::WebPageVisibilityState* visibility_state) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  WebContents* web_contents =
      WebContents::FromRenderFrameHost(render_frame_host);
  DCHECK(web_contents);

  prerender::PrerenderManager* prerender_manager =
      prerender::PrerenderManagerFactory::GetForBrowserContext(
          web_contents->GetBrowserContext());
  if (prerender_manager &&
      prerender_manager->IsWebContentsPrerendering(web_contents, nullptr)) {
    *visibility_state = blink::WebPageVisibilityStatePrerender;
  }
}

#if BUILDFLAG(ENABLE_WEBRTC)
void ChromeContentBrowserClient::MaybeCopyDisableWebRtcEncryptionSwitch(
    base::CommandLine* to_command_line,
    const base::CommandLine& from_command_line,
    version_info::Channel channel) {
#if defined(OS_ANDROID)
  const version_info::Channel kMaxDisableEncryptionChannel =
      version_info::Channel::BETA;
#else
  const version_info::Channel kMaxDisableEncryptionChannel =
      version_info::Channel::DEV;
#endif
  if (channel <= kMaxDisableEncryptionChannel) {
    static const char* const kWebRtcDevSwitchNames[] = {
      switches::kDisableWebRtcEncryption,
    };
    to_command_line->CopySwitchesFrom(from_command_line,
                                      kWebRtcDevSwitchNames,
                                      arraysize(kWebRtcDevSwitchNames));
  }
}
#endif  // BUILDFLAG(ENABLE_WEBRTC)

std::unique_ptr<content::MemoryCoordinatorDelegate>
ChromeContentBrowserClient::GetMemoryCoordinatorDelegate() {
  return memory::ChromeMemoryCoordinatorDelegate::Create();
}

#if BUILDFLAG(ENABLE_MEDIA_REMOTING)
void ChromeContentBrowserClient::CreateMediaRemoter(
    content::RenderFrameHost* render_frame_host,
    media::mojom::RemotingSourcePtr source,
    media::mojom::RemoterRequest request) {
#if defined(ENABLE_MEDIA_ROUTER)
  CastRemotingConnector::CreateMediaRemoter(
      render_frame_host, std::move(source), std::move(request));
#else
  // Chrome's media remoting implementation depends on the Media Router
  // infrastructure to identify remote sinks and provide the user interface for
  // sink selection. In the case where the Media Router is not present, simply
  // drop the interface request. This will prevent code paths for media remoting
  // in the renderer process from activating.
#endif
}
#endif  // BUILDFLAG(ENABLE_MEDIA_REMOTING)
