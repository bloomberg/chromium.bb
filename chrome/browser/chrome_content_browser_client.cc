// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_content_browser_client.h"

#include <iterator>
#include <map>
#include <set>
#include <utility>
#include <vector>

#include "base/base_switches.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/i18n/base_i18n_switches.h"
#include "base/i18n/character_encoding.h"
#include "base/macros.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/sequenced_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "base/task/post_task.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "chrome/app/chrome_content_browser_overlay_manifest.h"
#include "chrome/browser/accessibility/accessibility_labels_service.h"
#include "chrome/browser/accessibility/accessibility_labels_service_factory.h"
#include "chrome/browser/accessibility/caption_util.h"
#include "chrome/browser/after_startup_task_utils.h"
#include "chrome/browser/bluetooth/chrome_bluetooth_delegate.h"
#include "chrome/browser/browser_about_handler.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_delegate.h"
#include "chrome/browser/captive_portal/captive_portal_service_factory.h"
#include "chrome/browser/chrome_content_browser_client_parts.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry_factory.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings_factory.h"
#include "chrome/browser/data_use_measurement/chrome_data_use_measurement.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/engagement/site_engagement_service.h"
#include "chrome/browser/extensions/chrome_extension_cookies.h"
#include "chrome/browser/external_protocol/external_protocol_handler.h"
#include "chrome/browser/font_family_cache.h"
#include "chrome/browser/gpu/chrome_browser_main_extra_parts_gpu.h"
#include "chrome/browser/hid/chrome_hid_delegate.h"
#include "chrome/browser/interstitials/enterprise_util.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/browser/lookalikes/lookalike_url_navigation_throttle.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/media/router/presentation/presentation_service_delegate_impl.h"
#include "chrome/browser/media/router/presentation/receiver_presentation_service_delegate_impl.h"
#include "chrome/browser/media/webrtc/audio_debug_recordings_handler.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/webrtc/webrtc_logging_controller.h"
#include "chrome/browser/memory/chrome_browser_main_extra_parts_memory.h"
#include "chrome/browser/metrics/chrome_browser_main_extra_parts_metrics.h"
#include "chrome/browser/metrics/chrome_feature_list_creator.h"
#include "chrome/browser/nacl_host/nacl_browser_delegate_impl.h"
#include "chrome/browser/net/chrome_network_delegate.h"
#include "chrome/browser/net/profile_network_context_service.h"
#include "chrome/browser/net/profile_network_context_service_factory.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/notifications/platform_notification_service_factory.h"
#include "chrome/browser/notifications/platform_notification_service_impl.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/payments/payment_request_display_manager_factory.h"
#include "chrome/browser/performance_manager/chrome_browser_main_extra_parts_performance_manager.h"
#include "chrome/browser/performance_manager/chrome_content_browser_client_performance_manager_part.h"
#include "chrome/browser/permissions/attestation_permission_request.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/plugins/pdf_iframe_navigation_throttle.h"
#include "chrome/browser/plugins/plugin_utils.h"
#include "chrome/browser/prerender/isolated/isolated_prerender_features.h"
#include "chrome/browser/prerender/isolated/isolated_prerender_url_loader_interceptor.h"
#include "chrome/browser/prerender/prerender_final_status.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/prerender/prerender_manager_factory.h"
#include "chrome/browser/prerender/prerender_message_filter.h"
#include "chrome/browser/prerender/prerender_util.h"
#include "chrome/browser/previews/previews_content_util.h"
#include "chrome/browser/previews/previews_service.h"
#include "chrome/browser/previews/previews_service_factory.h"
#include "chrome/browser/previews/previews_ui_tab_helper.h"
#include "chrome/browser/profiles/chrome_browser_main_extra_parts_profiles.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_io_data.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/renderer_updater.h"
#include "chrome/browser/profiles/renderer_updater_factory.h"
#include "chrome/browser/profiling_host/chrome_browser_main_extra_parts_profiling.h"
#include "chrome/browser/profiling_host/profiling_process_host.h"
#include "chrome/browser/renderer_host/chrome_navigation_ui_data.h"
#include "chrome/browser/renderer_host/pepper/chrome_browser_pepper_host_factory.h"
#include "chrome/browser/renderer_preferences_util.h"
#include "chrome/browser/resource_coordinator/background_tab_navigation_throttle.h"
#include "chrome/browser/safe_browsing/certificate_reporting_service.h"
#include "chrome/browser/safe_browsing/certificate_reporting_service_factory.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_utils.h"
#include "chrome/browser/safe_browsing/delayed_warning_navigation_throttle.h"
#include "chrome/browser/safe_browsing/safe_browsing_navigation_throttle.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/safe_browsing/ui_manager.h"
#include "chrome/browser/safe_browsing/url_checker_delegate_impl.h"
#include "chrome/browser/safe_browsing/url_lookup_service_factory.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/sharing/sms/sms_remote_fetcher.h"
#include "chrome/browser/signin/chrome_signin_proxying_url_loader_factory.h"
#include "chrome/browser/signin/chrome_signin_url_loader_throttle.h"
#include "chrome/browser/signin/header_modification_delegate_impl.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/site_isolation/site_isolation_policy.h"
#include "chrome/browser/speech/chrome_speech_recognition_manager_delegate.h"
#include "chrome/browser/speech/tts_controller_delegate_impl.h"
#include "chrome/browser/ssl/chrome_security_blocking_page_factory.h"
#include "chrome/browser/ssl/ssl_client_auth_metrics.h"
#include "chrome/browser/ssl/ssl_client_certificate_selector.h"
#include "chrome/browser/ssl/typed_navigation_timing_throttle.h"
#include "chrome/browser/subresource_filter/chrome_subresource_filter_client.h"
#include "chrome/browser/sync_file_system/local/sync_file_system_backend.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/tracing/chrome_tracing_delegate.h"
#include "chrome/browser/translate/translate_service.h"
#include "chrome/browser/ui/blocked_content/blocked_window_params.h"
#include "chrome/browser/ui/blocked_content/popup_blocker.h"
#include "chrome/browser/ui/blocked_content/tab_under_navigation_throttle.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/browser/ui/login/login_handler.h"
#include "chrome/browser/ui/login/login_navigation_throttle.h"
#include "chrome/browser/ui/login/login_tab_helper.h"
#include "chrome/browser/ui/prefs/pref_watcher.h"
#include "chrome/browser/ui/sync/sync_promo_ui.h"
#include "chrome/browser/ui/tab_contents/chrome_web_contents_view_delegate.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/chrome_web_ui_controller_factory.h"
#include "chrome/browser/ui/webui/log_web_ui_url.h"
#include "chrome/browser/usb/frame_usb_services.h"
#include "chrome/browser/vr/vr_tab_helper.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/web_app_provider_base.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/env_vars.h"
#include "chrome/common/google_url_loader_throttle.h"
#include "chrome/common/logging_chrome.h"
#include "chrome/common/pepper_permission_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/prerender_url_loader_throttle.h"
#include "chrome/common/prerender_util.h"
#include "chrome/common/profiler/stack_sampling_configuration.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/renderer_configuration.mojom.h"
#include "chrome/common/secure_origin_whitelist.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/installer/util/google_update_settings.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/browsing_data/content/browsing_data_helper.h"
#include "components/browsing_data/core/browsing_data_utils.h"
#include "components/captive_portal/content/captive_portal_service.h"
#include "components/captive_portal/core/buildflags.h"
#include "components/cdm/browser/cdm_message_filter_android.h"
#include "components/certificate_matching/certificate_principal_pattern.h"
#include "components/cloud_devices/common/cloud_devices_switches.h"
#include "components/content_settings/browser/tab_specific_content_settings.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_data.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_service.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_features.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/dom_distiller/core/dom_distiller_switches.h"
#include "components/dom_distiller/core/url_constants.h"
#include "components/embedder_support/switches.h"
#include "components/error_page/common/error_page_switches.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/feature_list.h"
#include "components/google/core/common/google_util.h"
#include "components/language/core/browser/pref_names.h"
#include "components/metrics/client_info.h"
#include "components/net_log/chrome_net_log.h"
#include "components/page_load_metrics/browser/metrics_navigation_throttle.h"
#include "components/page_load_metrics/browser/metrics_web_contents_observer.h"
#include "components/page_load_metrics/browser/page_load_metrics_util.h"
#include "components/payments/content/payment_request_display_manager.h"
#include "components/performance_manager/embedder/performance_manager_registry.h"
#include "components/permissions/permission_context_base.h"
#include "components/permissions/quota_permission_context_impl.h"
#include "components/policy/content/policy_blacklist_navigation_throttle.h"
#include "components/policy/content/policy_blacklist_service.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/policy_constants.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/previews/content/previews_decider.h"
#include "components/previews/content/previews_decider_impl.h"
#include "components/previews/content/previews_ui_service.h"
#include "components/previews/content/previews_user_data.h"
#include "components/previews/core/previews_experiments.h"
#include "components/previews/core/previews_features.h"
#include "components/previews/core/previews_switches.h"
#include "components/rappor/public/rappor_utils.h"
#include "components/rappor/rappor_service_impl.h"
#include "components/safe_browsing/buildflags.h"
#include "components/safe_browsing/content/browser/browser_url_loader_throttle.h"
#include "components/safe_browsing/content/password_protection/password_protection_navigation_throttle.h"
#include "components/safe_browsing/core/browser/url_checker_delegate.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/safe_browsing/core/db/database_manager.h"
#include "components/safe_browsing/core/features.h"
#include "components/safe_browsing/core/realtime/policy_engine.h"
#include "components/safe_browsing/core/realtime/url_lookup_service.h"
#include "components/security_interstitials/content/origin_policy_ui.h"
#include "components/security_interstitials/content/ssl_cert_reporter.h"
#include "components/security_interstitials/content/ssl_error_handler.h"
#include "components/security_interstitials/content/ssl_error_navigation_throttle.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/site_isolation/preloaded_isolated_origins.h"
#include "components/subresource_filter/content/browser/content_subresource_filter_throttle_manager.h"
#include "components/translate/core/common/translate_switches.h"
#include "components/ukm/app_source_url_recorder.h"
#include "components/url_formatter/url_fixer.h"
#include "components/variations/variations_associated_data.h"
#include "components/variations/variations_http_header_provider.h"
#include "components/variations/variations_switches.h"
#include "components/version_info/version_info.h"
#include "components/viz/common/features.h"
#include "components/viz/common/viz_utils.h"
#include "content/public/browser/browser_child_process_host.h"
#include "content/public/browser/browser_main_parts.h"
#include "content/public/browser/browser_ppapi_host.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/browser_url_handler.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/browser/certificate_request_result_type.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/client_certificate_delegate.h"
#include "content/public/browser/file_url_loader.h"
#include "content/public/browser/gpu_data_manager.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/overlay_window.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/resource_context.h"
#include "content/public/browser/site_isolation_policy.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/tts_controller.h"
#include "content/public/browser/tts_platform.h"
#include "content/public/browser/url_loader_request_interceptor.h"
#include "content/public/browser/vpn_service_proxy.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_ui_url_loader_factory.h"
#include "content/public/common/child_process_host.h"
#include "content/public/common/content_descriptors.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/navigation_policy.h"
#include "content/public/common/network_service_util.h"
#include "content/public/common/service_names.mojom.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/url_utils.h"
#include "content/public/common/user_agent.h"
#include "content/public/common/web_preferences.h"
#include "content/public/common/window_container_type.mojom-shared.h"
#include "device/vr/buildflags/buildflags.h"
#include "extensions/buildflags/buildflags.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/google_api_keys.h"
#include "gpu/config/gpu_switches.h"
#include "ipc/ipc_message.h"
#include "media/audio/audio_manager.h"
#include "media/base/media_switches.h"
#include "media/media_buildflags.h"
#include "media/mojo/buildflags.h"
#include "media/webrtc/webrtc_switches.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/scoped_interface_endpoint_handle.h"
#include "net/base/load_flags.h"
#include "net/base/mime_util.h"
#include "net/http/http_util.h"
#include "net/ssl/client_cert_store.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "pdf/buildflags.h"
#include "ppapi/buildflags/buildflags.h"
#include "ppapi/host/ppapi_host.h"
#include "printing/buildflags/buildflags.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "services/network/public/cpp/network_switches.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/service_manager/embedder/switches.h"
#include "services/service_manager/sandbox/sandbox_type.h"
#include "services/service_manager/sandbox/switches.h"
#include "services/strings/grit/services_strings.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"
#include "third_party/blink/public/common/switches.h"
#include "third_party/blink/public/mojom/renderer_preferences.mojom.h"
#include "third_party/blink/public/mojom/site_engagement/site_engagement.mojom.h"
#include "third_party/blink/public/mojom/user_agent/user_agent_metadata.mojom.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/widevine/cdm/buildflags.h"
#include "ui/base/buildflags.h"
#include "ui/base/clipboard/clipboard_format_type.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_features.h"
#include "ui/display/display.h"
#include "ui/gfx/color_utils.h"
#include "ui/native_theme/native_theme.h"
#include "ui/resources/grit/ui_resources.h"
#include "url/gurl.h"
#include "url/origin.h"

#if defined(OS_WIN)
#include "base/strings/string_tokenizer.h"
#include "base/win/win_util.h"
#include "base/win/windows_version.h"
#include "chrome/browser/chrome_browser_main_win.h"
#include "chrome/install_static/install_util.h"
#include "sandbox/win/src/sandbox_policy.h"
#elif defined(OS_MACOSX)
#include "chrome/browser/apps/intent_helper/mac_apps_navigation_throttle.h"
#include "chrome/browser/chrome_browser_main_mac.h"
#elif defined(OS_CHROMEOS)
#include "ash/public/cpp/ash_pref_names.h"
#include "ash/public/cpp/tablet_mode.h"
#include "chrome/app/chrome_crash_reporter_client.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/arc/fileapi/arc_content_file_system_backend_delegate.h"
#include "chrome/browser/chromeos/arc/fileapi/arc_documents_provider_backend_delegate.h"
#include "chrome/browser/chromeos/chrome_browser_main_chromeos.h"
#include "chrome/browser/chromeos/chrome_content_browser_client_chromeos_part.h"
#include "chrome/browser/chromeos/drive/fileapi/drivefs_file_system_backend_delegate.h"
#include "chrome/browser/chromeos/file_manager/app_id.h"
#include "chrome/browser/chromeos/file_system_provider/fileapi/backend_delegate.h"
#include "chrome/browser/chromeos/fileapi/external_file_url_loader_factory.h"
#include "chrome/browser/chromeos/fileapi/file_system_backend.h"
#include "chrome/browser/chromeos/fileapi/mtp_file_system_backend_delegate.h"
#include "chrome/browser/chromeos/login/signin/merge_session_navigation_throttle.h"
#include "chrome/browser/chromeos/login/signin/merge_session_throttling_utils.h"
#include "chrome/browser/chromeos/login/signin_partition_manager.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/policy_cert_service_factory.h"
#include "chrome/browser/chromeos/policy/system_features_disable_list_policy_handler.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/smb_client/fileapi/smbfs_file_system_backend_delegate.h"
#include "chrome/browser/chromeos/system/input_device_settings.h"
#include "chrome/browser/speech/tts_chromeos.h"
#include "chrome/browser/ui/ash/chrome_browser_main_extra_parts_ash.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chromeos/constants/chromeos_constants.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/constants/chromeos_switches.h"
#include "components/crash/core/app/breakpad_linux.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "services/service_manager/public/mojom/interface_provider_spec.mojom.h"
#elif defined(OS_LINUX)
#include "chrome/browser/chrome_browser_main_linux.h"
#elif defined(OS_ANDROID)
#include "base/android/application_status_listener.h"
#include "chrome/android/features/dev_ui/buildflags.h"
#include "chrome/android/modules/extra_icu/provider/module_provider.h"
#include "chrome/browser/android/app_hooks.h"
#include "chrome/browser/android/customtabs/client_data_header_web_contents_observer.h"
#include "chrome/browser/android/devtools_manager_delegate_android.h"
#include "chrome/browser/android/ntp/new_tab_page_url_handler.h"
#include "chrome/browser/android/service_tab_launcher.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/android/tab_web_contents_delegate_android.h"
#include "chrome/browser/chrome_browser_main_android.h"
#include "chrome/browser/download/android/available_offline_content_provider.h"
#include "chrome/browser/download/android/intercept_oma_download_navigation_throttle.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "chrome/common/chrome_descriptors.h"
#include "components/crash/content/browser/child_exit_observer_android.h"
#include "components/crash/content/browser/crash_memory_metrics_collector_android.h"
#include "components/navigation_interception/intercept_navigation_delegate.h"
#include "content/public/browser/android/java_interfaces.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "ui/base/resource/resource_bundle_android.h"
#include "ui/base/ui_base_paths.h"
#if BUILDFLAG(DFMIFY_DEV_UI)
#include "chrome/browser/dev_ui/android/dev_ui_loader_throttle.h"
#endif  // BUILDFLAG(DFMIFY_DEV_UI)
#elif defined(OS_POSIX)
#include "chrome/browser/chrome_browser_main_posix.h"
#endif

#if !defined(OS_ANDROID)
#include "chrome/browser/badging/badge_manager.h"
#include "chrome/browser/devtools/chrome_devtools_manager_delegate.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/media/unified_autoplay_config.h"
#include "chrome/browser/search/instant_service.h"
#include "chrome/browser/search/instant_service_factory.h"
#include "chrome/browser/serial/chrome_serial_delegate.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/passwords/google_password_manager_navigation_throttle.h"
#include "chrome/browser/ui/search/new_tab_page_navigation_throttle.h"
#include "chrome/browser/webauthn/authenticator_request_scheduler.h"
#include "chrome/browser/webauthn/chrome_authenticator_request_delegate.h"
#include "chrome/common/importer/profile_import.mojom.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#endif  //  !defined(OS_ANDROID)

#if defined(OS_WIN) || defined(OS_MACOSX) || \
    (defined(OS_LINUX) && !defined(OS_CHROMEOS))
#include "chrome/browser/browser_switcher/browser_switcher_navigation_throttle.h"
#include "services/service_manager/sandbox/features.h"
#endif

#if defined(OS_LINUX)
#include "components/crash/core/app/crash_switches.h"
#include "components/crash/core/app/crashpad.h"
#endif

#if defined(OS_POSIX) && !defined(OS_MACOSX)
#if !defined(OS_ANDROID)
#include "base/debug/leak_annotations.h"
#include "components/crash/core/app/breakpad_linux.h"
#endif  // !defined(OS_ANDROID)
#include "components/crash/content/browser/crash_handler_host_linux.h"
#endif

// TODO(crbug.com/939205):  Once the upcoming App Service is available, use a
// single navigation throttle to display the intent picker on all platforms.
#if !defined(OS_ANDROID)
#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/apps/intent_helper/chromeos_apps_navigation_throttle.h"
#include "chrome/browser/chromeos/apps/intent_helper/common_apps_navigation_throttle.h"
#else
#include "chrome/browser/apps/intent_helper/apps_navigation_throttle.h"
#endif
#endif

#if defined(TOOLKIT_VIEWS)
#include "chrome/browser/ui/views/chrome_browser_main_extra_parts_views.h"
#endif

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
#include "chrome/browser/ui/views/chrome_browser_main_extra_parts_views_linux.h"
#endif

#if defined(USE_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#endif

#if defined(USE_X11)
#include "chrome/browser/chrome_browser_main_extra_parts_x11.h"
#endif

#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
#include "components/captive_portal/content/captive_portal_tab_helper.h"
#include "components/captive_portal/content/captive_portal_url_loader_throttle.h"
#endif

#if BUILDFLAG(ENABLE_NACL)
#include "components/nacl/browser/nacl_browser.h"
#include "components/nacl/browser/nacl_host_message_filter.h"
#include "components/nacl/browser/nacl_process_host.h"
#include "components/nacl/common/nacl_process_type.h"
#include "components/nacl/common/nacl_switches.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/accessibility/animation_policy_prefs.h"
#include "chrome/browser/apps/platform_apps/platform_app_navigation_redirector.h"
#include "chrome/browser/extensions/chrome_content_browser_client_extensions_part.h"
#include "chrome/browser/extensions/chrome_extension_web_contents_observer.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/user_script_listener.h"
#include "chrome/browser/media/cast_transport_host_filter.h"
#include "chrome/browser/speech/extension_api/tts_engine_extension_api.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "components/guest_view/browser/guest_view_base.h"
#include "components/guest_view/browser/guest_view_manager.h"
#include "extensions/browser/api/web_request/web_request_api.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_navigation_throttle.h"
#include "extensions/browser/extension_protocols.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#include "extensions/browser/guest_view/web_view/web_view_permission_helper.h"
#include "extensions/browser/guest_view/web_view/web_view_renderer_state.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "extensions/common/manifest_handlers/shared_module_info.h"
#include "extensions/common/manifest_handlers/web_accessible_resources_info.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/permissions/socket_permission.h"
#include "extensions/common/switches.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

#if BUILDFLAG(ENABLE_PLUGINS)
#include "chrome/browser/plugins/chrome_content_browser_client_plugins_part.h"
#include "chrome/browser/plugins/flash_download_interception.h"
#include "chrome/browser/plugins/plugin_response_interceptor_url_loader_throttle.h"
#endif

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
#include "chrome/browser/supervised_user/supervised_user_google_auth_navigation_throttle.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/app_mode/kiosk_settings_navigation_throttle.h"
#include "chrome/browser/chromeos/child_accounts/time_limits/web_time_limit_navigation_throttle.h"
#endif  // defined(OS_CHROMEOS)

#if BUILDFLAG(ENABLE_MEDIA_REMOTING)
#include "chrome/browser/media/cast_remoting_connector.h"
#endif

#if BUILDFLAG(ENABLE_PRINTING)
#include "chrome/browser/printing/printing_message_filter.h"
#endif

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
#include "chrome/browser/media/output_protection_impl.h"
#if defined(OS_WIN) && BUILDFLAG(ENABLE_WIDEVINE)
#include "chrome/browser/media/widevine_hardware_caps_win.h"
#include "third_party/widevine/cdm/widevine_cdm_common.h"
#endif
#endif

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
#include "chrome/browser/supervised_user/supervised_user_navigation_throttle.h"
#endif

#if BUILDFLAG(FULL_SAFE_BROWSING)
#include "chrome/browser/safe_browsing/chrome_password_protection_service.h"
#include "chrome/browser/safe_browsing/client_side_detection_service_factory.h"
#endif

#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
#include "chrome/browser/offline_pages/offline_page_tab_helper.h"
#include "chrome/browser/offline_pages/offline_page_url_loader_request_interceptor.h"
#endif

#if BUILDFLAG(ENABLE_VR) && !defined(OS_ANDROID)
#include "device/vr/public/mojom/isolated_xr_service.mojom.h"
#endif

#if BUILDFLAG(ENABLE_WEBUI_TAB_STRIP)
#include "chrome/browser/ui/webui/tab_strip/chrome_content_browser_client_tab_strip_part.h"
#endif

#if BUILDFLAG(FULL_SAFE_BROWSING)
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_dialog_delegate.h"
#endif

#if BUILDFLAG(ENABLE_VR)
#include "chrome/browser/vr/chrome_xr_integration_client.h"
#endif

using base::FileDescriptor;
using content::BrowserThread;
using content::BrowserURLHandler;
using content::BrowsingDataFilterBuilder;
using content::ChildProcessSecurityPolicy;
using content::QuotaPermissionContext;
using content::RenderFrameHost;
using content::RenderViewHost;
using content::SiteInstance;
using content::WebContents;
using content::WebPreferences;
using message_center::NotifierId;

#if defined(OS_POSIX)
using content::PosixFileDescriptorInfo;
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
using extensions::APIPermission;
using extensions::ChromeContentBrowserClientExtensionsPart;
using extensions::Extension;
using extensions::Manifest;
#endif

#if BUILDFLAG(ENABLE_PLUGINS)
using plugins::ChromeContentBrowserClientPluginsPart;
#endif

namespace {

#if defined(OS_WIN) && !defined(COMPONENT_BUILD) && !defined(ADDRESS_SANITIZER)
// Enables pre-launch Code Integrity Guard (CIG) for Chrome renderers, when
// running on Windows 10 1511 and above. See
// https://blogs.windows.com/blog/tag/code-integrity-guard/.
const base::Feature kRendererCodeIntegrity{"RendererCodeIntegrity",
                                           base::FEATURE_ENABLED_BY_DEFAULT};
#endif  // defined(OS_WIN) && !defined(COMPONENT_BUILD) &&
        // !defined(ADDRESS_SANITIZER)

// Wrapper for SSLErrorHandler::HandleSSLError() that supplies //chrome-level
// parameters.
void HandleSSLErrorWrapper(
    content::WebContents* web_contents,
    int cert_error,
    const net::SSLInfo& ssl_info,
    const GURL& request_url,
    std::unique_ptr<SSLCertReporter> ssl_cert_reporter,
    SSLErrorHandler::BlockingPageReadyCallback blocking_page_ready_callback) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());

  // This can happen if GetBrowserContext no longer exists by the time this
  // gets called (e.g. the SSL error was in a webview that has since been
  // destroyed); if that's the case we don't need to handle the error (and will
  // crash if we attempt to).
  if (!profile)
    return;

  captive_portal::CaptivePortalService* captive_portal_service = nullptr;

#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
  captive_portal_service = CaptivePortalServiceFactory::GetForProfile(profile);
#endif

  SSLErrorHandler::HandleSSLError(
      web_contents, cert_error, ssl_info, request_url,
      std::move(ssl_cert_reporter), std::move(blocking_page_ready_callback),
      g_browser_process->network_time_tracker(), captive_portal_service,
      std::make_unique<ChromeSecurityBlockingPageFactory>(),
      profile->GetPrefs()->GetBoolean(prefs::kSSLErrorOverrideAllowed));
}

enum AppLoadedInTabSource {
  // A platform app page tried to load one of its own URLs in a tab.
  APP_LOADED_IN_TAB_SOURCE_APP = 0,

  // A platform app background page tried to load one of its own URLs in a tab.
  APP_LOADED_IN_TAB_SOURCE_BACKGROUND_PAGE,

  // An extension or app tried to load a resource of a different platform app in
  // a tab.
  APP_LOADED_IN_TAB_SOURCE_OTHER_EXTENSION,

  // A non-app and non-extension page tried to load a platform app in a tab.
  APP_LOADED_IN_TAB_SOURCE_OTHER,

  APP_LOADED_IN_TAB_SOURCE_MAX
};

// Cached version of the locale so we can return the locale on the I/O
// thread.
std::string& GetIOThreadApplicationLocale() {
  static base::NoDestructor<std::string> s;
  return *s;
}

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

// Handles the rewriting of the new tab page URL based on group policy.
bool HandleNewTabPageLocationOverride(
    GURL* url,
    content::BrowserContext* browser_context) {
  if (!url->SchemeIs(content::kChromeUIScheme) ||
      url->host() != chrome::kChromeUINewTabHost)
    return false;

  Profile* profile = Profile::FromBrowserContext(browser_context);
  std::string ntp_location =
      profile->GetPrefs()->GetString(prefs::kNewTabPageLocationOverride);
  if (ntp_location.empty())
    return false;

  *url = GURL(ntp_location);
  return true;
}

#if !defined(OS_ANDROID)
// Check if the current url is whitelisted based on a list of whitelisted urls.
bool IsURLWhitelisted(const GURL& current_url,
                      base::Value::ConstListView whitelisted_urls) {
  // Only check on HTTP and HTTPS pages.
  if (!current_url.SchemeIsHTTPOrHTTPS())
    return false;

  for (auto const& value : whitelisted_urls) {
    ContentSettingsPattern pattern =
        ContentSettingsPattern::FromString(value.GetString());
    if (pattern == ContentSettingsPattern::Wildcard() || !pattern.IsValid())
      continue;
    if (pattern.Matches(current_url))
      return true;
  }

  return false;
}

// Check if autoplay is allowed by policy configuration.
bool IsAutoplayAllowedByPolicy(content::WebContents* contents,
                               PrefService* prefs) {
  DCHECK(prefs);

  // Check if we have globally allowed autoplay by policy.
  if (prefs->GetBoolean(prefs::kAutoplayAllowed) &&
      prefs->IsManagedPreference(prefs::kAutoplayAllowed)) {
    return true;
  }

  if (!contents)
    return false;

  // Check if the current URL matches a URL pattern on the whitelist.
  const base::ListValue* autoplay_whitelist =
      prefs->GetList(prefs::kAutoplayWhitelist);
  return autoplay_whitelist &&
         prefs->IsManagedPreference(prefs::kAutoplayWhitelist) &&
         IsURLWhitelisted(contents->GetURL(), autoplay_whitelist->GetList());
}
#endif

#if defined(OS_ANDROID)
int GetCrashSignalFD(const base::CommandLine& command_line) {
  return crashpad::CrashHandlerHost::Get()->GetDeathSignalSocket();
}
#elif defined(OS_POSIX) && !defined(OS_MACOSX)
breakpad::CrashHandlerHostLinux* CreateCrashHandlerHost(
    const std::string& process_type) {
  base::FilePath dumps_path;
  base::PathService::Get(chrome::DIR_CRASH_DUMPS, &dumps_path);
  {
    ANNOTATE_SCOPED_MEMORY_LEAK;
    bool upload = !getenv(env_vars::kHeadless);
    breakpad::CrashHandlerHostLinux* crash_handler =
        new breakpad::CrashHandlerHostLinux(process_type, dumps_path, upload);
    crash_handler->StartUploaderThread();
    return crash_handler;
  }
}

int GetCrashSignalFD(const base::CommandLine& command_line) {
  if (crash_reporter::IsCrashpadEnabled()) {
    int fd;
    pid_t pid;
    return crash_reporter::GetHandlerSocket(&fd, &pid) ? fd : -1;
  }

  // Extensions have the same process type as renderers.
  if (command_line.HasSwitch(extensions::switches::kExtensionProcess)) {
    static breakpad::CrashHandlerHostLinux* crash_handler = nullptr;
    if (!crash_handler)
      crash_handler = CreateCrashHandlerHost("extension");
    return crash_handler->GetDeathSignalSocket();
  }

  std::string process_type =
      command_line.GetSwitchValueASCII(switches::kProcessType);

  if (process_type == switches::kRendererProcess) {
    static breakpad::CrashHandlerHostLinux* crash_handler = nullptr;
    if (!crash_handler)
      crash_handler = CreateCrashHandlerHost(process_type);
    return crash_handler->GetDeathSignalSocket();
  }

  if (process_type == switches::kPpapiPluginProcess) {
    static breakpad::CrashHandlerHostLinux* crash_handler = nullptr;
    if (!crash_handler)
      crash_handler = CreateCrashHandlerHost(process_type);
    return crash_handler->GetDeathSignalSocket();
  }

  if (process_type == switches::kGpuProcess) {
    static breakpad::CrashHandlerHostLinux* crash_handler = nullptr;
    if (!crash_handler)
      crash_handler = CreateCrashHandlerHost(process_type);
    return crash_handler->GetDeathSignalSocket();
  }

  if (process_type == switches::kUtilityProcess) {
    static breakpad::CrashHandlerHostLinux* crash_handler = nullptr;
    if (!crash_handler)
      crash_handler = CreateCrashHandlerHost(process_type);
    return crash_handler->GetDeathSignalSocket();
  }

  return -1;
}
#endif  // defined(OS_ANDROID)

void SetApplicationLocaleOnIOThread(const std::string& locale) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  GetIOThreadApplicationLocale() = locale;
}

// An implementation of the SSLCertReporter interface used by
// SSLErrorHandler. Uses CertificateReportingService to send reports. The
// service handles queueing and re-sending of failed reports. Each certificate
// error creates a new instance of this class.
class CertificateReportingServiceCertReporter : public SSLCertReporter {
 public:
  explicit CertificateReportingServiceCertReporter(
      content::WebContents* web_contents)
      : service_(CertificateReportingServiceFactory::GetForBrowserContext(
            web_contents->GetBrowserContext())) {}
  ~CertificateReportingServiceCertReporter() override {}

  // SSLCertReporter implementation
  void ReportInvalidCertificateChain(
      const std::string& serialized_report) override {
    service_->Send(serialized_report);
  }

 private:
  CertificateReportingService* service_;

  DISALLOW_COPY_AND_ASSIGN(CertificateReportingServiceCertReporter);
};

#if BUILDFLAG(ENABLE_EXTENSIONS)

AppLoadedInTabSource ClassifyAppLoadedInTabSource(
    const GURL& opener_url,
    const extensions::Extension* target_platform_app) {
  if (!opener_url.SchemeIs(extensions::kExtensionScheme)) {
    // The forbidden app URL was being opened by a non-extension page (e.g.
    // http).
    return APP_LOADED_IN_TAB_SOURCE_OTHER;
  }

  if (opener_url.host_piece() != target_platform_app->id()) {
    // The forbidden app URL was being opened by a different app or extension.
    return APP_LOADED_IN_TAB_SOURCE_OTHER_EXTENSION;
  }

  // This platform app was trying to window.open() one of its own URLs.
  if (opener_url ==
      extensions::BackgroundInfo::GetBackgroundURL(target_platform_app)) {
    // Source was the background page.
    return APP_LOADED_IN_TAB_SOURCE_BACKGROUND_PAGE;
  }

  // Source was a different page inside the app.
  return APP_LOADED_IN_TAB_SOURCE_APP;
}

// Returns true if there is is an extension matching |url| in
// |opener_render_process_id| with APIPermission::kBackground.
//
// Note that GetExtensionOrAppByURL requires a full URL in order to match with a
// hosted app, even though normal extensions just use the host.
bool URLHasExtensionBackgroundPermission(
    extensions::ProcessMap* process_map,
    extensions::ExtensionRegistry* registry,
    const GURL& url,
    int opener_render_process_id) {
  // Note: includes web URLs that are part of an extension's web extent.
  const Extension* extension =
      registry->enabled_extensions().GetExtensionOrAppByURL(url);
  return extension &&
         extension->permissions_data()->HasAPIPermission(
             APIPermission::kBackground) &&
         process_map->Contains(extension->id(), opener_render_process_id);
}

#endif

mojo::PendingRemote<chrome::mojom::PrerenderCanceler> GetPrerenderCanceler(
    const base::Callback<content::WebContents*()>& wc_getter) {
  mojo::PendingRemote<chrome::mojom::PrerenderCanceler> canceler;
  prerender::PrerenderContents::FromWebContents(wc_getter.Run())
      ->AddPrerenderCancelerReceiver(canceler.InitWithNewPipeAndPassReceiver());
  return canceler;
}

// Encapculates logic to determine if enterprise policies should be honored.
// This is a copy of the code in policy_loader_win.cc but it's ok to duplicate
// as a new central class to replace those checks is in the making.
bool ShouldHonorPolicies() {
#if defined(OS_WIN)
  bool is_enterprise_version =
      base::win::OSInfo::GetInstance()->version_type() != base::win::SUITE_HOME;
  return base::win::IsEnrolledToDomain() ||
         (base::win::IsDeviceRegisteredWithManagement() &&
          is_enterprise_version);
#else   // defined(OS_WIN)
  // TODO(pastarmovj): Replace this with check for MacOS and the new management
  // service once it is ready.
  return true;
#endif  // defined(OS_WIN)
}

void LaunchURL(const GURL& url,
               content::WebContents::OnceGetter web_contents_getter,
               ui::PageTransition page_transition,
               bool has_user_gesture,
               const base::Optional<url::Origin>& initiating_origin) {
  // If there is no longer a WebContents, the request may have raced with tab
  // closing. Don't fire the external request. (It may have been a prerender.)
  content::WebContents* web_contents = std::move(web_contents_getter).Run();
  if (!web_contents)
    return;

  // Do not launch external requests attached to unswapped prerenders.
  prerender::PrerenderContents* prerender_contents =
      prerender::PrerenderContents::FromWebContents(web_contents);
  if (prerender_contents) {
    prerender_contents->Destroy(prerender::FINAL_STATUS_UNSUPPORTED_SCHEME);
    return;
  }

  // Do not launch external requests for schemes that have a handler registered.
  ProtocolHandlerRegistry* protocol_handler_registry =
      ProtocolHandlerRegistryFactory::GetForBrowserContext(
          web_contents->GetBrowserContext());
  if (protocol_handler_registry &&
      protocol_handler_registry->IsHandledProtocol(url.scheme()))
    return;

  bool is_whitelisted = false;
  PolicyBlacklistService* service =
      PolicyBlacklistFactory::GetForBrowserContext(
          web_contents->GetBrowserContext());
  if (ShouldHonorPolicies() && service) {
    const policy::URLBlacklist::URLBlacklistState url_state =
        service->GetURLBlacklistState(url);
    is_whitelisted =
        url_state == policy::URLBlacklist::URLBlacklistState::URL_IN_WHITELIST;
  }

  // If the URL is in whitelist, we launch it without asking the user and
  // without any additional security checks. Since the URL is whitelisted,
  // we assume it can be executed.
  if (is_whitelisted) {
    ExternalProtocolHandler::LaunchUrlWithoutSecurityCheck(url, web_contents);
  } else {
    ExternalProtocolHandler::LaunchUrl(
        url, web_contents->GetRenderViewHost()->GetProcess()->GetID(),
        web_contents->GetRenderViewHost()->GetRoutingID(), page_transition,
        has_user_gesture, initiating_origin);
  }
}

std::string GetProduct() {
  return version_info::GetProductNameAndVersionForUserAgent();
}

void MaybeAppendSecureOriginsAllowlistSwitch(base::CommandLine* cmdline) {
  // |allowlist| combines pref/policy + cmdline switch in the browser process.
  // For renderer and utility (e.g. NetworkService) processes the switch is the
  // only available source, so below the combined (pref/policy + cmdline)
  // allowlist of secure origins is injected into |cmdline| for these other
  // processes.
  std::vector<std::string> allowlist =
      network::SecureOriginAllowlist::GetInstance().GetCurrentAllowlist();
  if (!allowlist.empty()) {
    cmdline->AppendSwitchASCII(
        network::switches::kUnsafelyTreatInsecureOriginAsSecure,
        base::JoinString(allowlist, ","));
  }
}

#if defined(OS_WIN) && !defined(COMPONENT_BUILD) && !defined(ADDRESS_SANITIZER)
// Returns the full path to |module_name|. Both dev builds (where |module_name|
// is in the current executable's directory) and proper installs (where
// |module_name| is in a versioned sub-directory of the current executable's
// directory) are supported. The identified file is not guaranteed to exist.
base::FilePath GetModulePath(base::StringPiece16 module_name) {
  base::FilePath exe_dir;
  const bool has_path = base::PathService::Get(base::DIR_EXE, &exe_dir);
  DCHECK(has_path);

  // Look for the module in a versioned sub-directory of the current
  // executable's directory and return the path if it can be read. This is the
  // expected location of modules for proper installs.
  const base::FilePath module_path =
      exe_dir.AppendASCII(chrome::kChromeVersion).Append(module_name);
  if (base::PathExists(module_path))
    return module_path;

  // Otherwise, return the path to the module in the current executable's
  // directory. This is the expected location of modules for dev builds.
  return exe_dir.Append(module_name);
}
#endif  // defined(OS_WIN) && !defined(COMPONENT_BUILD) &&
        // !defined(ADDRESS_SANITIZER)

void MaybeAddThrottle(
    std::unique_ptr<content::NavigationThrottle> maybe_throttle,
    std::vector<std::unique_ptr<content::NavigationThrottle>>* throttles) {
  if (maybe_throttle)
    throttles->push_back(std::move(maybe_throttle));
}

void MaybeAddThrottles(
    std::vector<std::unique_ptr<content::NavigationThrottle>> additional,
    std::vector<std::unique_ptr<content::NavigationThrottle>>* combined) {
  combined->insert(combined->end(), std::make_move_iterator(additional.begin()),
                   std::make_move_iterator(additional.end()));
}

// Returns whether |web_contents| is within a hosted app.
bool IsInHostedApp(WebContents* web_contents) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  return (browser &&
          web_app::AppBrowserController::IsForWebAppBrowser(browser));
#else
  return false;
#endif
}

void MaybeRecordSameSiteCookieEngagementHistogram(
    content::RenderFrameHost* render_frame_host,
    blink::mojom::WebFeature feature) {
  if (feature != blink::mojom::WebFeature::kCookieNoSameSite &&
      feature != blink::mojom::WebFeature::kCookieInsecureAndSameSiteNone) {
    return;
  }

  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(render_frame_host);

  content::BrowserContext* browser_context =
      WebContents::FromRenderFrameHost(render_frame_host)->GetBrowserContext();
  Profile* profile = Profile::FromBrowserContext(browser_context);
  SiteEngagementService* site_engagement_service =
      SiteEngagementService::Get(profile);
  if (!site_engagement_service)
    return;

  blink::mojom::EngagementLevel engagement_level =
      site_engagement_service->GetEngagementLevel(
          render_frame_host->GetLastCommittedURL());
  if (feature == blink::mojom::WebFeature::kCookieNoSameSite) {
    UMA_HISTOGRAM_ENUMERATION(
        "Net.SameSiteBlockedCookieSiteEngagement.CookieNoSameSite",
        engagement_level);
  } else {
    UMA_HISTOGRAM_ENUMERATION(
        "Net.SameSiteBlockedCookieSiteEngagement."
        "CookieInsecureAndSameSiteNone",
        engagement_level);
  }
}

}  // namespace

// Generate a pseudo-random permutation of the following brand/version pairs:
//   1. The base project (i.e. Chromium)
//   2. The browser brand, if available
//   3. A randomized string containing escaped characters to ensure proper
//      header parsing, along with an arbitrarily low version to ensure proper
//      version checking.
blink::UserAgentBrandList GenerateBrandVersionList(
    int seed,
    base::Optional<std::string> brand,
    std::string major_version) {
  DCHECK_GE(seed, 0);
  const int npermutations = 6;  // 3!
  int permutation = seed % npermutations;

  // Pick a stable permutation seeded by major version number. any values here
  // and in order should be under three.
  const std::vector<std::vector<int>> orders{{0, 1, 2}, {0, 2, 1}, {1, 0, 2},
                                             {1, 2, 0}, {2, 0, 1}, {2, 1, 0}};
  const std::vector<int> order = orders[permutation];
  DCHECK_EQ(6u, orders.size());
  DCHECK_EQ(3u, order.size());

  const std::vector<std::string> escaped_chars = {"\\", "\"", ";"};
  std::string greasey_brand =
      base::StrCat({escaped_chars[order[0]], "Not", escaped_chars[order[1]],
                    "A", escaped_chars[order[2]], "Brand"});

  blink::UserAgentBrandVersion greasey_bv = {greasey_brand, "99"};
  blink::UserAgentBrandVersion chromium_bv = {"Chromium", major_version};

  blink::UserAgentBrandList greased_brand_version_list(3);

  if (brand) {
    blink::UserAgentBrandVersion brand_bv = {brand.value(), major_version};

    greased_brand_version_list[order[0]] = greasey_bv;
    greased_brand_version_list[order[1]] = chromium_bv;
    greased_brand_version_list[order[2]] = brand_bv;
  } else {
    greased_brand_version_list[seed % 2] = greasey_bv;
    greased_brand_version_list[(seed + 1) % 2] = chromium_bv;

    // If left, the last element would make a blank "" at the end of the header.
    greased_brand_version_list.pop_back();
  }

  return greased_brand_version_list;
}

const blink::UserAgentBrandList& GetBrandVersionList() {
  static const base::NoDestructor<blink::UserAgentBrandList>
      greased_brand_version_list([] {
        int major_version_number;
        std::string major_version = version_info::GetMajorVersionNumber();
        base::StringToInt(major_version, &major_version_number);
        base::Optional<std::string> brand;
#if !BUILDFLAG(CHROMIUM_BRANDING)
        brand = version_info::GetProductName();
#endif
        return GenerateBrandVersionList(major_version_number, brand,
                                        major_version);
      }());
  return *greased_brand_version_list;
}

std::string GetUserAgent() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kUserAgent)) {
    std::string ua = command_line->GetSwitchValueASCII(switches::kUserAgent);
    if (net::HttpUtil::IsValidHeaderValue(ua))
      return ua;
    LOG(WARNING) << "Ignored invalid value for flag --" << switches::kUserAgent;
  }

  if (base::FeatureList::IsEnabled(blink::features::kFreezeUserAgent)) {
    return content::GetFrozenUserAgent(
        command_line->HasSwitch(switches::kUseMobileUserAgent),
        version_info::GetMajorVersionNumber());
  }

  std::string product = GetProduct();
#if defined(OS_ANDROID)
  if (command_line->HasSwitch(switches::kUseMobileUserAgent))
    product += " Mobile";
#endif
  return content::BuildUserAgentFromProduct(product);
}

blink::UserAgentMetadata GetUserAgentMetadata() {
  blink::UserAgentMetadata metadata;

  metadata.brand_version_list = GetBrandVersionList();
  metadata.full_version = version_info::GetVersionNumber();
  metadata.platform = version_info::GetOSType();
  metadata.platform_version =
      content::GetOSVersion(content::IncludeAndroidBuildNumber::Exclude,
                            content::IncludeAndroidModel::Exclude);
  metadata.architecture = content::GetLowEntropyCpuArchitecture();
  metadata.model = content::BuildModelInfo();

  metadata.mobile = false;
#if defined(OS_ANDROID)
  metadata.mobile = base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kUseMobileUserAgent);
#endif

  return metadata;
}

ChromeContentBrowserClient::ChromeContentBrowserClient(
    StartupData* startup_data)
    : startup_data_(startup_data) {
#if BUILDFLAG(ENABLE_PLUGINS)
  extra_parts_.push_back(new ChromeContentBrowserClientPluginsPart);
#endif

#if defined(OS_CHROMEOS)
  extra_parts_.push_back(new ChromeContentBrowserClientChromeOsPart);
#endif  // defined(OS_CHROMEOS)

#if BUILDFLAG(ENABLE_WEBUI_TAB_STRIP)
  extra_parts_.push_back(new ChromeContentBrowserClientTabStripPart);
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
  extra_parts_.push_back(new ChromeContentBrowserClientExtensionsPart);
#endif

  extra_parts_.push_back(new ChromeContentBrowserClientPerformanceManagerPart);
}

ChromeContentBrowserClient::~ChromeContentBrowserClient() {
  for (int i = static_cast<int>(extra_parts_.size()) - 1; i >= 0; --i)
    delete extra_parts_[i];
  extra_parts_.clear();
}

// static
void ChromeContentBrowserClient::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterFilePathPref(prefs::kDiskCacheDir, base::FilePath());
  registry->RegisterIntegerPref(prefs::kDiskCacheSize, 0);
  registry->RegisterStringPref(prefs::kIsolateOrigins, std::string());
  registry->RegisterBooleanPref(prefs::kSitePerProcess, false);
  registry->RegisterBooleanPref(prefs::kTabFreezingEnabled, true);
}

// static
void ChromeContentBrowserClient::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(prefs::kDisable3DAPIs, false);
  registry->RegisterBooleanPref(prefs::kEnableHyperlinkAuditing, true);
  registry->RegisterListPref(prefs::kEnableDeprecatedWebPlatformFeatures);
  // Register user prefs for mapping SitePerProcess and IsolateOrigins in
  // user policy in addition to the same named ones in Local State (which are
  // used for mapping the command-line flags).
  registry->RegisterStringPref(prefs::kIsolateOrigins, std::string());
  registry->RegisterBooleanPref(prefs::kSitePerProcess, false);
  registry->RegisterListPref(prefs::kUserTriggeredIsolatedOrigins);
  registry->RegisterDictionaryPref(
      prefs::kDevToolsBackgroundServicesExpirationDict);
  registry->RegisterBooleanPref(prefs::kSignedHTTPExchangeEnabled, true);
#if !defined(OS_ANDROID)
  registry->RegisterBooleanPref(prefs::kAutoplayAllowed, false);
  registry->RegisterListPref(prefs::kAutoplayWhitelist);
#endif
  registry->RegisterListPref(prefs::kCorsMitigationList);
  registry->RegisterBooleanPref(prefs::kCorsLegacyModeEnabled, false);
}

// static
void ChromeContentBrowserClient::SetApplicationLocale(
    const std::string& locale) {
  // The common case is that this function is called early in Chrome startup
  // before any threads are created or registered. When there are no threads,
  // we can just set the string without worrying about threadsafety.
  if (!BrowserThread::IsThreadInitialized(BrowserThread::IO)) {
    GetIOThreadApplicationLocale() = locale;
    return;
  }

  // Otherwise we're being called to change the locale. In this case set it on
  // the IO thread.
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  base::PostTask(FROM_HERE, {BrowserThread::IO},
                 base::BindOnce(&SetApplicationLocaleOnIOThread, locale));
}

std::unique_ptr<content::BrowserMainParts>
ChromeContentBrowserClient::CreateBrowserMainParts(
    const content::MainFunctionParams& parameters) {
  std::unique_ptr<ChromeBrowserMainParts> main_parts;
  // Construct the Main browser parts based on the OS type.
#if defined(OS_WIN)
  main_parts =
      std::make_unique<ChromeBrowserMainPartsWin>(parameters, startup_data_);
#elif defined(OS_MACOSX)
  main_parts =
      std::make_unique<ChromeBrowserMainPartsMac>(parameters, startup_data_);
#elif defined(OS_CHROMEOS)
  main_parts = std::make_unique<chromeos::ChromeBrowserMainPartsChromeos>(
      parameters, startup_data_);
#elif defined(OS_LINUX)
  main_parts =
      std::make_unique<ChromeBrowserMainPartsLinux>(parameters, startup_data_);
#elif defined(OS_ANDROID)
  main_parts = std::make_unique<ChromeBrowserMainPartsAndroid>(parameters,
                                                               startup_data_);
#elif defined(OS_POSIX)
  main_parts =
      std::make_unique<ChromeBrowserMainPartsPosix>(parameters, startup_data_);
#else
  NOTREACHED();
  main_parts =
      std::make_unique<ChromeBrowserMainParts>(parameters, startup_data_);
#endif

  bool add_profiles_extra_parts = true;
#if defined(OS_ANDROID)
  if (startup_data_->HasBuiltProfilePrefService())
    add_profiles_extra_parts = false;
#endif
  if (add_profiles_extra_parts)
    chrome::AddProfilesExtraParts(main_parts.get());

    // Construct additional browser parts. Stages are called in the order in
    // which they are added.
#if defined(TOOLKIT_VIEWS)
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  main_parts->AddParts(new ChromeBrowserMainExtraPartsViewsLinux());
#else
  main_parts->AddParts(new ChromeBrowserMainExtraPartsViews());
#endif
#endif

#if defined(OS_CHROMEOS)
  // TODO(jamescook): Combine with ChromeBrowserMainPartsChromeos.
  main_parts->AddParts(new ChromeBrowserMainExtraPartsAsh());
#endif

#if defined(USE_X11)
  main_parts->AddParts(new ChromeBrowserMainExtraPartsX11());
#endif

  main_parts->AddParts(new ChromeBrowserMainExtraPartsPerformanceManager);

  main_parts->AddParts(new ChromeBrowserMainExtraPartsProfiling);

  main_parts->AddParts(new ChromeBrowserMainExtraPartsMemory);

  chrome::AddMetricsExtraParts(main_parts.get());

  // Always add ChromeBrowserMainExtraPartsGpu last to make sure
  // GpuDataManager initialization could pick up about:flags settings.
  main_parts->AddParts(new ChromeBrowserMainExtraPartsGpu);

  return main_parts;
}

void ChromeContentBrowserClient::PostAfterStartupTask(
    const base::Location& from_here,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    base::OnceClosure task) {
  AfterStartupTaskUtils::PostTask(from_here, task_runner, std::move(task));

  InitNetworkContextsParentDirectory();

  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  safe_browsing_service_ = g_browser_process->safe_browsing_service();
}

bool ChromeContentBrowserClient::IsBrowserStartupComplete() {
  return AfterStartupTaskUtils::IsBrowserStartupComplete();
}

void ChromeContentBrowserClient::SetBrowserStartupIsCompleteForTesting() {
  AfterStartupTaskUtils::SetBrowserStartupIsCompleteForTesting();
}

bool ChromeContentBrowserClient::IsShuttingDown() {
  return browser_shutdown::HasShutdownStarted();
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
  return CreateWebContentsViewDelegate(web_contents);
}

bool ChromeContentBrowserClient::AllowGpuLaunchRetryOnIOThread() {
#if defined(OS_ANDROID)
  const base::android::ApplicationState app_state =
      base::android::ApplicationStatusListener::GetState();
  return base::android::APPLICATION_STATE_UNKNOWN == app_state ||
         base::android::APPLICATION_STATE_HAS_RUNNING_ACTIVITIES == app_state ||
         base::android::APPLICATION_STATE_HAS_PAUSED_ACTIVITIES == app_state;
#else
  return true;
#endif
}

void ChromeContentBrowserClient::RenderProcessWillLaunch(
    content::RenderProcessHost* host) {
  int id = host->GetID();
  Profile* profile = Profile::FromBrowserContext(host->GetBrowserContext());
#if BUILDFLAG(ENABLE_EXTENSIONS)
  host->AddFilter(new cast::CastTransportHostFilter());
#endif
#if BUILDFLAG(ENABLE_PRINTING)
  host->AddFilter(new printing::PrintingMessageFilter(id, profile));
#endif
  host->AddFilter(new prerender::PrerenderMessageFilter(id));

  WebRtcLoggingController::AttachToRenderProcessHost(
      host, g_browser_process->webrtc_log_uploader());

  // The audio manager outlives the host, so it's safe to hand a raw pointer to
  // it to the AudioDebugRecordingsHandler, which is owned by the host.
  AudioDebugRecordingsHandler* audio_debug_recordings_handler =
      new AudioDebugRecordingsHandler(profile);
  host->SetUserData(
      AudioDebugRecordingsHandler::kAudioDebugRecordingsHandlerKey,
      std::make_unique<base::UserDataAdapter<AudioDebugRecordingsHandler>>(
          audio_debug_recordings_handler));

#if BUILDFLAG(ENABLE_NACL)
  host->AddFilter(new nacl::NaClHostMessageFilter(id, profile->IsOffTheRecord(),
                                                  profile->GetPath()));
#endif

#if defined(OS_ANDROID)
  // Data cannot be persisted if the profile is off the record.
  host->AddFilter(
      new cdm::CdmMessageFilterAndroid(!profile->IsOffTheRecord(), false));

  // Register CrashMemoryMetricsCollector to report oom related metrics.
  host->SetUserData(
      CrashMemoryMetricsCollector::kCrashMemoryMetricsCollectorKey,
      std::make_unique<CrashMemoryMetricsCollector>(host));
#endif

  Profile* original_profile = profile->GetOriginalProfile();
  RendererUpdaterFactory::GetForProfile(original_profile)
      ->InitializeRenderer(host);

  for (size_t i = 0; i < extra_parts_.size(); ++i)
    extra_parts_[i]->RenderProcessWillLaunch(host);
}

GURL ChromeContentBrowserClient::GetEffectiveURL(
    content::BrowserContext* browser_context,
    const GURL& url) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  if (!profile)
    return url;

#if !defined(OS_ANDROID)
  // If the input |url| should be assigned to the Instant renderer, make its
  // effective URL distinct from other URLs on the search provider's domain.
  // This needs to happen even if |url| corresponds to an isolated origin; see
  // https://crbug.com/755595.
  if (search::ShouldAssignURLToInstantRenderer(url, profile))
    return search::GetEffectiveURLForInstant(url, profile);
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
  return ChromeContentBrowserClientExtensionsPart::GetEffectiveURL(profile,
                                                                   url);
#else
  return url;
#endif
}

bool ChromeContentBrowserClient::
    ShouldCompareEffectiveURLsForSiteInstanceSelection(
        content::BrowserContext* browser_context,
        content::SiteInstance* candidate_site_instance,
        bool is_main_frame,
        const GURL& candidate_url,
        const GURL& destination_url) {
  DCHECK(browser_context);
  DCHECK(candidate_site_instance);
#if BUILDFLAG(ENABLE_EXTENSIONS)
  return ChromeContentBrowserClientExtensionsPart::
      ShouldCompareEffectiveURLsForSiteInstanceSelection(
          browser_context, candidate_site_instance, is_main_frame,
          candidate_url, destination_url);
#else
  return true;
#endif
}

bool ChromeContentBrowserClient::ShouldUseMobileFlingCurve() {
#if defined(OS_ANDROID)
  return true;
#elif defined(OS_CHROMEOS)
  return ash::TabletMode::Get() && ash::TabletMode::Get()->InTabletMode();
#else
  return false;
#endif  // defined(OS_ANDROID)
}

bool ChromeContentBrowserClient::ShouldUseProcessPerSite(
    content::BrowserContext* browser_context,
    const GURL& site_url) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  if (!profile)
    return false;

  // NTP should use process-per-site.  This is a performance optimization to
  // reduce process count associated with NTP tabs.
  if (site_url == GURL(chrome::kChromeUINewTabURL))
    return true;

  // The web footer experiment should share its renderer to not effectively
  // instantiate one per window. See https://crbug.com/993502.
  if (site_url == GURL(chrome::kChromeUIWebFooterExperimentURL))
    return true;

#if !defined(OS_ANDROID)
  if (search::ShouldUseProcessPerSiteForInstantSiteURL(site_url, profile))
    return true;
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (ChromeContentBrowserClientExtensionsPart::ShouldUseProcessPerSite(
          profile, site_url))
    return true;
#endif

  // Non-extension, non-NTP URLs should generally use process-per-site-instance
  // (rather than process-per-site).
  return false;
}

bool ChromeContentBrowserClient::ShouldUseSpareRenderProcessHost(
    content::BrowserContext* browser_context,
    const GURL& site_url) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  if (!profile)
    return false;

#if !defined(OS_ANDROID)
  // Instant renderers should not use a spare process, because they require
  // passing switches::kInstantProcess to the renderer process when it
  // launches.  A spare process is launched earlier, before it is known which
  // navigation will use it, so it lacks this flag.
  if (search::ShouldAssignURLToInstantRenderer(site_url, profile))
    return false;
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
  return ChromeContentBrowserClientExtensionsPart::
      ShouldUseSpareRenderProcessHost(profile, site_url);
#else
  return true;
#endif
}

bool ChromeContentBrowserClient::DoesSiteRequireDedicatedProcess(
    content::BrowserContext* browser_context,
    const GURL& effective_site_url) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
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
  if (!ChromeContentBrowserClientExtensionsPart::ShouldLockToOrigin(
          browser_context, effective_site_url)) {
    return false;
  }
#endif
  return true;
}

bool ChromeContentBrowserClient::DoesWebUISchemeRequireProcessLock(
    base::StringPiece scheme) {
  // Note: This method can be called from multiple threads. It is not safe to
  // assume it runs only on the UI thread.

  // chrome-search: documents commit only in the NTP instant process and are not
  // locked to chrome-search: origin.
  if (scheme == chrome::kChromeSearchScheme)
    return false;

  // All other WebUIs must be locked to origin.
  return true;
}

bool ChromeContentBrowserClient::ShouldTreatURLSchemeAsFirstPartyWhenTopLevel(
    base::StringPiece scheme,
    bool is_embedded_origin_secure) {
  if (is_embedded_origin_secure && scheme == content::kChromeUIScheme)
    return true;
#if BUILDFLAG(ENABLE_EXTENSIONS)
  return scheme == extensions::kExtensionScheme;
#else
  return false;
#endif
}

bool ChromeContentBrowserClient::
    ShouldIgnoreSameSiteCookieRestrictionsWhenTopLevel(
        base::StringPiece scheme,
        bool is_embedded_origin_secure) {
  return is_embedded_origin_secure && scheme == content::kChromeUIScheme;
}

void ChromeContentBrowserClient::OverrideURLLoaderFactoryParams(
    content::BrowserContext* browser_context,
    const url::Origin& origin,
    bool is_for_isolated_world,
    network::mojom::URLLoaderFactoryParams* factory_params) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  ChromeContentBrowserClientExtensionsPart::OverrideURLLoaderFactoryParams(
      browser_context, origin, is_for_isolated_world, factory_params);
#endif
}

// These are treated as WebUI schemes but do not get WebUI bindings. Also,
// view-source is allowed for these schemes.
void ChromeContentBrowserClient::GetAdditionalWebUISchemes(
    std::vector<std::string>* additional_schemes) {
  additional_schemes->push_back(chrome::kChromeSearchScheme);
  additional_schemes->push_back(dom_distiller::kDomDistillerScheme);
  additional_schemes->push_back(content::kChromeDevToolsScheme);
}

void ChromeContentBrowserClient::GetAdditionalViewSourceSchemes(
    std::vector<std::string>* additional_schemes) {
  GetAdditionalWebUISchemes(additional_schemes);

#if BUILDFLAG(ENABLE_EXTENSIONS)
  additional_schemes->push_back(extensions::kExtensionScheme);
#endif
}

bool ChromeContentBrowserClient::LogWebUIUrl(const GURL& web_ui_url) {
  return webui::LogWebUIUrl(web_ui_url);
}

bool ChromeContentBrowserClient::IsWebUIAllowedToMakeNetworkRequests(
    const url::Origin& origin) {
  return ChromeWebUIControllerFactory::IsWebUIAllowedToMakeNetworkRequests(
      origin);
}

bool ChromeContentBrowserClient::IsHandledURL(const GURL& url) {
  return ProfileIOData::IsHandledURL(url);
}

bool ChromeContentBrowserClient::CanCommitURL(
    content::RenderProcessHost* process_host,
    const GURL& url) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  return ChromeContentBrowserClientExtensionsPart::CanCommitURL(process_host,
                                                                url);
#else
  return true;
#endif
}

void ChromeContentBrowserClient::OverrideNavigationParams(
    SiteInstance* site_instance,
    ui::PageTransition* transition,
    bool* is_renderer_initiated,
    content::Referrer* referrer,
    base::Optional<url::Origin>* initiator_origin) {
  DCHECK(transition);
  DCHECK(is_renderer_initiated);
  DCHECK(referrer);
  // While using SiteInstance::GetSiteURL() is unreliable and the wrong thing to
  // use for making security decisions 99.44% of the time, for detecting the NTP
  // it is reliable and the correct way. See http://crbug.com/624410.
  if (site_instance && search::IsNTPURL(site_instance->GetSiteURL()) &&
      ui::PageTransitionCoreTypeIs(*transition, ui::PAGE_TRANSITION_LINK)) {
    // Clicks on tiles of the new tab page should be treated as if a user
    // clicked on a bookmark.  This is consistent with native implementations
    // like Android's.  This also helps ensure that security features (like
    // Sec-Fetch-Site and SameSite-cookies) will treat the navigation as
    // browser-initiated.
    *transition = ui::PAGE_TRANSITION_AUTO_BOOKMARK;
    *is_renderer_initiated = false;
    *referrer = content::Referrer();
    *initiator_origin = base::nullopt;
  }
}

bool ChromeContentBrowserClient::ShouldStayInParentProcessForNTP(
    const GURL& url,
    SiteInstance* parent_site_instance) {
  // While using SiteInstance::GetSiteURL() is unreliable and the wrong thing to
  // use for making security decisions 99.44% of the time, for detecting the NTP
  // it is reliable and the correct way. See http://crbug.com/624410.
  return url.SchemeIs(chrome::kChromeSearchScheme) && parent_site_instance &&
         search::IsNTPURL(parent_site_instance->GetSiteURL());
}

bool ChromeContentBrowserClient::IsSuitableHost(
    content::RenderProcessHost* process_host,
    const GURL& site_url) {
  Profile* profile =
      Profile::FromBrowserContext(process_host->GetBrowserContext());
  // This may be nullptr during tests. In that case, just assume any site can
  // share any host.
  if (!profile)
    return true;

#if !defined(OS_ANDROID)
  // Instant URLs should only be in the instant process and instant process
  // should only have Instant URLs.
  InstantService* instant_service =
      InstantServiceFactory::GetForProfile(profile);
  if (instant_service) {
    bool is_instant_process =
        instant_service->IsInstantProcess(process_host->GetID());
    bool should_be_in_instant_process =
        search::ShouldAssignURLToInstantRenderer(site_url, profile);
    if (is_instant_process || should_be_in_instant_process)
      return is_instant_process && should_be_in_instant_process;
  }
#endif

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
    content::BrowserContext* browser_context,
    const GURL& url) {
  // It has to be a valid URL for us to check for an extension.
  if (!url.is_valid())
    return false;

#if BUILDFLAG(ENABLE_EXTENSIONS)
  Profile* profile = Profile::FromBrowserContext(browser_context);
  return ChromeContentBrowserClientExtensionsPart::
      ShouldTryToUseExistingProcessHost(profile, url);
#else
  return false;
#endif
}

bool ChromeContentBrowserClient::ShouldSubframesTryToReuseExistingProcess(
    content::RenderFrameHost* main_frame) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  return ChromeContentBrowserClientExtensionsPart::
      ShouldSubframesTryToReuseExistingProcess(main_frame);
#else
  return true;
#endif
}

void ChromeContentBrowserClient::SiteInstanceGotProcess(
    SiteInstance* site_instance) {
  CHECK(site_instance->HasProcess());

  Profile* profile =
      Profile::FromBrowserContext(site_instance->GetBrowserContext());
  if (!profile)
    return;

#if !defined(OS_ANDROID)
  // Remember the ID of the Instant process to signal the renderer process
  // on startup in |AppendExtraCommandLineSwitches| below.
  if (search::ShouldAssignURLToInstantRenderer(site_instance->GetSiteURL(),
                                               profile)) {
    InstantService* instant_service =
        InstantServiceFactory::GetForProfile(profile);
    if (instant_service)
      instant_service->AddInstantProcess(site_instance->GetProcess()->GetID());
  }
#endif

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
    const GURL& current_effective_url,
    const GURL& destination_effective_url) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  return ChromeContentBrowserClientExtensionsPart::
      ShouldSwapBrowsingInstancesForNavigation(
          site_instance, current_effective_url, destination_effective_url);
#else
  return false;
#endif
}

bool ChromeContentBrowserClient::ShouldIsolateErrorPage(bool in_main_frame) {
  // TODO(nasko): Consider supporting error page isolation in subframes if
  // Site Isolation is enabled.
  return in_main_frame;
}

bool ChromeContentBrowserClient::ShouldAssignSiteForURL(const GURL& url) {
  return !url.SchemeIs(chrome::kChromeNativeScheme);
}

std::vector<url::Origin>
ChromeContentBrowserClient::GetOriginsRequiringDedicatedProcess() {
  std::vector<url::Origin> isolated_origin_list;

// Sign-in process isolation is not needed on Android, see
// https://crbug.com/739418.
#if !defined(OS_ANDROID)
  isolated_origin_list.push_back(
      url::Origin::Create(GaiaUrls::GetInstance()->gaia_url()));
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
  auto origins_from_extensions = ChromeContentBrowserClientExtensionsPart::
      GetOriginsRequiringDedicatedProcess();
  std::move(std::begin(origins_from_extensions),
            std::end(origins_from_extensions),
            std::back_inserter(isolated_origin_list));
#endif

  // Include additional origins preloaded with specific browser configurations,
  // if any.  For example, this is used on Google Chrome for Android to preload
  // a list of important sites to isolate.
  auto built_in_origins =
      site_isolation::GetBrowserSpecificBuiltInIsolatedOrigins();
  std::move(std::begin(built_in_origins), std::end(built_in_origins),
            std::back_inserter(isolated_origin_list));

  return isolated_origin_list;
}

bool ChromeContentBrowserClient::ShouldEnableStrictSiteIsolation() {
  return base::FeatureList::IsEnabled(features::kSitePerProcess);
}

bool ChromeContentBrowserClient::ShouldDisableSiteIsolation() {
  return SiteIsolationPolicy::ShouldDisableSiteIsolationDueToMemoryThreshold();
}

std::vector<std::string>
ChromeContentBrowserClient::GetAdditionalSiteIsolationModes() {
  if (SiteIsolationPolicy::IsIsolationForPasswordSitesEnabled())
    return {"Isolate Password Sites"};
  else
    return {};
}

void ChromeContentBrowserClient::PersistIsolatedOrigin(
    content::BrowserContext* context,
    const url::Origin& origin) {
  DCHECK(!context->IsOffTheRecord());
  Profile* profile = Profile::FromBrowserContext(context);
  ListPrefUpdate update(profile->GetPrefs(),
                        prefs::kUserTriggeredIsolatedOrigins);
  base::ListValue* list = update.Get();
  base::Value value(origin.Serialize());
  if (!base::Contains(list->GetList(), value))
    list->Append(std::move(value));
}

bool ChromeContentBrowserClient::IsFileAccessAllowed(
    const base::FilePath& path,
    const base::FilePath& absolute_path,
    const base::FilePath& profile_path) {
  return ChromeNetworkDelegate::IsAccessAllowed(path, absolute_path,
                                                profile_path);
}

namespace {

bool IsAutoReloadEnabled() {
  const base::CommandLine& browser_command_line =
      *base::CommandLine::ForCurrentProcess();
  if (browser_command_line.HasSwitch(switches::kEnableAutoReload))
    return true;
  if (browser_command_line.HasSwitch(switches::kDisableAutoReload))
    return false;
  return true;
}

void MaybeAppendBlinkSettingsSwitchForFieldTrial(
    const base::CommandLine& browser_command_line,
    base::CommandLine* command_line) {
  // List of field trials that modify the blink-settings command line flag. No
  // two field trials in the list should specify the same keys, otherwise one
  // field trial may overwrite another. See Source/core/frame/Settings.in in
  // Blink for the list of valid keys.
  static const char* const kBlinkSettingsFieldTrials[] = {
      // Keys: disallowFetchForDocWrittenScriptsInMainFrame
      //       disallowFetchForDocWrittenScriptsInMainFrameOnSlowConnections
      //       disallowFetchForDocWrittenScriptsInMainFrameIfEffectively2G
      "DisallowFetchForDocWrittenScriptsInMainFrame",
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
#if defined(OS_ANDROID)
  bool enable_crash_reporter = true;
#else
  bool enable_crash_reporter = false;
  if (crash_reporter::IsCrashpadEnabled()) {
    command_line->AppendSwitch(crash_reporter::kEnableCrashpad);
    enable_crash_reporter = true;

    int fd;
    pid_t pid;
    if (crash_reporter::GetHandlerSocket(&fd, &pid)) {
      command_line->AppendSwitchASCII(
          crash_reporter::switches::kCrashpadHandlerPid,
          base::NumberToString(pid));
    }
  } else {
    enable_crash_reporter = breakpad::IsCrashReporterEnabled();
  }
#endif
  if (enable_crash_reporter) {
    std::string switch_value;
    std::unique_ptr<metrics::ClientInfo> client_info =
        GoogleUpdateSettings::LoadMetricsClientInfo();
    if (client_info)
      switch_value = client_info->client_id;
    switch_value.push_back(',');
    switch_value.append(chrome::GetChannelName());
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
                                 base::size(kCommonSwitchNames));

  static const char* const kDinosaurEasterEggSwitches[] = {
      error_page::switches::kDisableDinosaurEasterEgg,
  };
  command_line->CopySwitchesFrom(browser_command_line,
                                 kDinosaurEasterEggSwitches,
                                 base::size(kDinosaurEasterEggSwitches));

  if (content::Referrer::ShouldForceLegacyDefaultReferrerPolicy())
    command_line->AppendSwitch(switches::kForceLegacyDefaultReferrerPolicy);

#if defined(OS_CHROMEOS)
  // On Chrome OS need to pass primary user homedir (in multi-profiles session).
  base::FilePath homedir;
  base::PathService::Get(base::DIR_HOME, &homedir);
  command_line->AppendSwitchASCII(chromeos::switches::kHomedir,
                                  homedir.value().c_str());
#endif

  if (process_type == switches::kRendererProcess) {
    content::RenderProcessHost* process =
        content::RenderProcessHost::FromID(child_process_id);
    Profile* profile =
        process ? Profile::FromBrowserContext(process->GetBrowserContext())
                : nullptr;
    for (size_t i = 0; i < extra_parts_.size(); ++i) {
      extra_parts_[i]->AppendExtraRendererCommandLineSwitches(command_line,
                                                              process, profile);
    }

#if defined(OS_CHROMEOS)
    const std::string& login_profile = browser_command_line.GetSwitchValueASCII(
        chromeos::switches::kLoginProfile);
    if (!login_profile.empty())
      command_line->AppendSwitchASCII(chromeos::switches::kLoginProfile,
                                      login_profile);
#endif

    MaybeCopyDisableWebRtcEncryptionSwitch(command_line, browser_command_line,
                                           chrome::GetChannel());
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
        for (auto it = switches->begin(); it != switches->end(); ++it) {
          std::string switch_to_enable;
          if (it->GetAsString(&switch_to_enable))
            command_line->AppendSwitch(switch_to_enable);
        }
      }

#if defined(FULL_SAFE_BROWSING)
      // Disable client-side phishing detection in the renderer if it is
      // disabled in the Profile preferences, or by command line flag.
      if (!safe_browsing::IsSafeBrowsingEnabled(*prefs) ||
          !safe_browsing::ClientSideDetectionServiceFactory::GetForProfile(
              profile)) {
        command_line->AppendSwitch(
            switches::kDisableClientSidePhishingDetection);
      }
#endif

      if (prefs->GetBoolean(prefs::kPrintPreviewDisabled))
        command_line->AppendSwitch(switches::kDisablePrintPreview);

#if !defined(OS_ANDROID)
      InstantService* instant_service =
          InstantServiceFactory::GetForProfile(profile);
      if (instant_service &&
          instant_service->IsInstantProcess(process->GetID())) {
        command_line->AppendSwitch(switches::kInstantProcess);
      }
#endif

      if (prefs->HasPrefPath(prefs::kAllowDinosaurEasterEgg) &&
          !prefs->GetBoolean(prefs::kAllowDinosaurEasterEgg)) {
        command_line->AppendSwitch(
            error_page::switches::kDisableDinosaurEasterEgg);
      }

      MaybeAppendSecureOriginsAllowlistSwitch(command_line);

      if (prefs->HasPrefPath(prefs::kAllowPopupsDuringPageUnload) &&
          prefs->GetBoolean(prefs::kAllowPopupsDuringPageUnload)) {
        command_line->AppendSwitch(switches::kAllowPopupsDuringPageUnload);
      }

      if (prefs->HasPrefPath(prefs::kAllowSyncXHRInPageDismissal) &&
          prefs->GetBoolean(prefs::kAllowSyncXHRInPageDismissal)) {
        command_line->AppendSwitch(switches::kAllowSyncXHRInPageDismissal);
      }

      if (prefs->HasPrefPath(prefs::kWebComponentsV0Enabled) &&
          prefs->GetBoolean(prefs::kWebComponentsV0Enabled)) {
        command_line->AppendSwitch(switches::kWebComponentsV0Enabled);
      }
      if (prefs->HasPrefPath(prefs::kUseLegacyFormControls) &&
          prefs->GetBoolean(prefs::kUseLegacyFormControls)) {
        command_line->AppendSwitch(switches::kUseLegacyFormControls);
      }

      if (prefs->HasPrefPath(prefs::kScrollToTextFragmentEnabled) &&
          !prefs->GetBoolean(prefs::kScrollToTextFragmentEnabled)) {
        command_line->AppendSwitch(switches::kDisableScrollToTextFragment);
      }

      if (!profile->ShouldEnableOutOfBlinkCors()) {
        command_line->AppendSwitch(
            network::switches::kForceToDisableOutOfBlinkCors);
      }

      if (prefs->HasPrefPath(prefs::kAppCacheForceEnabled) &&
          prefs->GetBoolean(prefs::kAppCacheForceEnabled)) {
        command_line->AppendSwitch(switches::kAppCacheForceEnabled);
      }

      // The UserAgentClientHint feature is typically managed via a
      // base::Feature, but it has a managed policy override. The override is
      // communicated to blink via a custom command-line flag.
      PrefService* local_state = g_browser_process->local_state();
      if (!local_state->GetBoolean(
              policy::policy_prefs::kUserAgentClientHintsEnabled)) {
        command_line->AppendSwitch(
            blink::switches::kUserAgentClientHintDisable);
      }
    }

    if (IsAutoReloadEnabled())
      command_line->AppendSwitch(switches::kEnableAutoReload);

    MaybeAppendBlinkSettingsSwitchForFieldTrial(browser_command_line,
                                                command_line);

#if defined(OS_ANDROID)
    // If the platform is Android, force the distillability service on.
    command_line->AppendSwitch(switches::kEnableDistillabilityService);
#endif

    // Please keep this in alphabetical order.
    static const char* const kSwitchNames[] = {
      autofill::switches::kIgnoreAutocompleteOffForAutofill,
      autofill::switches::kShowAutofillSignatures,
#if defined(OS_CHROMEOS)
      switches::kShortMergeSessionTimeoutForTest,  // For tests only.
#endif
#if BUILDFLAG(ENABLE_EXTENSIONS)
      extensions::switches::kAllowHTTPBackgroundPage,
      extensions::switches::kAllowLegacyExtensionManifests,
      extensions::switches::kDisableExtensionsHttpThrottling,
      extensions::switches::kEnableExperimentalExtensionApis,
      extensions::switches::kExtensionsOnChromeURLs,
      extensions::switches::kSetExtensionThrottleTestParams,  // For tests only.
      extensions::switches::kWhitelistedExtensionID,
#endif
      switches::kAllowInsecureLocalhost,
      switches::kAppsGalleryURL,
      switches::kCloudPrintURL,
      switches::kCloudPrintXmppEndpoint,
      switches::kDisableBundledPpapiFlash,
      switches::kDisableJavaScriptHarmonyShipping,
      variations::switches::kEnableBenchmarking,
      switches::kEnableDistillabilityService,
      switches::kEnableNaCl,
#if BUILDFLAG(ENABLE_NACL)
      switches::kEnableNaClDebug,
      switches::kEnableNaClNonSfiMode,
#endif
      switches::kEnableNetBenchmarking,
#if defined(OS_CHROMEOS)
      switches::kForceAppMode,
#endif
#if BUILDFLAG(ENABLE_NACL)
      switches::kForcePNaClSubzero,
#endif
      switches::kForceUIDirection,
      switches::kJavaScriptHarmony,
      switches::kEnableExperimentalWebAssemblyFeatures,
      embedder_support::kOriginTrialDisabledFeatures,
      embedder_support::kOriginTrialDisabledTokens,
      embedder_support::kOriginTrialPublicKey,
      switches::kPpapiFlashArgs,
      switches::kPpapiFlashPath,
      switches::kPpapiFlashVersion,
      switches::kReaderModeHeuristics,
      translate::switches::kTranslateSecurityOrigin,
    };

    command_line->CopySwitchesFrom(browser_command_line, kSwitchNames,
                                   base::size(kSwitchNames));
  } else if (process_type == switches::kUtilityProcess) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
    static const char* const kSwitchNames[] = {
        extensions::switches::kAllowHTTPBackgroundPage,
        extensions::switches::kEnableExperimentalExtensionApis,
        extensions::switches::kExtensionsOnChromeURLs,
        extensions::switches::kWhitelistedExtensionID,
    };

    command_line->CopySwitchesFrom(browser_command_line, kSwitchNames,
                                   base::size(kSwitchNames));
#endif
    MaybeAppendSecureOriginsAllowlistSwitch(command_line);
  } else if (process_type == service_manager::switches::kZygoteProcess) {
    static const char* const kSwitchNames[] = {
      // Load (in-process) Pepper plugins in-process in the zygote pre-sandbox.
      switches::kDisableBundledPpapiFlash,
#if BUILDFLAG(ENABLE_NACL)
      switches::kEnableNaClDebug,
      switches::kEnableNaClNonSfiMode,
      switches::kForcePNaClSubzero,
      switches::kNaClDangerousNoSandboxNonSfi,
#endif
      switches::kPpapiFlashPath,
      switches::kPpapiFlashVersion,
    };

    command_line->CopySwitchesFrom(browser_command_line, kSwitchNames,
                                   base::size(kSwitchNames));
  } else if (process_type == switches::kGpuProcess) {
    // If --ignore-gpu-blacklist is passed in, don't send in crash reports
    // because GPU is expected to be unreliable.
    if (browser_command_line.HasSwitch(switches::kIgnoreGpuBlacklist) &&
        !command_line->HasSwitch(switches::kDisableBreakpad))
      command_line->AppendSwitch(switches::kDisableBreakpad);
  }

#if defined(OS_CHROMEOS)
  if (ChromeCrashReporterClient::ShouldPassCrashLoopBefore(process_type)) {
    static const char* const kSwitchNames[] = {
        crash_reporter::switches::kCrashLoopBefore,
    };
    command_line->CopySwitchesFrom(browser_command_line, kSwitchNames,
                                   base::size(kSwitchNames));
  }
#endif

  StackSamplingConfiguration::Get()->AppendCommandLineSwitchForChildProcess(
      process_type, command_line);

#if defined(OS_LINUX)
  // Processes may only query perf_event_open with the BPF sandbox disabled.
  if (browser_command_line.HasSwitch(switches::kEnableThreadInstructionCount) &&
      command_line->HasSwitch(service_manager::switches::kNoSandbox)) {
    command_line->AppendSwitch(switches::kEnableThreadInstructionCount);
  }
#endif
}

std::string
ChromeContentBrowserClient::GetApplicationClientGUIDForQuarantineCheck() {
  return std::string(chrome::kApplicationClientIDStringForAVScanning);
}

std::string ChromeContentBrowserClient::GetApplicationLocale() {
  if (BrowserThread::CurrentlyOn(BrowserThread::IO))
    return GetIOThreadApplicationLocale();
  return g_browser_process->GetApplicationLocale();
}

std::string ChromeContentBrowserClient::GetAcceptLangs(
    content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
  return profile->GetPrefs()->GetString(language::prefs::kAcceptLanguages);
}

gfx::ImageSkia ChromeContentBrowserClient::GetDefaultFavicon() {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  return rb.GetNativeImageNamed(IDR_DEFAULT_FAVICON).AsImageSkia();
}

bool ChromeContentBrowserClient::IsDataSaverEnabled(
    content::BrowserContext* browser_context) {
  if (!browser_context || browser_context->IsOffTheRecord())
    return false;

  Profile* profile = Profile::FromBrowserContext(browser_context);
  return profile && data_reduction_proxy::DataReductionProxySettings::
                        IsDataSaverEnabledByUser(profile->IsOffTheRecord(),
                                                 profile->GetPrefs());
}

void ChromeContentBrowserClient::UpdateRendererPreferencesForWorker(
    content::BrowserContext* browser_context,
    blink::mojom::RendererPreferences* out_prefs) {
  DCHECK(browser_context);
  DCHECK(out_prefs);
  renderer_preferences_util::UpdateFromSystemSettings(
      out_prefs, Profile::FromBrowserContext(browser_context));
}

bool ChromeContentBrowserClient::AllowAppCache(
    const GURL& manifest_url,
    const GURL& site_for_cookies,
    const base::Optional<url::Origin>& top_frame_origin,
    content::BrowserContext* context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return CookieSettingsFactory::GetForProfile(
             Profile::FromBrowserContext(context))
      ->IsCookieAccessAllowed(manifest_url, site_for_cookies, top_frame_origin);
}

content::AllowServiceWorkerResult
ChromeContentBrowserClient::AllowServiceWorkerOnIO(
    const GURL& scope,
    const GURL& site_for_cookies,
    const base::Optional<url::Origin>& top_frame_origin,
    const GURL& script_url,
    content::ResourceContext* context) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  GURL first_party_url = top_frame_origin ? top_frame_origin->GetURL() : GURL();

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Check if this is an extension-related service worker, and, if so, if it's
  // allowed (this can return false if, e.g., the extension is disabled).
  // If it's not allowed, return immediately. We deliberately do *not* report
  // to the TabSpecificContentSettings, since the service worker is blocked
  // because of the extension, rather than because of the user's content
  // settings.
  if (!ChromeContentBrowserClientExtensionsPart::AllowServiceWorkerOnIO(
          scope, first_party_url, script_url, context)) {
    return content::AllowServiceWorkerResult::No();
  }
#endif

  ProfileIOData* io_data = ProfileIOData::FromResourceContext(context);

  // Check if JavaScript is allowed.
  content_settings::SettingInfo info;
  std::unique_ptr<base::Value> value =
      io_data->GetHostContentSettingsMap()->GetWebsiteSetting(
          first_party_url, first_party_url, ContentSettingsType::JAVASCRIPT,
          std::string(), &info);
  ContentSetting setting = content_settings::ValueToContentSetting(value.get());
  bool allow_javascript = (setting == CONTENT_SETTING_ALLOW);

  // Check if cookies are allowed.
  bool allow_cookies = io_data->GetCookieSettings()->IsCookieAccessAllowed(
      scope, site_for_cookies, top_frame_origin);

  return content::AllowServiceWorkerResult::FromPolicy(!allow_javascript,
                                                       !allow_cookies);
}

content::AllowServiceWorkerResult
ChromeContentBrowserClient::AllowServiceWorkerOnUI(
    const GURL& scope,
    const GURL& site_for_cookies,
    const base::Optional<url::Origin>& top_frame_origin,
    const GURL& script_url,
    content::BrowserContext* context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  GURL first_party_url = top_frame_origin ? top_frame_origin->GetURL() : GURL();

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Check if this is an extension-related service worker, and, if so, if it's
  // allowed (this can return false if, e.g., the extension is disabled).
  // If it's not allowed, return immediately. We deliberately do *not* report
  // to the TabSpecificContentSettings, since the service worker is blocked
  // because of the extension, rather than because of the user's content
  // settings.
  if (!ChromeContentBrowserClientExtensionsPart::AllowServiceWorkerOnUI(
          scope, first_party_url, script_url, context)) {
    return content::AllowServiceWorkerResult::No();
  }
#endif

  Profile* profile = Profile::FromBrowserContext(context);

  // Check if JavaScript is allowed.
  content_settings::SettingInfo info;
  std::unique_ptr<base::Value> value =
      HostContentSettingsMapFactory::GetForProfile(profile)->GetWebsiteSetting(
          first_party_url, first_party_url, ContentSettingsType::JAVASCRIPT,
          std::string(), &info);
  ContentSetting setting = content_settings::ValueToContentSetting(value.get());
  bool allow_javascript = (setting == CONTENT_SETTING_ALLOW);

  // Check if cookies are allowed.
  bool allow_cookies =
      CookieSettingsFactory::GetForProfile(profile)->IsCookieAccessAllowed(
          scope, site_for_cookies, top_frame_origin);

  return content::AllowServiceWorkerResult::FromPolicy(!allow_javascript,
                                                       !allow_cookies);
}

bool ChromeContentBrowserClient::AllowSharedWorker(
    const GURL& worker_url,
    const GURL& site_for_cookies,
    const base::Optional<url::Origin>& top_frame_origin,
    const std::string& name,
    const url::Origin& constructor_origin,
    content::BrowserContext* context,
    int render_process_id,
    int render_frame_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Check if cookies are allowed.
  bool allow =
      CookieSettingsFactory::GetForProfile(Profile::FromBrowserContext(context))
          ->IsCookieAccessAllowed(worker_url, site_for_cookies,
                                  top_frame_origin);

  content_settings::TabSpecificContentSettings::SharedWorkerAccessed(
      render_process_id, render_frame_id, worker_url, name, constructor_origin,
      !allow);
  return allow;
}

bool ChromeContentBrowserClient::DoesSchemeAllowCrossOriginSharedWorker(
    const std::string& scheme) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Extensions are allowed to start cross-origin shared workers.
  if (scheme == extensions::kExtensionScheme)
    return true;
#endif

  return false;
}

bool ChromeContentBrowserClient::AllowSignedExchange(
    content::BrowserContext* browser_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  Profile* profile = Profile::FromBrowserContext(browser_context);
  return profile->GetPrefs()->GetBoolean(prefs::kSignedHTTPExchangeEnabled);
}

void ChromeContentBrowserClient::AllowWorkerFileSystem(
    const GURL& url,
    content::BrowserContext* browser_context,
    const std::vector<content::GlobalFrameRoutingId>& render_frames,
    base::OnceCallback<void(bool)> callback) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  auto cookie_settings = CookieSettingsFactory::GetForProfile(profile);
  bool allow = cookie_settings->IsCookieAccessAllowed(url, url, base::nullopt);

#if BUILDFLAG(ENABLE_EXTENSIONS)
  GuestPermissionRequestHelper(url, render_frames, std::move(callback), allow);
#else
  FileSystemAccessed(url, render_frames, std::move(callback), allow);
#endif
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
void ChromeContentBrowserClient::GuestPermissionRequestHelper(
    const GURL& url,
    const std::vector<content::GlobalFrameRoutingId>& render_frames,
    base::OnceCallback<void(bool)> callback,
    bool allow) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::map<int, int> process_map;
  std::map<int, int>::const_iterator it;
  bool has_web_view_guest = false;
  // Record access to file system for potential display in UI.
  for (const auto& it : render_frames) {
    if (process_map.find(it.child_id) != process_map.end())
      continue;

    process_map.insert(std::pair<int, int>(it.child_id, it.frame_routing_id));

    if (extensions::WebViewRendererState::GetInstance()->IsGuest(it.child_id))
      has_web_view_guest = true;
  }
  if (!has_web_view_guest) {
    FileSystemAccessed(url, render_frames, std::move(callback), allow);
    return;
  }
  DCHECK_EQ(1U, process_map.size());
  it = process_map.begin();

  extensions::WebViewPermissionHelper* web_view_permission_helper =
      extensions::WebViewPermissionHelper::FromFrameID(it->first, it->second);
  web_view_permission_helper->RequestFileSystemPermission(
      url, allow,
      base::BindOnce(&ChromeContentBrowserClient::FileSystemAccessed,
                     weak_factory_.GetWeakPtr(), url, render_frames,
                     std::move(callback)));
}
#endif

void ChromeContentBrowserClient::FileSystemAccessed(
    const GURL& url,
    const std::vector<content::GlobalFrameRoutingId>& render_frames,
    base::OnceCallback<void(bool)> callback,
    bool allow) {
  // Record access to file system for potential display in UI.
  for (const auto& it : render_frames) {
    content_settings::TabSpecificContentSettings::FileSystemAccessed(
        it.child_id, it.frame_routing_id, url, !allow);
  }
  std::move(callback).Run(allow);
}

bool ChromeContentBrowserClient::AllowWorkerIndexedDB(
    const GURL& url,
    content::BrowserContext* browser_context,
    const std::vector<content::GlobalFrameRoutingId>& render_frames) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  auto cookie_settings = CookieSettingsFactory::GetForProfile(profile);

  bool allow = cookie_settings->IsCookieAccessAllowed(url, url, base::nullopt);

  // Record access to IndexedDB for potential display in UI.
  for (const auto& it : render_frames) {
    content_settings::TabSpecificContentSettings::IndexedDBAccessed(
        it.child_id, it.frame_routing_id, url, !allow);
  }

  return allow;
}

bool ChromeContentBrowserClient::AllowWorkerCacheStorage(
    const GURL& url,
    content::BrowserContext* browser_context,
    const std::vector<content::GlobalFrameRoutingId>& render_frames) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  auto cookie_settings = CookieSettingsFactory::GetForProfile(profile);
  bool allow = cookie_settings->IsCookieAccessAllowed(url, url, base::nullopt);

  // Record access to CacheStorage for potential display in UI.
  for (const auto& it : render_frames) {
    content_settings::TabSpecificContentSettings::CacheStorageAccessed(
        it.child_id, it.frame_routing_id, url, !allow);
  }

  return allow;
}

bool ChromeContentBrowserClient::AllowWorkerWebLocks(
    const GURL& url,
    content::BrowserContext* browser_context,
    const std::vector<content::GlobalFrameRoutingId>& render_frames) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  auto cookie_settings = CookieSettingsFactory::GetForProfile(profile);
  return cookie_settings->IsCookieAccessAllowed(url, url, base::nullopt);
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
          permissions::PermissionContextBase::kPermissionsKillSwitchFieldStudy,
          "Bluetooth") ==
      permissions::PermissionContextBase::kPermissionsKillSwitchBlockedValue) {
    // The kill switch is enabled for this permission. Block requests.
    return AllowWebBluetoothResult::BLOCK_GLOBALLY_DISABLED;
  }

  const HostContentSettingsMap* const content_settings =
      HostContentSettingsMapFactory::GetForProfile(
          Profile::FromBrowserContext(browser_context));

  if (content_settings->GetContentSetting(
          requesting_origin.GetURL(), embedding_origin.GetURL(),
          ContentSettingsType::BLUETOOTH_GUARD,
          std::string()) == CONTENT_SETTING_BLOCK) {
    return AllowWebBluetoothResult::BLOCK_POLICY;
  }
  return AllowWebBluetoothResult::ALLOW;
}

std::string ChromeContentBrowserClient::GetWebBluetoothBlocklist() {
  return variations::GetVariationParamValue("WebBluetoothBlocklist",
                                            "blocklist_additions");
}

#if defined(OS_CHROMEOS)
void ChromeContentBrowserClient::OnTrustAnchorUsed(
    content::BrowserContext* browser_context) {
  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  if (user_manager) {
    const user_manager::User* user =
        chromeos::ProfileHelper::Get()->GetUserByProfile(
            Profile::FromBrowserContext(browser_context));
    if (user && !user->username_hash().empty()) {
      policy::PolicyCertServiceFactory::SetUsedPolicyCertificates(
          user->username_hash());
    }
  }
}
#endif

scoped_refptr<network::SharedURLLoaderFactory>
ChromeContentBrowserClient::GetSystemSharedURLLoaderFactory() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI) ||
         !BrowserThread::IsThreadInitialized(BrowserThread::UI));

  if (!SystemNetworkContextManager::GetInstance())
    return nullptr;

  return SystemNetworkContextManager::GetInstance()
      ->GetSharedURLLoaderFactory();
}

network::mojom::NetworkContext*
ChromeContentBrowserClient::GetSystemNetworkContext() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(g_browser_process->system_network_context_manager());
  return g_browser_process->system_network_context_manager()->GetContext();
}

std::string ChromeContentBrowserClient::GetGeolocationApiKey() {
  return google_apis::GetAPIKey();
}

#if defined(OS_ANDROID)
bool ChromeContentBrowserClient::ShouldUseGmsCoreGeolocationProvider() {
  // Indicate that Chrome uses the GMS core location provider.
  return true;
}
#endif

scoped_refptr<content::QuotaPermissionContext>
ChromeContentBrowserClient::CreateQuotaPermissionContext() {
  return new permissions::QuotaPermissionContextImpl();
}

content::GeneratedCodeCacheSettings
ChromeContentBrowserClient::GetGeneratedCodeCacheSettings(
    content::BrowserContext* context) {
  base::FilePath cache_path;
  chrome::GetUserCacheDirectory(context->GetPath(), &cache_path);
  // If we pass 0 for size, disk_cache will pick a default size using the
  // heuristics based on available disk size. These are implemented in
  // disk_cache::PreferredCacheSize in net/disk_cache/cache_util.cc.
  int64_t size_in_bytes = 0;
  DCHECK(g_browser_process);
  PrefService* local_state = g_browser_process->local_state();
  if (local_state) {
    size_in_bytes = local_state->GetInteger(prefs::kDiskCacheSize);
    base::FilePath disk_cache_dir =
        local_state->GetFilePath(prefs::kDiskCacheDir);
    if (!disk_cache_dir.empty())
      cache_path = disk_cache_dir.Append(cache_path.BaseName());
  }
  return content::GeneratedCodeCacheSettings(true, size_in_bytes, cache_path);
}

void ChromeContentBrowserClient::AllowCertificateError(
    content::WebContents* web_contents,
    int cert_error,
    const net::SSLInfo& ssl_info,
    const GURL& request_url,
    bool is_main_frame_request,
    bool strict_enforcement,
    base::OnceCallback<void(content::CertificateRequestResultType)> callback) {
  DCHECK(web_contents);
  if (!is_main_frame_request) {
    // A sub-resource has a certificate error. The user doesn't really
    // have a context for making the right decision, so block the
    // request hard, without an info bar to allow showing the insecure
    // content.
    if (!callback.is_null())
      std::move(callback).Run(content::CERTIFICATE_REQUEST_RESULT_TYPE_DENY);
    return;
  }

  // If the tab is being prerendered, cancel the prerender and the request.
  prerender::PrerenderContents* prerender_contents =
      prerender::PrerenderContents::FromWebContents(web_contents);
  if (prerender_contents) {
    prerender_contents->Destroy(prerender::FINAL_STATUS_SSL_ERROR);
    if (!callback.is_null()) {
      std::move(callback).Run(content::CERTIFICATE_REQUEST_RESULT_TYPE_CANCEL);
    }
    return;
  }

  std::move(callback).Run(content::CERTIFICATE_REQUEST_RESULT_TYPE_DENY);
  return;
}

#if !defined(OS_ANDROID)
bool ChromeContentBrowserClient::ShouldDenyRequestOnCertificateError(
    const GURL main_page_url) {
  // Desktop Reader Mode pages should never load resources with certificate
  // errors. Desktop Reader Mode is more strict about security than Reader Mode
  // on Android: the desktop version has its own security indicator and
  // is not downgraded to a WARNING, whereas Android will show "Not secure"
  // in the omnibox (for low-end devices which show the omnibox on Reader Mode
  // pages).
  return main_page_url.SchemeIs(dom_distiller::kDomDistillerScheme);
}
#endif

namespace {

certificate_matching::CertificatePrincipalPattern
ParseCertificatePrincipalPattern(const base::Value* pattern) {
  return certificate_matching::CertificatePrincipalPattern::
      ParseFromOptionalDict(pattern, "CN", "L", "O", "OU");
}

// Attempts to auto-select a client certificate according to the value of
// |ContentSettingsType::AUTO_SELECT_CERTIFICATE| content setting for
// |requesting_url|. If no certificate was auto-selected, returns nullptr.
std::unique_ptr<net::ClientCertIdentity> AutoSelectCertificate(
    Profile* profile,
    const GURL& requesting_url,
    net::ClientCertIdentityList& client_certs) {
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile);
  std::unique_ptr<base::Value> setting =
      host_content_settings_map->GetWebsiteSetting(
          requesting_url, requesting_url,
          ContentSettingsType::AUTO_SELECT_CERTIFICATE, std::string(), nullptr);

  if (!setting || !setting->is_dict())
    return nullptr;

  const base::Value* filters =
      setting->FindKeyOfType("filters", base::Value::Type::LIST);
  if (!filters) {
    // |setting_dict| has the wrong format (e.g. single filter instead of a
    // list of filters). This content setting is only provided by
    // the |PolicyProvider|, which should always set it to a valid format.
    // Therefore, delete the invalid value.
    host_content_settings_map->SetWebsiteSettingDefaultScope(
        requesting_url, requesting_url,
        ContentSettingsType::AUTO_SELECT_CERTIFICATE, std::string(), nullptr);
    return nullptr;
  }

  for (const base::Value& filter : filters->GetList()) {
    DCHECK(filter.is_dict());

    auto issuer_pattern = ParseCertificatePrincipalPattern(
        filter.FindKeyOfType("ISSUER", base::Value::Type::DICTIONARY));
    auto subject_pattern = ParseCertificatePrincipalPattern(
        filter.FindKeyOfType("SUBJECT", base::Value::Type::DICTIONARY));
    // Use the first certificate that is matched by the filter.
    for (auto& client_cert : client_certs) {
      if (issuer_pattern.Matches(client_cert->certificate()->issuer()) &&
          subject_pattern.Matches(client_cert->certificate()->subject())) {
        return std::move(client_cert);
      }
    }
  }

  return nullptr;
}

}  // namespace

base::OnceClosure ChromeContentBrowserClient::SelectClientCertificate(
    content::WebContents* web_contents,
    net::SSLCertRequestInfo* cert_request_info,
    net::ClientCertIdentityList client_certs,
    std::unique_ptr<content::ClientCertificateDelegate> delegate) {
  prerender::PrerenderContents* prerender_contents =
      prerender::PrerenderContents::FromWebContents(web_contents);
  if (prerender_contents) {
    prerender_contents->Destroy(
        prerender::FINAL_STATUS_SSL_CLIENT_CERTIFICATE_REQUESTED);
    return base::OnceClosure();
  }

  GURL requesting_url("https://" + cert_request_info->host_and_port.ToString());
  DCHECK(requesting_url.is_valid())
      << "Invalid URL string: https://"
      << cert_request_info->host_and_port.ToString();

  bool may_show_cert_selection = true;

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
#if defined(OS_CHROMEOS)
  if (chromeos::ProfileHelper::IsSigninProfile(profile)) {
    // On the sign-in profile, never show certificate selection to the user. A
    // client certificate is an identifier that can be stable for a long time,
    // so only the administrator is allowed to decide which endpoints should see
    // it.
    may_show_cert_selection = false;

    content::StoragePartition* storage_partition =
        content::BrowserContext::GetStoragePartition(
            profile, web_contents->GetSiteInstance());
    chromeos::login::SigninPartitionManager* signin_partition_manager =
        chromeos::login::SigninPartitionManager::Factory::GetForBrowserContext(
            profile);

    // On the sign-in profile, only allow client certs in the context of the
    // sign-in frame.
    if (!signin_partition_manager->IsCurrentSigninStoragePartition(
            storage_partition)) {
      LOG(WARNING)
          << "Client cert requested in sign-in profile in wrong context.";
      // Continue without client certificate. We do this to mimic the case of no
      // client certificate being present in the profile's certificate store.
      delegate->ContinueWithCertificate(nullptr, nullptr);
      return base::OnceClosure();
    }
    VLOG(1) << "Client cert requested in sign-in profile.";
  }
#endif  // defined(OS_CHROMEOS)

  std::unique_ptr<net::ClientCertIdentity> auto_selected_identity =
      AutoSelectCertificate(profile, requesting_url, client_certs);
  if (auto_selected_identity) {
    // The callback will own |auto_selected_identity| and |delegate|, keeping
    // them alive until after ContinueWithCertificate is called.
    scoped_refptr<net::X509Certificate> cert =
        auto_selected_identity->certificate();
    net::ClientCertIdentity::SelfOwningAcquirePrivateKey(
        std::move(auto_selected_identity),
        base::BindOnce(
            &content::ClientCertificateDelegate::ContinueWithCertificate,
            std::move(delegate), std::move(cert)));
    LogClientAuthResult(ClientCertSelectionResult::kAutoSelect);
    return base::OnceClosure();
  }

  if (!may_show_cert_selection) {
    LOG(WARNING) << "No client cert matched by policy and user selection is "
                    "not allowed.";
    LogClientAuthResult(ClientCertSelectionResult::kNoSelectionAllowed);
    // Continue without client certificate. We do this to mimic the case of no
    // client certificate being present in the profile's certificate store.
    delegate->ContinueWithCertificate(nullptr, nullptr);
    return base::OnceClosure();
  }

  return chrome::ShowSSLClientCertificateSelector(
      web_contents, cert_request_info, std::move(client_certs),
      std::move(delegate));
}

content::MediaObserver* ChromeContentBrowserClient::GetMediaObserver() {
  return MediaCaptureDevicesDispatcher::GetInstance();
}

content::FeatureObserverClient*
ChromeContentBrowserClient::GetFeatureObserverClient() {
  return ChromeBrowserMainExtraPartsPerformanceManager::GetInstance()
      ->GetFeatureObserverClient();
}

content::PlatformNotificationService*
ChromeContentBrowserClient::GetPlatformNotificationService(
    content::BrowserContext* browser_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  Profile* profile = Profile::FromBrowserContext(browser_context);
  return PlatformNotificationServiceFactory::GetForProfile(profile);
}

bool ChromeContentBrowserClient::CanCreateWindow(
    RenderFrameHost* opener,
    const GURL& opener_url,
    const GURL& opener_top_level_frame_url,
    const url::Origin& source_origin,
    content::mojom::WindowContainerType container_type,
    const GURL& target_url,
    const content::Referrer& referrer,
    const std::string& frame_name,
    WindowOpenDisposition disposition,
    const blink::mojom::WindowFeatures& features,
    bool user_gesture,
    bool opener_suppressed,
    bool* no_javascript_access) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(opener);

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(opener);
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  DCHECK(profile);
  *no_javascript_access = false;

  // If the opener is trying to create a background window but doesn't have
  // the appropriate permission, fail the attempt.
  if (container_type == content::mojom::WindowContainerType::BACKGROUND) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
    auto* process_map = extensions::ProcessMap::Get(profile);
    auto* registry = extensions::ExtensionRegistry::Get(profile);
    if (!URLHasExtensionBackgroundPermission(process_map, registry, opener_url,
                                             opener->GetProcess()->GetID())) {
      return false;
    }

    // Note: this use of GetExtensionOrAppByURL is safe but imperfect.  It may
    // return a recently installed Extension even if this CanCreateWindow call
    // was made by an old copy of the page in a normal web process.  That's ok,
    // because the permission check above would have caused an early return
    // already. We must use the full URL to find hosted apps, though, and not
    // just the origin.
    const Extension* extension =
        registry->enabled_extensions().GetExtensionOrAppByURL(opener_url);
    if (extension && !extensions::BackgroundInfo::AllowJSAccess(extension))
      *no_javascript_access = true;
#endif

    return true;
  }

#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (extensions::WebViewRendererState::GetInstance()->IsGuest(
          opener->GetProcess()->GetID())) {
    return true;
  }

  if (target_url.SchemeIs(extensions::kExtensionScheme)) {
    // Intentionally duplicating |registry| code from above because we want to
    // reduce calls to retrieve them as this function is a SYNC IPC handler.
    auto* registry = extensions::ExtensionRegistry::Get(profile);
    const Extension* extension =
        registry->enabled_extensions().GetExtensionOrAppByURL(target_url);
    if (extension && extension->is_platform_app()) {
      UMA_HISTOGRAM_ENUMERATION(
          "Extensions.AppLoadedInTab",
          ClassifyAppLoadedInTabSource(opener_url, extension),
          APP_LOADED_IN_TAB_SOURCE_MAX);

      // window.open() may not be used to load v2 apps in a regular tab.
      return false;
    }
  }
#endif

#if BUILDFLAG(ENABLE_PLUGINS)
  HostContentSettingsMap* content_settings =
      HostContentSettingsMapFactory::GetForProfile(profile);
  if (FlashDownloadInterception::ShouldStopFlashDownloadAction(
          content_settings, opener_top_level_frame_url, target_url,
          user_gesture)) {
    FlashDownloadInterception::InterceptFlashDownloadNavigation(
        web_contents, opener_top_level_frame_url);
    return false;
  }
#endif

  DCHECK(!prerender::PrerenderContents::FromWebContents(web_contents));

  BlockedWindowParams blocked_params(
      target_url, source_origin, opener->GetSiteInstance(), referrer,
      frame_name, disposition, features, user_gesture, opener_suppressed);
  NavigateParams nav_params = blocked_params.CreateNavigateParams(web_contents);
  return !MaybeBlockPopup(web_contents, &opener_top_level_frame_url,
                          &nav_params, nullptr /*=open_url_params*/,
                          blocked_params.features());
}

content::SpeechRecognitionManagerDelegate*
ChromeContentBrowserClient::CreateSpeechRecognitionManagerDelegate() {
  return new speech::ChromeSpeechRecognitionManagerDelegate();
}

content::TtsControllerDelegate*
ChromeContentBrowserClient::GetTtsControllerDelegate() {
  TtsControllerDelegateImpl* delegate =
      TtsControllerDelegateImpl::GetInstance();
#if !defined(OS_ANDROID)
  TtsExtensionEngine* tts_extension_engine = TtsExtensionEngine::GetInstance();
  delegate->SetTtsEngineDelegate(tts_extension_engine);
#endif
  return delegate;
}

content::TtsPlatform* ChromeContentBrowserClient::GetTtsPlatform() {
#ifdef OS_CHROMEOS
  return TtsPlatformImplChromeOs::GetInstance();
#else
  return nullptr;
#endif
}

void ChromeContentBrowserClient::OverrideWebkitPrefs(
    RenderViewHost* rvh,
    WebPreferences* web_prefs) {
  Profile* profile =
      Profile::FromBrowserContext(rvh->GetProcess()->GetBrowserContext());
  PrefService* prefs = profile->GetPrefs();

// Fill font preferences. These are not registered on Android
// - http://crbug.com/308033, http://crbug.com/696364.
#if !defined(OS_ANDROID)
  FontFamilyCache::FillFontFamilyMap(profile,
                                     prefs::kWebKitStandardFontFamilyMap,
                                     &web_prefs->standard_font_family_map);
  FontFamilyCache::FillFontFamilyMap(profile, prefs::kWebKitFixedFontFamilyMap,
                                     &web_prefs->fixed_font_family_map);
  FontFamilyCache::FillFontFamilyMap(profile, prefs::kWebKitSerifFontFamilyMap,
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

  web_prefs->default_font_size =
      prefs->GetInteger(prefs::kWebKitDefaultFontSize);
  web_prefs->default_fixed_font_size =
      prefs->GetInteger(prefs::kWebKitDefaultFixedFontSize);
  web_prefs->minimum_font_size =
      prefs->GetInteger(prefs::kWebKitMinimumFontSize);
  web_prefs->minimum_logical_font_size =
      prefs->GetInteger(prefs::kWebKitMinimumLogicalFontSize);
#endif

  web_prefs->default_encoding = prefs->GetString(prefs::kDefaultCharset);

  web_prefs->dom_paste_enabled =
      prefs->GetBoolean(prefs::kWebKitDomPasteEnabled);
  web_prefs->javascript_can_access_clipboard =
      prefs->GetBoolean(prefs::kWebKitJavascriptCanAccessClipboard);
  web_prefs->tabs_to_links = prefs->GetBoolean(prefs::kWebkitTabsToLinks);

  if (!prefs->GetBoolean(prefs::kWebKitJavascriptEnabled))
    web_prefs->javascript_enabled = false;

  if (!prefs->GetBoolean(prefs::kWebKitWebSecurityEnabled))
    web_prefs->web_security_enabled = false;

  if (!prefs->GetBoolean(prefs::kWebKitPluginsEnabled))
    web_prefs->plugins_enabled = false;
  web_prefs->loads_images_automatically =
      prefs->GetBoolean(prefs::kWebKitLoadsImagesAutomatically);

  if (prefs->GetBoolean(prefs::kDisable3DAPIs)) {
    web_prefs->webgl1_enabled = false;
    web_prefs->webgl2_enabled = false;
  }

  web_prefs->allow_running_insecure_content =
      prefs->GetBoolean(prefs::kWebKitAllowRunningInsecureContent);
#if defined(OS_ANDROID)
  web_prefs->font_scale_factor =
      static_cast<float>(prefs->GetDouble(prefs::kWebKitFontScaleFactor));
  web_prefs->force_enable_zoom =
      prefs->GetBoolean(prefs::kWebKitForceEnableZoom);
#endif
  web_prefs->force_dark_mode_enabled =
      prefs->GetBoolean(prefs::kWebKitForceDarkModeEnabled);

#if defined(OS_CHROMEOS)
  web_prefs->always_show_focus =
      prefs->GetBoolean(ash::prefs::kAccessibilityFocusHighlightEnabled);
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
  web_prefs->default_encoding =
      base::GetCanonicalEncodingNameByAliasName(web_prefs->default_encoding);
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

  web_prefs->data_saver_enabled = IsDataSaverEnabled(profile);

  web_prefs->data_saver_holdback_web_api_enabled =
      base::GetFieldTrialParamByFeatureAsBool(features::kDataSaverHoldback,
                                              "holdback_web", false);

  auto* contents = content::WebContents::FromRenderViewHost(rvh);
  if (contents) {
#if defined(OS_ANDROID)
    auto* delegate = TabAndroid::FromWebContents(contents)
                         ? static_cast<android::TabWebContentsDelegateAndroid*>(
                               contents->GetDelegate())
                         : nullptr;
    if (delegate) {
      web_prefs->embedded_media_experience_enabled =
          delegate->ShouldEnableEmbeddedMediaExperience();

      web_prefs->picture_in_picture_enabled =
          delegate->IsPictureInPictureEnabled();

      web_prefs->preferred_color_scheme =
          delegate->IsNightModeEnabled() ? blink::PreferredColorScheme::kDark
                                         : blink::PreferredColorScheme::kLight;
    }
#endif  // defined(OS_ANDROID)

    // web_app_scope value is platform specific.
#if defined(OS_ANDROID)
    if (delegate)
      web_prefs->web_app_scope = delegate->GetManifestScope();
#elif BUILDFLAG(ENABLE_EXTENSIONS)
    {
      web_prefs->web_app_scope = GURL();
      // Set |web_app_scope| based on the app associated with the app window if
      // any. Note that the app associated with the window never changes, even
      // if the app navigates off scope. This is not a problem because we still
      // want to use the scope of the app associated with the window, not the
      // WebContents.
      Browser* browser = chrome::FindBrowserWithWebContents(contents);
      if (browser && browser->app_controller() &&
          browser->app_controller()->HasAppId()) {
        const web_app::AppId& app_id = browser->app_controller()->GetAppId();
        const web_app::AppRegistrar& registrar =
            web_app::WebAppProviderBase::GetProviderBase(profile)->registrar();
        if (registrar.IsLocallyInstalled(app_id))
          web_prefs->web_app_scope = registrar.GetAppScope(app_id);
      }
    }
#endif

    web_prefs->immersive_mode_enabled = vr::VrTabHelper::IsInVr(contents);
  }

  if (base::FeatureList::IsEnabled(features::kLowPriorityIframes)) {
    // Obtain the maximum effective connection type at which the feature is
    // enabled.
    std::string effective_connection_type_param =
        base::GetFieldTrialParamValueByFeature(
            features::kLowPriorityIframes,
            "max_effective_connection_type_threshold");

    base::Optional<net::EffectiveConnectionType> effective_connection_type =
        net::GetEffectiveConnectionTypeForName(effective_connection_type_param);
    if (effective_connection_type) {
      web_prefs->low_priority_iframes_threshold =
          effective_connection_type.value();
    }
  }

  web_prefs->lazy_load_enabled = !contents || !contents->GetDelegate() ||
                                 contents->GetDelegate()->ShouldAllowLazyLoad();

  if (base::FeatureList::IsEnabled(features::kLazyFrameLoading)) {
    const char* param_name =
        web_prefs->data_saver_enabled
            ? "lazy_frame_loading_distance_thresholds_px_by_ect"
            : "lazy_frame_loading_distance_thresholds_px_by_ect_with_data_"
              "saver_enabled";

    base::StringPairs pairs;
    base::SplitStringIntoKeyValuePairs(
        base::GetFieldTrialParamValueByFeature(features::kLazyFrameLoading,
                                               param_name),
        ':', ',', &pairs);

    for (const auto& pair : pairs) {
      base::Optional<net::EffectiveConnectionType> effective_connection_type =
          net::GetEffectiveConnectionTypeForName(pair.first);
      int value = 0;
      if (effective_connection_type && base::StringToInt(pair.second, &value)) {
        web_prefs->lazy_frame_loading_distance_thresholds_px
            [effective_connection_type.value()] = value;
      }
    }
  }

  if (base::FeatureList::IsEnabled(features::kLazyImageLoading)) {
    const char* param_name =
        web_prefs->data_saver_enabled
            ? "lazy_image_loading_distance_thresholds_px_by_ect"
            : "lazy_image_loading_distance_thresholds_px_by_ect_with_data_"
              "saver_enabled";

    base::StringPairs pairs;
    base::SplitStringIntoKeyValuePairs(
        base::GetFieldTrialParamValueByFeature(features::kLazyImageLoading,
                                               param_name),
        ':', ',', &pairs);

    for (const auto& pair : pairs) {
      base::Optional<net::EffectiveConnectionType> effective_connection_type =
          net::GetEffectiveConnectionTypeForName(pair.first);
      int value = 0;
      if (effective_connection_type && base::StringToInt(pair.second, &value)) {
        web_prefs->lazy_image_loading_distance_thresholds_px
            [effective_connection_type.value()] = value;
      }
    }

    pairs.clear();
    base::SplitStringIntoKeyValuePairs(
        base::GetFieldTrialParamValueByFeature(features::kLazyImageLoading,
                                               "lazy_image_first_k_fully_load"),
        ':', ',', &pairs);

    for (const auto& pair : pairs) {
      base::Optional<net::EffectiveConnectionType> effective_connection_type =
          net::GetEffectiveConnectionTypeForName(pair.first);
      int value = 0;
      if (effective_connection_type && base::StringToInt(pair.second, &value)) {
        web_prefs
            ->lazy_image_first_k_fully_load[effective_connection_type.value()] =
            value;
      }
    }
  }

  if (base::FeatureList::IsEnabled(
          features::kNetworkQualityEstimatorWebHoldback)) {
    std::string effective_connection_type_param =
        base::GetFieldTrialParamValueByFeature(
            features::kNetworkQualityEstimatorWebHoldback,
            "web_effective_connection_type_override");

    base::Optional<net::EffectiveConnectionType> effective_connection_type =
        net::GetEffectiveConnectionTypeForName(effective_connection_type_param);
    DCHECK(effective_connection_type_param.empty() ||
           effective_connection_type);
    if (effective_connection_type) {
      DCHECK_NE(net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN,
                effective_connection_type.value());
      web_prefs->network_quality_estimator_web_holdback =
          effective_connection_type.value();
    }
  }

#if !defined(OS_ANDROID)
  if (IsAutoplayAllowedByPolicy(contents, prefs)) {
    // If autoplay is allowed by policy then force the no user gesture required
    // autoplay policy.
    web_prefs->autoplay_policy =
        content::AutoplayPolicy::kNoUserGestureRequired;
  } else if (base::FeatureList::IsEnabled(media::kAutoplayDisableSettings) &&
             web_prefs->autoplay_policy ==
                 content::AutoplayPolicy::kDocumentUserActivationRequired) {
    // If the autoplay disable settings feature is enabled and the autoplay
    // policy is set to using the unified policy then set the default autoplay
    // policy based on user preference.
    web_prefs->autoplay_policy =
        UnifiedAutoplayConfig::ShouldBlockAutoplay(profile)
            ? content::AutoplayPolicy::kDocumentUserActivationRequired
            : content::AutoplayPolicy::kNoUserGestureRequired;
  }

  auto* native_theme = GetWebTheme();
  switch (native_theme->GetPreferredColorScheme()) {
    case ui::NativeTheme::PreferredColorScheme::kDark:
      web_prefs->preferred_color_scheme = blink::PreferredColorScheme::kDark;
      break;
    case ui::NativeTheme::PreferredColorScheme::kLight:
      web_prefs->preferred_color_scheme = blink::PreferredColorScheme::kLight;
      break;
    case ui::NativeTheme::PreferredColorScheme::kNoPreference:
      web_prefs->preferred_color_scheme =
          blink::PreferredColorScheme::kNoPreference;
  }
#endif  // !defined(OS_ANDROID)

  web_prefs->translate_service_available = TranslateService::IsAvailable(prefs);

  // Force a light preferred color scheme on certain URLs if kWebUIDarkMode is
  // disabled; some of the UI is not yet correctly themed. Note: the WebUI CSS
  // explicitly uses light (instead of not dark), which is why we don't reset
  // back to no-preference. https://crbug.com/965811
  if (!base::FeatureList::IsEnabled(features::kWebUIDarkMode)) {
    const GURL url = rvh->GetSiteInstance()->GetSiteURL();
    bool force_light = url.SchemeIs(content::kChromeUIScheme);
#if BUILDFLAG(ENABLE_EXTENSIONS)
    if (!force_light) {
      force_light = url.SchemeIs(extensions::kExtensionScheme) &&
                    url.host_piece() == extension_misc::kPdfExtensionId;
    }
#endif
    if (force_light)
      web_prefs->preferred_color_scheme = blink::PreferredColorScheme::kLight;
  }

  base::Optional<ui::CaptionStyle> style =
      captions::GetCaptionStyleFromUserSettings(prefs,
                                                true /* record_metrics */);
  if (style) {
    web_prefs->text_track_background_color = style->background_color;
    web_prefs->text_track_text_color = style->text_color;
    web_prefs->text_track_text_size = style->text_size;
    web_prefs->text_track_text_shadow = style->text_shadow;
    web_prefs->text_track_font_family = style->font_family;
    web_prefs->text_track_font_variant = style->font_variant;
    web_prefs->text_track_window_color = style->window_color;
    web_prefs->text_track_window_padding = style->window_padding;
    web_prefs->text_track_window_radius = style->window_radius;
  }

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

  // The group policy NTP URL handler must be registered before the other NTP
  // URL handlers below.
  handler->AddHandlerPair(&HandleNewTabPageLocationOverride,
                          BrowserURLHandler::null_handler());

#if defined(OS_ANDROID)
  // Handler to rewrite chrome://newtab on Android.
  handler->AddHandlerPair(&chrome::android::HandleAndroidNativePageURL,
                          BrowserURLHandler::null_handler());
#else   // defined(OS_ANDROID)
  // Handler to rewrite chrome://newtab for InstantExtended.
  handler->AddHandlerPair(&search::HandleNewTabURLRewrite,
                          &search::HandleNewTabURLReverseRewrite);
#endif  // defined(OS_ANDROID)

  // chrome: & friends.
  handler->AddHandlerPair(&ChromeContentBrowserClient::HandleWebUI,
                          &ChromeContentBrowserClient::HandleWebUIReverse);
}

base::FilePath ChromeContentBrowserClient::GetDefaultDownloadDirectory() {
  return DownloadPrefs::GetDefaultDownloadDirectory();
}

std::string ChromeContentBrowserClient::GetDefaultDownloadName() {
  return l10n_util::GetStringUTF8(IDS_DEFAULT_DOWNLOAD_FILENAME);
}

base::FilePath ChromeContentBrowserClient::GetFontLookupTableCacheDir() {
  base::FilePath user_data_dir;
  base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
  DCHECK(!user_data_dir.empty());
  return user_data_dir.Append(FILE_PATH_LITERAL("FontLookupTableCache"));
}

base::FilePath ChromeContentBrowserClient::GetShaderDiskCacheDirectory() {
  base::FilePath user_data_dir;
  base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
  DCHECK(!user_data_dir.empty());
  return user_data_dir.Append(FILE_PATH_LITERAL("ShaderCache"));
}

base::FilePath ChromeContentBrowserClient::GetGrShaderDiskCacheDirectory() {
  base::FilePath user_data_dir;
  base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
  DCHECK(!user_data_dir.empty());
  return user_data_dir.Append(FILE_PATH_LITERAL("GrShaderCache"));
}

void ChromeContentBrowserClient::DidCreatePpapiPlugin(
    content::BrowserPpapiHost* browser_host) {
#if BUILDFLAG(ENABLE_PLUGINS)
  ChromeContentBrowserClientPluginsPart::DidCreatePpapiPlugin(browser_host);
#endif
}

content::BrowserPpapiHost*
ChromeContentBrowserClient::GetExternalBrowserPpapiHost(int plugin_process_id) {
#if BUILDFLAG(ENABLE_NACL)
  content::BrowserChildProcessHostIterator iter(PROCESS_TYPE_NACL_LOADER);
  while (!iter.Done()) {
    nacl::NaClProcessHost* host =
        static_cast<nacl::NaClProcessHost*>(iter.GetDelegate());
    if (host->process() && host->process()->GetData().id == plugin_process_id) {
      // Found the plugin.
      return host->browser_ppapi_host();
    }
    ++iter;
  }
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
      browser_context, url, private_api, params);
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

std::unique_ptr<ui::SelectFilePolicy>
ChromeContentBrowserClient::CreateSelectFilePolicy(WebContents* web_contents) {
  return std::make_unique<ChromeSelectFilePolicy>(web_contents);
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
  *schemes = secure_origin_whitelist::GetSchemesBypassingSecureContextCheck();
}

void ChromeContentBrowserClient::GetURLRequestAutoMountHandlers(
    std::vector<storage::URLRequestAutoMountHandler>* handlers) {
  for (size_t i = 0; i < extra_parts_.size(); ++i)
    extra_parts_[i]->GetURLRequestAutoMountHandlers(handlers);
}

::rappor::RapporService* ChromeContentBrowserClient::GetRapporService() {
  return g_browser_process->rappor_service();
}

void ChromeContentBrowserClient::GetAdditionalFileSystemBackends(
    content::BrowserContext* browser_context,
    const base::FilePath& storage_partition_path,
    std::vector<std::unique_ptr<storage::FileSystemBackend>>*
        additional_backends) {
#if defined(OS_CHROMEOS)
  storage::ExternalMountPoints* external_mount_points =
      content::BrowserContext::GetMountPoints(browser_context);
  DCHECK(external_mount_points);
  auto backend = std::make_unique<chromeos::FileSystemBackend>(
      std::make_unique<chromeos::file_system_provider::BackendDelegate>(),
      std::make_unique<chromeos::MTPFileSystemBackendDelegate>(
          storage_partition_path),
      std::make_unique<arc::ArcContentFileSystemBackendDelegate>(),
      std::make_unique<arc::ArcDocumentsProviderBackendDelegate>(),
      std::make_unique<drive::DriveFsFileSystemBackendDelegate>(
          Profile::FromBrowserContext(browser_context)),
      std::make_unique<chromeos::smb_client::SmbFsFileSystemBackendDelegate>(
          Profile::FromBrowserContext(browser_context)),
      external_mount_points, storage::ExternalMountPoints::GetSystemInstance());
  backend->AddSystemMountPoints();
  DCHECK(backend->CanHandleType(storage::kFileSystemTypeExternal));
  additional_backends->push_back(std::move(backend));
#endif

  for (size_t i = 0; i < extra_parts_.size(); ++i) {
    extra_parts_[i]->GetAdditionalFileSystemBackends(
        browser_context, storage_partition_path, additional_backends);
  }
}

#if defined(OS_POSIX) && !defined(OS_MACOSX)
void ChromeContentBrowserClient::GetAdditionalMappedFilesForChildProcess(
    const base::CommandLine& command_line,
    int child_process_id,
    PosixFileDescriptorInfo* mappings) {
#if defined(OS_ANDROID)
  base::MemoryMappedFile::Region region;
  int fd = ui::GetMainAndroidPackFd(&region);
  mappings->ShareWithRegion(kAndroidUIResourcesPakDescriptor, fd, region);

  // For Android: Native resources for DFMs should only be used by the browser
  // process. Their file descriptors and memory mapped file regions are not
  // passed to child processes.

  fd = ui::GetCommonResourcesPackFd(&region);
  mappings->ShareWithRegion(kAndroidChrome100PercentPakDescriptor, fd, region);

  fd = ui::GetLocalePackFd(&region);
  mappings->ShareWithRegion(kAndroidLocalePakDescriptor, fd, region);

  // Optional secondary locale .pak file.
  fd = ui::GetSecondaryLocalePackFd(&region);
  if (fd != -1) {
    mappings->ShareWithRegion(kAndroidSecondaryLocalePakDescriptor, fd, region);
  }

  base::FilePath app_data_path;
  base::PathService::Get(base::DIR_ANDROID_APP_DATA, &app_data_path);
  DCHECK(!app_data_path.empty());
#endif  // defined(OS_ANDROID)
  int crash_signal_fd = GetCrashSignalFD(command_line);
  if (crash_signal_fd >= 0) {
    mappings->Share(service_manager::kCrashDumpSignal, crash_signal_fd);
  }
}
#endif  // defined(OS_POSIX) && !defined(OS_MACOSX)

#if defined(OS_WIN)
base::string16 ChromeContentBrowserClient::GetAppContainerSidForSandboxType(
    service_manager::SandboxType sandbox_type) {
  // TODO(wfh): Add support for more process types here. crbug.com/499523
  switch (sandbox_type) {
    case service_manager::SandboxType::kRenderer:
      return base::string16(install_static::GetSandboxSidPrefix()) +
             L"129201922";
    case service_manager::SandboxType::kUtility:
      return base::string16();
    case service_manager::SandboxType::kGpu:
      return base::string16();
    case service_manager::SandboxType::kPpapi:
      return base::string16(install_static::GetSandboxSidPrefix()) +
             L"129201925";
    case service_manager::SandboxType::kNoSandbox:
    case service_manager::SandboxType::kNoSandboxAndElevatedPrivileges:
    case service_manager::SandboxType::kXrCompositing:
    case service_manager::SandboxType::kNetwork:
    case service_manager::SandboxType::kCdm:
    case service_manager::SandboxType::kPrintCompositor:
    case service_manager::SandboxType::kAudio:
    case service_manager::SandboxType::kSpeechRecognition:
    case service_manager::SandboxType::kProxyResolver:
    case service_manager::SandboxType::kPdfConversion:
    case service_manager::SandboxType::kSharingService:
    case service_manager::SandboxType::kVideoCapture:
      // Should never reach here.
      CHECK(0);
      return base::string16();
  }
}

bool ChromeContentBrowserClient::PreSpawnRenderer(sandbox::TargetPolicy* policy,
                                                  RendererSpawnFlags flags) {
// Does not work under component build because all the component DLLs would need
// to be manually added and maintained. Does not work under ASAN build because
// ASAN has not yet fully initialized its instrumentation by the time the CIG
// intercepts run.
#if !defined(COMPONENT_BUILD) && !defined(ADDRESS_SANITIZER)
  if ((flags & RendererSpawnFlags::RENDERER_CODE_INTEGRITY) == 0)
    return true;
  if (!base::FeatureList::IsEnabled(kRendererCodeIntegrity))
    return true;

  // Only enable signing mitigation if launching from chrome.exe.
  base::FilePath exe_path;
  if (!base::PathService::Get(base::FILE_EXE, &exe_path))
    return true;
  if (chrome::kBrowserProcessExecutableName != exe_path.BaseName().value())
    return true;

  sandbox::MitigationFlags mitigations = policy->GetProcessMitigations();
  mitigations |= sandbox::MITIGATION_FORCE_MS_SIGNED_BINS;
  sandbox::ResultCode result = policy->SetProcessMitigations(mitigations);
  if (result != sandbox::SBOX_ALL_OK)
    return false;

  // Allow loading Chrome's DLLs.
  for (const auto* dll : {chrome::kBrowserResourcesDll, chrome::kElfDll}) {
    result = policy->AddRule(sandbox::TargetPolicy::SUBSYS_SIGNED_BINARY,
                             sandbox::TargetPolicy::SIGNED_ALLOW_LOAD,
                             GetModulePath(dll).value().c_str());
    if (result != sandbox::SBOX_ALL_OK)
      return false;
  }
#endif  // !defined(COMPONENT_BUILD) && !defined(ADDRESS_SANITIZER)

  return true;
}

bool ChromeContentBrowserClient::IsRendererCodeIntegrityEnabled() {
  PrefService* local_state = g_browser_process->local_state();
  if (local_state &&
      local_state->HasPrefPath(prefs::kRendererCodeIntegrityEnabled) &&
      !local_state->GetBoolean(prefs::kRendererCodeIntegrityEnabled))
    return false;
  return true;
}

#endif  // defined(OS_WIN)

#if defined(OS_WIN) || defined(OS_MACOSX) || \
    (defined(OS_LINUX) && !defined(OS_CHROMEOS))
bool ShouldEnableAudioSandbox(const policy::PolicyMap& policies) {
  const base::Value* audio_sandbox_enabled_policy_value =
      policies.GetValue(policy::key::kAudioSandboxEnabled);
  if (audio_sandbox_enabled_policy_value) {
    bool force_enable_audio_sandbox;
    audio_sandbox_enabled_policy_value->GetAsBoolean(
        &force_enable_audio_sandbox);
    return force_enable_audio_sandbox;
  }

  return base::FeatureList::IsEnabled(
      service_manager::features::kAudioServiceSandbox);
}
#endif

void ChromeContentBrowserClient::WillStartServiceManager() {
#if defined(OS_WIN) || defined(OS_MACOSX) || \
    (defined(OS_LINUX) && !defined(OS_CHROMEOS))
  if (startup_data_) {
    auto* chrome_feature_list_creator =
        startup_data_->chrome_feature_list_creator();
    // This has to run very early before ServiceManagerContext is created.
    const policy::PolicyMap& policies =
        chrome_feature_list_creator->browser_policy_connector()
            ->GetPolicyService()
            ->GetPolicies(policy::PolicyNamespace(policy::POLICY_DOMAIN_CHROME,
                                                  std::string()));

    service_manager::EnableAudioSandbox(ShouldEnableAudioSandbox(policies));
  }
#endif
}

base::Optional<service_manager::Manifest>
ChromeContentBrowserClient::GetServiceManifestOverlay(base::StringPiece name) {
  if (name == content::mojom::kBrowserServiceName)
    return GetChromeContentBrowserOverlayManifest();
  return base::nullopt;
}

void ChromeContentBrowserClient::OpenURL(
    content::SiteInstance* site_instance,
    const content::OpenURLParams& params,
    base::OnceCallback<void(content::WebContents*)> callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  content::BrowserContext* browser_context = site_instance->GetBrowserContext();

#if defined(OS_ANDROID)
  ServiceTabLauncher::GetInstance()->LaunchTab(browser_context, params,
                                               std::move(callback));
#else
  NavigateParams nav_params(Profile::FromBrowserContext(browser_context),
                            params.url, params.transition);
  nav_params.FillNavigateParamsFromOpenURLParams(params);

  Navigate(&nav_params);
  std::move(callback).Run(nav_params.navigated_or_inserted_contents);
#endif
}

content::ControllerPresentationServiceDelegate*
ChromeContentBrowserClient::GetControllerPresentationServiceDelegate(
    content::WebContents* web_contents) {
  if (media_router::MediaRouterEnabled(web_contents->GetBrowserContext())) {
    return media_router::PresentationServiceDelegateImpl::
        GetOrCreateForWebContents(web_contents);
  }
  return nullptr;
}

content::ReceiverPresentationServiceDelegate*
ChromeContentBrowserClient::GetReceiverPresentationServiceDelegate(
    content::WebContents* web_contents) {
  if (media_router::MediaRouterEnabled(web_contents->GetBrowserContext())) {
    // ReceiverPresentationServiceDelegateImpl exists only for WebContents
    // created for offscreen presentations. The WebContents must belong to
    // an incognito profile.
    if (auto* impl = media_router::ReceiverPresentationServiceDelegateImpl::
            FromWebContents(web_contents)) {
      DCHECK(web_contents->GetBrowserContext()->IsOffTheRecord());
      return impl;
    }
  }
  return nullptr;
}

void ChromeContentBrowserClient::RecordURLMetric(const std::string& metric,
                                                 const GURL& url) {
  if (url.is_valid()) {
    rappor::SampleDomainAndRegistryFromGURL(g_browser_process->rappor_service(),
                                            metric, url);
  }
}

std::string ChromeContentBrowserClient::GetMetricSuffixForURL(const GURL& url) {
  // Don't change these returned strings. They are written (in hashed form) into
  // UMA logs. If you add more strings, you must update histograms.xml and get
  // histograms review. Only Google domains should be here for privacy purposes.
  // TODO(falken): Ideally Chrome would log the relevant UMA directly and this
  // function could be removed.
  if (page_load_metrics::IsGoogleSearchResultUrl(url))
    return "search";
  if (url.host() == "docs.google.com")
    return "docs";
  return std::string();
}

std::vector<std::unique_ptr<content::NavigationThrottle>>
ChromeContentBrowserClient::CreateThrottlesForNavigation(
    content::NavigationHandle* handle) {
  std::vector<std::unique_ptr<content::NavigationThrottle>> throttles;

  // MetricsNavigationThrottle requires that it runs before NavigationThrottles
  // that may delay or cancel navigations, so only NavigationThrottles that
  // don't delay or cancel navigations (e.g. throttles that are only observing
  // callbacks without affecting navigation behavior) should be added before
  // MetricsNavigationThrottle.
  if (handle->IsInMainFrame()) {
    throttles.push_back(
        page_load_metrics::MetricsNavigationThrottle::Create(handle));
  }

#if BUILDFLAG(ENABLE_PLUGINS)
  MaybeAddThrottle(FlashDownloadInterception::MaybeCreateThrottleFor(handle),
                   &throttles);
#endif

#if defined(OS_CHROMEOS)
  MaybeAddThrottle(
      chromeos::WebTimeLimitNavigationThrottle::MaybeCreateThrottleFor(handle),
      &throttles);
#endif  // defined(OS_CHROMEOS)

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
  MaybeAddThrottle(
      SupervisedUserNavigationThrottle::MaybeCreateThrottleFor(handle),
      &throttles);
#endif

#if defined(OS_ANDROID)
  // TODO(davidben): This is insufficient to integrate with prerender properly.
  // https://crbug.com/370595
  prerender::PrerenderContents* prerender_contents =
      prerender::PrerenderContents::FromWebContents(handle->GetWebContents());
  if (!prerender_contents && handle->IsInMainFrame()) {
    throttles.push_back(
        navigation_interception::InterceptNavigationDelegate::CreateThrottleFor(
            handle, navigation_interception::SynchronyMode::kAsync));
  }
  throttles.push_back(InterceptOMADownloadNavigationThrottle::Create(handle));

#if BUILDFLAG(DFMIFY_DEV_UI)
  // If the DevUI DFM is already installed, then this is a no-op, except for the
  // side effect of ensuring that the DevUI DFM is loaded.
  MaybeAddThrottle(dev_ui::DevUiLoaderThrottle::MaybeCreateThrottleFor(handle),
                   &throttles);
#endif  // BUILDFLAG(DFMIFY_DEV_UI)

#elif BUILDFLAG(ENABLE_EXTENSIONS)
  if (handle->IsInMainFrame()) {
    // Redirect some navigations to apps that have registered matching URL
    // handlers ('url_handlers' in the manifest).
    MaybeAddThrottle(
        PlatformAppNavigationRedirector::MaybeCreateThrottleFor(handle),
        &throttles);
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
  }
#endif

#if !defined(OS_ANDROID)
  auto url_to_apps_throttle =
#if defined(OS_CHROMEOS)
      base::FeatureList::IsEnabled(features::kAppServiceIntentHandling)
          ? apps::CommonAppsNavigationThrottle::MaybeCreate(handle)
          : chromeos::ChromeOsAppsNavigationThrottle::MaybeCreate(handle);
#elif defined(OS_MACOSX)
      apps::MacAppsNavigationThrottle::MaybeCreate(handle);
#else
      apps::AppsNavigationThrottle::MaybeCreate(handle);
#endif
  if (url_to_apps_throttle)
    throttles.push_back(std::move(url_to_apps_throttle));
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
  throttles.push_back(
      std::make_unique<extensions::ExtensionNavigationThrottle>(handle));

  MaybeAddThrottle(extensions::ExtensionsBrowserClient::Get()
                       ->GetUserScriptListener()
                       ->CreateNavigationThrottle(handle),
                   &throttles);
#endif

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
  MaybeAddThrottle(
      SupervisedUserGoogleAuthNavigationThrottle::MaybeCreate(handle),
      &throttles);
#endif

  content::WebContents* web_contents = handle->GetWebContents();
  if (auto* subresource_filter_client =
          ChromeSubresourceFilterClient::FromWebContents(web_contents)) {
    subresource_filter_client->MaybeAppendNavigationThrottles(handle,
                                                              &throttles);
  }

#if !defined(OS_ANDROID)
  // BackgroundTabNavigationThrottle is used by TabManager, which is only
  // enabled on non-Android platforms.
  MaybeAddThrottle(resource_coordinator::BackgroundTabNavigationThrottle::
                       MaybeCreateThrottleFor(handle),
                   &throttles);
#endif

#if BUILDFLAG(FULL_SAFE_BROWSING)
  MaybeAddThrottle(safe_browsing::MaybeCreateNavigationThrottle(handle),
                   &throttles);
#endif

  MaybeAddThrottle(
      LookalikeUrlNavigationThrottle::MaybeCreateNavigationThrottle(handle),
      &throttles);

  MaybeAddThrottle(PDFIFrameNavigationThrottle::MaybeCreateThrottleFor(handle),
                   &throttles);

  MaybeAddThrottle(TabUnderNavigationThrottle::MaybeCreate(handle), &throttles);

  throttles.push_back(std::make_unique<PolicyBlacklistNavigationThrottle>(
      handle, handle->GetWebContents()->GetBrowserContext()));

  // Before setting up SSL error detection, configure SSLErrorHandler to invoke
  // the relevant extension API whenever an SSL interstitial is shown.
  SSLErrorHandler::SetClientCallbackOnInterstitialsShown(
      base::BindRepeating(&MaybeTriggerSecurityInterstitialShownEvent));
  throttles.push_back(std::make_unique<SSLErrorNavigationThrottle>(
      handle,
      std::make_unique<CertificateReportingServiceCertReporter>(web_contents),
      base::BindOnce(&HandleSSLErrorWrapper), base::BindOnce(&IsInHostedApp)));

  throttles.push_back(std::make_unique<LoginNavigationThrottle>(handle));

  MaybeAddThrottle(
      TypedNavigationTimingThrottle::MaybeCreateThrottleFor(handle),
      &throttles);

#if !defined(OS_ANDROID)
  MaybeAddThrottle(DevToolsWindow::MaybeCreateNavigationThrottle(handle),
                   &throttles);

  MaybeAddThrottle(NewTabPageNavigationThrottle::MaybeCreateThrottleFor(handle),
                   &throttles);

  MaybeAddThrottle(
      GooglePasswordManagerNavigationThrottle::MaybeCreateThrottleFor(handle),
      &throttles);
#endif

  throttles.push_back(
      std::make_unique<safe_browsing::SafeBrowsingNavigationThrottle>(handle));

  if (base::FeatureList::IsEnabled(safe_browsing::kDelayedWarnings)) {
    throttles.push_back(
        std::make_unique<safe_browsing::DelayedWarningNavigationThrottle>(
            handle));
  }

#if defined(OS_WIN) || defined(OS_MACOSX) || \
    (defined(OS_LINUX) && !defined(OS_CHROMEOS))
  MaybeAddThrottle(browser_switcher::BrowserSwitcherNavigationThrottle::
                       MaybeCreateThrottleFor(handle),
                   &throttles);
#endif

#if defined(OS_CHROMEOS)
  MaybeAddThrottle(
      chromeos::KioskSettingsNavigationThrottle::MaybeCreateThrottleFor(handle),
      &throttles);
#endif

  auto* performance_manager_registry =
      performance_manager::PerformanceManagerRegistry::GetInstance();
  if (performance_manager_registry) {
    MaybeAddThrottles(
        performance_manager_registry->CreateThrottlesForNavigation(handle),
        &throttles);
  }

  return throttles;
}

std::unique_ptr<content::NavigationUIData>
ChromeContentBrowserClient::GetNavigationUIData(
    content::NavigationHandle* navigation_handle) {
  return std::make_unique<ChromeNavigationUIData>(navigation_handle);
}

void ChromeContentBrowserClient::GetHardwareSecureDecryptionCaps(
    const std::string& key_system,
    base::flat_set<media::VideoCodec>* video_codecs,
    base::flat_set<media::EncryptionScheme>* encryption_schemes) {
#if defined(OS_WIN) && BUILDFLAG(ENABLE_LIBRARY_CDMS) && \
    BUILDFLAG(ENABLE_WIDEVINE)
  if (key_system == kWidevineKeySystem) {
    GetWidevineHardwareCaps(video_codecs, encryption_schemes);
  }
#endif
}

content::DevToolsManagerDelegate*
ChromeContentBrowserClient::GetDevToolsManagerDelegate() {
#if defined(OS_ANDROID)
  return new DevToolsManagerDelegateAndroid();
#else
  return new ChromeDevToolsManagerDelegate();
#endif
}

void ChromeContentBrowserClient::UpdateDevToolsBackgroundServiceExpiration(
    content::BrowserContext* browser_context,
    int service,
    base::Time expiration_time) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  DCHECK(profile);

  auto* pref_service = profile->GetPrefs();
  DCHECK(pref_service);

  DictionaryPrefUpdate pref_update(
      pref_service, prefs::kDevToolsBackgroundServicesExpirationDict);
  base::DictionaryValue* exp_dict = pref_update.Get();

  // Convert |expiration_time| to minutes since that is the most granular
  // option that returns an int. base::Value does not accept int64.
  int expiration_time_minutes =
      expiration_time.ToDeltaSinceWindowsEpoch().InMinutes();
  exp_dict->SetInteger(base::NumberToString(service), expiration_time_minutes);
}

base::flat_map<int, base::Time>
ChromeContentBrowserClient::GetDevToolsBackgroundServiceExpirations(
    content::BrowserContext* browser_context) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  DCHECK(profile);

  auto* pref_service = profile->GetPrefs();
  DCHECK(pref_service);

  auto* expiration_dict = pref_service->GetDictionary(
      prefs::kDevToolsBackgroundServicesExpirationDict);
  DCHECK(expiration_dict);

  base::flat_map<int, base::Time> expiration_times;
  for (const auto& it : *expiration_dict) {
    // key.
    int service = 0;
    bool did_convert = base::StringToInt(it.first, &service);
    DCHECK(did_convert);

    // value.
    DCHECK(it.second->is_int());
    base::TimeDelta delta = base::TimeDelta::FromMinutes(it.second->GetInt());
    base::Time expiration_time = base::Time::FromDeltaSinceWindowsEpoch(delta);

    expiration_times[service] = expiration_time;
  }

  return expiration_times;
}

content::TracingDelegate* ChromeContentBrowserClient::GetTracingDelegate() {
  return new ChromeTracingDelegate();
}

bool ChromeContentBrowserClient::IsPluginAllowedToCallRequestOSFileHandle(
    content::BrowserContext* browser_context,
    const GURL& url) {
#if BUILDFLAG(ENABLE_PLUGINS) && BUILDFLAG(ENABLE_EXTENSIONS)
  return ChromeContentBrowserClientPluginsPart::
      IsPluginAllowedToCallRequestOSFileHandle(browser_context, url);
#else
  return false;
#endif
}

bool ChromeContentBrowserClient::IsPluginAllowedToUseDevChannelAPIs(
    content::BrowserContext* browser_context,
    const GURL& url) {
#if BUILDFLAG(ENABLE_PLUGINS) && BUILDFLAG(ENABLE_EXTENSIONS)
  return ChromeContentBrowserClientPluginsPart::
      IsPluginAllowedToUseDevChannelAPIs(browser_context, url);
#else
  return false;
#endif
}

void ChromeContentBrowserClient::OverridePageVisibilityState(
    RenderFrameHost* render_frame_host,
    content::PageVisibilityState* visibility_state) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  WebContents* web_contents =
      WebContents::FromRenderFrameHost(render_frame_host);
  DCHECK(web_contents);

  prerender::PrerenderManager* prerender_manager =
      prerender::PrerenderManagerFactory::GetForBrowserContext(
          web_contents->GetBrowserContext());
  if (prerender_manager &&
      prerender_manager->IsWebContentsPrerendering(web_contents, nullptr)) {
    *visibility_state = content::PageVisibilityState::kHiddenButPainting;
  }
}

void ChromeContentBrowserClient::InitNetworkContextsParentDirectory() {
  base::FilePath user_data_dir;
  base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
  DCHECK(!user_data_dir.empty());
  network_contexts_parent_directory_.push_back(user_data_dir);

  base::FilePath cache_dir;
  chrome::GetUserCacheDirectory(user_data_dir, &cache_dir);
  DCHECK(!cache_dir.empty());
  // On some platforms, the cache is a child of the user_data_dir so only
  // return the one path.
  if (!user_data_dir.IsParent(cache_dir))
    network_contexts_parent_directory_.push_back(cache_dir);

  // If the cache location has been overridden by a switch or preference,
  // include that as well.
  if (auto* local_state = g_browser_process->local_state()) {
    base::FilePath pref_cache_dir =
        local_state->GetFilePath(prefs::kDiskCacheDir);
    if (!pref_cache_dir.empty() && !user_data_dir.IsParent(cache_dir))
      network_contexts_parent_directory_.push_back(pref_cache_dir);
  }
}

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
    to_command_line->CopySwitchesFrom(from_command_line, kWebRtcDevSwitchNames,
                                      base::size(kWebRtcDevSwitchNames));
  }
}

#if BUILDFLAG(ENABLE_MEDIA_REMOTING)
void ChromeContentBrowserClient::CreateMediaRemoter(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingRemote<media::mojom::RemotingSource> source,
    mojo::PendingReceiver<media::mojom::Remoter> receiver) {
  CastRemotingConnector::CreateMediaRemoter(
      render_frame_host, std::move(source), std::move(receiver));
}
#endif  // BUILDFLAG(ENABLE_MEDIA_REMOTING)

base::FilePath ChromeContentBrowserClient::GetLoggingFileName(
    const base::CommandLine& command_line) {
  return logging::GetLogFileName(command_line);
}

namespace {
// TODO(jam): move this to a separate file.
class ProtocolHandlerThrottle : public blink::URLLoaderThrottle {
 public:
  explicit ProtocolHandlerThrottle(
      ProtocolHandlerRegistry* protocol_handler_registry)
      : protocol_handler_registry_(protocol_handler_registry) {}
  ~ProtocolHandlerThrottle() override = default;

  void WillStartRequest(network::ResourceRequest* request,
                        bool* defer) override {
    TranslateUrl(&request->url);
  }

  void WillRedirectRequest(
      net::RedirectInfo* redirect_info,
      const network::mojom::URLResponseHead& response_head,
      bool* defer,
      std::vector<std::string>* to_be_removed_headers,
      net::HttpRequestHeaders* modified_headers,
      net::HttpRequestHeaders* modified_cors_exempt_headers) override {
    TranslateUrl(&redirect_info->new_url);
  }

 private:
  void TranslateUrl(GURL* url) {
    if (!protocol_handler_registry_->IsHandledProtocol(url->scheme()))
      return;
    GURL translated_url = protocol_handler_registry_->Translate(*url);
    if (!translated_url.is_empty())
      *url = translated_url;
  }

  ProtocolHandlerRegistry* protocol_handler_registry_;
};
}  // namespace

std::vector<std::unique_ptr<blink::URLLoaderThrottle>>
ChromeContentBrowserClient::CreateURLLoaderThrottles(
    const network::ResourceRequest& request,
    content::BrowserContext* browser_context,
    const base::RepeatingCallback<content::WebContents*()>& wc_getter,
    content::NavigationUIData* navigation_ui_data,
    int frame_tree_node_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  std::vector<std::unique_ptr<blink::URLLoaderThrottle>> result;

  Profile* profile = Profile::FromBrowserContext(browser_context);

  ChromeNavigationUIData* chrome_navigation_ui_data =
      static_cast<ChromeNavigationUIData*>(navigation_ui_data);

#if BUILDFLAG(SAFE_BROWSING_DB_LOCAL) || BUILDFLAG(SAFE_BROWSING_DB_REMOTE)
  bool matches_enterprise_whitelist = safe_browsing::IsURLWhitelistedByPolicy(
      request.url, *profile->GetPrefs());
  if (!matches_enterprise_whitelist) {
    // |url_lookup_service| is used when real time url check is enabled.
    safe_browsing::RealTimeUrlLookupService* url_lookup_service =
        // |safe_browsing_service_| may be unavailable in tests.
        safe_browsing_service_ &&
                safe_browsing::RealTimePolicyEngine::CanPerformFullURLLookup(
                    profile->GetPrefs(), profile->IsOffTheRecord())
            ? safe_browsing::RealTimeUrlLookupServiceFactory::GetForProfile(
                  profile)
            : nullptr;

    result.push_back(safe_browsing::BrowserURLLoaderThrottle::Create(
        base::BindOnce(
            &ChromeContentBrowserClient::GetSafeBrowsingUrlCheckerDelegate,
            base::Unretained(this),
            safe_browsing::IsSafeBrowsingEnabled(*profile->GetPrefs())),
        wc_getter, frame_tree_node_id,
        url_lookup_service ? url_lookup_service->GetWeakPtr() : nullptr));
  }
#endif

#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
  result.push_back(
      std::make_unique<captive_portal::CaptivePortalURLLoaderThrottle>(
          wc_getter.Run()));
#endif

  if (chrome_navigation_ui_data &&
      chrome_navigation_ui_data->prerender_mode() != prerender::NO_PRERENDER) {
    result.push_back(std::make_unique<prerender::PrerenderURLLoaderThrottle>(
        chrome_navigation_ui_data->prerender_mode(),
        chrome_navigation_ui_data->prerender_histogram_prefix(),
        GetPrerenderCanceler(wc_getter)));
  }

#if defined(OS_ANDROID)
  std::string client_data_header;
  if (frame_tree_node_id != content::RenderFrameHost::kNoFrameTreeNodeId) {
    auto* web_contents = WebContents::FromFrameTreeNodeId(frame_tree_node_id);
    // Could be null if the FrameTreeNode's RenderFrameHost is shutting down.
    if (web_contents) {
      auto* client_data_header_observer =
          customtabs::ClientDataHeaderWebContentsObserver::FromWebContents(
              web_contents);
      if (client_data_header_observer)
        client_data_header = client_data_header_observer->header();
    }
  }
#endif

  chrome::mojom::DynamicParams dynamic_params = {
      profile->GetPrefs()->GetBoolean(prefs::kForceGoogleSafeSearch),
      profile->GetPrefs()->GetInteger(prefs::kForceYouTubeRestrict),
      profile->GetPrefs()->GetString(prefs::kAllowedDomainsForApps)};
  result.push_back(std::make_unique<GoogleURLLoaderThrottle>(
#if defined(OS_ANDROID)
      client_data_header,
#endif
      std::move(dynamic_params)));

  result.push_back(std::make_unique<ProtocolHandlerThrottle>(
      ProtocolHandlerRegistryFactory::GetForBrowserContext(browser_context)));

#if BUILDFLAG(ENABLE_PLUGINS)
  result.push_back(std::make_unique<PluginResponseInterceptorURLLoaderThrottle>(
      request.resource_type, frame_tree_node_id));
#endif

  auto delegate =
      std::make_unique<signin::HeaderModificationDelegateImpl>(profile);
  auto signin_throttle =
      signin::URLLoaderThrottle::MaybeCreate(std::move(delegate), wc_getter);
  if (signin_throttle)
    result.push_back(std::move(signin_throttle));

  return result;
}

void ChromeContentBrowserClient::RegisterNonNetworkNavigationURLLoaderFactories(
    int frame_tree_node_id,
    NonNetworkURLLoaderFactoryMap* factories) {
#if BUILDFLAG(ENABLE_EXTENSIONS) || defined(OS_CHROMEOS)
  content::WebContents* web_contents =
      content::WebContents::FromFrameTreeNodeId(frame_tree_node_id);
#if BUILDFLAG(ENABLE_EXTENSIONS)
  factories->emplace(
      extensions::kExtensionScheme,
      extensions::CreateExtensionNavigationURLLoaderFactory(
          web_contents->GetBrowserContext(),
          !!extensions::WebViewGuest::FromWebContents(web_contents)));
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
#if defined(OS_CHROMEOS)
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  factories->emplace(content::kExternalFileScheme,
                     std::make_unique<chromeos::ExternalFileURLLoaderFactory>(
                         profile, content::ChildProcessHost::kInvalidUniqueID));
#endif  // defined(OS_CHROMEOS)
#endif  // BUILDFLAG(ENABLE_EXTENSIONS) || defined(OS_CHROMEOS)
}

void ChromeContentBrowserClient::
    RegisterNonNetworkWorkerMainResourceURLLoaderFactories(
        content::BrowserContext* browser_context,
        NonNetworkURLLoaderFactoryMap* factories) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  DCHECK(browser_context);
  DCHECK(factories);
  factories->emplace(
      extensions::kExtensionScheme,
      extensions::CreateExtensionWorkerMainResourceURLLoaderFactory(
          browser_context));
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
}

void ChromeContentBrowserClient::
    RegisterNonNetworkServiceWorkerUpdateURLLoaderFactories(
        content::BrowserContext* browser_context,
        NonNetworkURLLoaderFactoryMap* factories) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  DCHECK(browser_context);
  DCHECK(factories);
  factories->emplace(
      extensions::kExtensionScheme,
      extensions::CreateExtensionServiceWorkerScriptURLLoaderFactory(
          browser_context));
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
}

namespace {

// The FileURLLoaderFactory provided to the extension background pages.
// Checks with the ChildProcessSecurityPolicy to validate the file access.
class FileURLLoaderFactory : public network::mojom::URLLoaderFactory {
 public:
  explicit FileURLLoaderFactory(int child_id) : child_id_(child_id) {}

 private:
  // network::mojom::URLLoaderFactory:
  void CreateLoaderAndStart(
      mojo::PendingReceiver<network::mojom::URLLoader> loader,
      int32_t routing_id,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
      override {
    if (!content::ChildProcessSecurityPolicy::GetInstance()->CanRequestURL(
            child_id_, request.url)) {
      mojo::Remote<network::mojom::URLLoaderClient>(std::move(client))
          ->OnComplete(
              network::URLLoaderCompletionStatus(net::ERR_ACCESS_DENIED));
      return;
    }
    content::CreateFileURLLoaderBypassingSecurityChecks(
        request, std::move(loader), std::move(client),
        /*observer=*/nullptr,
        /* allow_directory_listing */ true);
  }

  void Clone(
      mojo::PendingReceiver<network::mojom::URLLoaderFactory> loader) override {
    receivers_.Add(this, std::move(loader));
  }

  int child_id_;
  mojo::ReceiverSet<network::mojom::URLLoaderFactory> receivers_;
  DISALLOW_COPY_AND_ASSIGN(FileURLLoaderFactory);
};

#if defined(OS_CHROMEOS)
bool IsSystemFeatureDisabled(policy::SystemFeature system_feature) {
  PrefService* const local_state = g_browser_process->local_state();
  if (!local_state)  // Sometimes it's not available in tests.
    return false;

  const base::ListValue* disabled_system_features_pref =
      local_state->GetList(policy::policy_prefs::kSystemFeaturesDisableList);
  if (!disabled_system_features_pref)
    return false;

  const auto disabled_system_features =
      disabled_system_features_pref->GetList();
  return base::Contains(disabled_system_features, base::Value(system_feature));
}

bool IsSystemFeatureURLDisabled(const GURL& url) {
  if (!url.SchemeIs(content::kChromeUIScheme))
    return false;

  // chrome://os-settings/pwa.html shouldn't be replaced to let the settings app
  // installation complete successfully.
  if (url.DomainIs(chrome::kChromeUIOSSettingsHost) &&
      url.path() != "/pwa.html" &&
      IsSystemFeatureDisabled(policy::SystemFeature::OS_SETTINGS)) {
    return true;
  }

  if (url.DomainIs(chrome::kChromeUISettingsHost) &&
      IsSystemFeatureDisabled(policy::SystemFeature::BROWSER_SETTINGS)) {
    return true;
  }

  return false;
}
#endif
}  // namespace

void ChromeContentBrowserClient::
    RegisterNonNetworkSubresourceURLLoaderFactories(
        int render_process_id,
        int render_frame_id,
        NonNetworkURLLoaderFactoryMap* factories) {
#if defined(OS_CHROMEOS) || BUILDFLAG(ENABLE_EXTENSIONS)
  content::RenderFrameHost* frame_host =
      RenderFrameHost::FromID(render_process_id, render_frame_id);
  WebContents* web_contents = WebContents::FromRenderFrameHost(frame_host);
#endif  // defined(OS_CHROMEOS) || BUILDFLAG(ENABLE_EXTENSIONS)

#if defined(OS_CHROMEOS)
  if (web_contents) {
    Profile* profile =
        Profile::FromBrowserContext(web_contents->GetBrowserContext());
    factories->emplace(content::kExternalFileScheme,
                       std::make_unique<chromeos::ExternalFileURLLoaderFactory>(
                           profile, render_process_id));
  }
#endif  // defined(OS_CHROMEOS)

#if BUILDFLAG(ENABLE_EXTENSIONS)
  auto factory = extensions::CreateExtensionURLLoaderFactory(render_process_id,
                                                             render_frame_id);
  if (factory)
    factories->emplace(extensions::kExtensionScheme, std::move(factory));

  // This logic should match
  // ChromeExtensionWebContentsObserver::RenderFrameCreated.
  if (!web_contents)
    return;

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  InstantService* instant_service =
      InstantServiceFactory::GetForProfile(profile);
  // The test below matches what's done by ShouldServiceRequestIOThread in
  // local_ntp_source.cc.
  if (instant_service->IsInstantProcess(render_process_id)) {
    factories->emplace(
        chrome::kChromeSearchScheme,
        content::CreateWebUIURLLoader(
            frame_host, chrome::kChromeSearchScheme,
            /*allowed_webui_hosts=*/base::flat_set<std::string>()));
  }

  extensions::ChromeExtensionWebContentsObserver* web_observer =
      extensions::ChromeExtensionWebContentsObserver::FromWebContents(
          web_contents);

  // There is nothing to do if no ChromeExtensionWebContentsObserver is attached
  // to the |web_contents|.
  if (!web_observer)
    return;

  const Extension* extension =
      web_observer->GetExtensionFromFrame(frame_host, false);
  if (!extension)
    return;

  std::vector<std::string> allowed_webui_hosts;
  // Support for chrome:// scheme if appropriate.
  if ((extension->is_extension() || extension->is_platform_app()) &&
      Manifest::IsComponentLocation(extension->location())) {
    // Components of chrome that are implemented as extensions or platform apps
    // are allowed to use chrome://resources/ and chrome://theme/ URLs.
    allowed_webui_hosts.emplace_back(content::kChromeUIResourcesHost);
    allowed_webui_hosts.emplace_back(chrome::kChromeUIThemeHost);
  }
  if (extension->is_extension() || extension->is_legacy_packaged_app() ||
      (extension->is_platform_app() &&
       Manifest::IsComponentLocation(extension->location()))) {
    // Extensions, legacy packaged apps, and component platform apps are allowed
    // to use chrome://favicon/, chrome://extension-icon/ and chrome://app-icon
    // URLs. Hosted apps are not allowed because they are served via web servers
    // (and are generally never given access to Chrome APIs).
    allowed_webui_hosts.emplace_back(chrome::kChromeUIExtensionIconHost);
    allowed_webui_hosts.emplace_back(chrome::kChromeUIFaviconHost);
    allowed_webui_hosts.emplace_back(chrome::kChromeUIAppIconHost);
  }
  if (!allowed_webui_hosts.empty()) {
    factories->emplace(
        content::kChromeUIScheme,
        content::CreateWebUIURLLoader(frame_host, content::kChromeUIScheme,
                                      std::move(allowed_webui_hosts)));
  }

  // Extension with a background page get file access that gets approval from
  // ChildProcessSecurityPolicy.
  extensions::ExtensionHost* host =
      extensions::ProcessManager::Get(web_contents->GetBrowserContext())
          ->GetBackgroundHostForExtension(extension->id());
  if (host) {
    factories->emplace(url::kFileScheme, std::make_unique<FileURLLoaderFactory>(
                                             render_process_id));
  }
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
}

bool ChromeContentBrowserClient::WillCreateURLLoaderFactory(
    content::BrowserContext* browser_context,
    content::RenderFrameHost* frame,
    int render_process_id,
    URLLoaderFactoryType type,
    const url::Origin& request_initiator,
    base::Optional<int64_t> navigation_id,
    mojo::PendingReceiver<network::mojom::URLLoaderFactory>* factory_receiver,
    mojo::PendingRemote<network::mojom::TrustedURLLoaderHeaderClient>*
        header_client,
    bool* bypass_redirect_checks,
    bool* disable_secure_dns,
    network::mojom::URLLoaderFactoryOverridePtr* factory_override) {
  bool use_proxy = false;

#if BUILDFLAG(ENABLE_EXTENSIONS)
  auto* web_request_api =
      extensions::BrowserContextKeyedAPIFactory<extensions::WebRequestAPI>::Get(
          browser_context);

  // NOTE: Some unit test environments do not initialize
  // BrowserContextKeyedAPI factories for e.g. WebRequest.
  if (web_request_api) {
    bool use_proxy_for_web_request =
        web_request_api->MaybeProxyURLLoaderFactory(
            browser_context, frame, render_process_id, type,
            std::move(navigation_id), factory_receiver, header_client);
    if (bypass_redirect_checks)
      *bypass_redirect_checks = use_proxy_for_web_request;
    use_proxy |= use_proxy_for_web_request;
  }
#endif

  use_proxy |= signin::ProxyingURLLoaderFactory::MaybeProxyRequest(
      frame, type == URLLoaderFactoryType::kNavigation, request_initiator,
      factory_receiver);

#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
  if (disable_secure_dns) {
    WebContents* web_contents = WebContents::FromRenderFrameHost(frame);
    *disable_secure_dns =
        web_contents &&
        captive_portal::CaptivePortalTabHelper::FromWebContents(web_contents) &&
        captive_portal::CaptivePortalTabHelper::FromWebContents(web_contents)
            ->is_captive_portal_window();
  }
#endif

  return use_proxy;
}

std::vector<std::unique_ptr<content::URLLoaderRequestInterceptor>>
ChromeContentBrowserClient::WillCreateURLLoaderRequestInterceptors(
    content::NavigationUIData* navigation_ui_data,
    int frame_tree_node_id,
    const scoped_refptr<network::SharedURLLoaderFactory>&
        network_loader_factory) {
  std::vector<std::unique_ptr<content::URLLoaderRequestInterceptor>>
      interceptors;
#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
  interceptors.push_back(
      std::make_unique<offline_pages::OfflinePageURLLoaderRequestInterceptor>(
          navigation_ui_data, frame_tree_node_id));
#endif

  if (base::FeatureList::IsEnabled(features::kIsolatePrerenders)) {
    interceptors.push_back(
        std::make_unique<IsolatedPrerenderURLLoaderInterceptor>(
            frame_tree_node_id));
  }

  return interceptors;
}

bool ChromeContentBrowserClient::WillInterceptWebSocket(
    content::RenderFrameHost* frame) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (!frame) {
    return false;
  }
  const auto* web_request_api =
      extensions::BrowserContextKeyedAPIFactory<extensions::WebRequestAPI>::Get(
          frame->GetProcess()->GetBrowserContext());

  // NOTE: Some unit test environments do not initialize
  // BrowserContextKeyedAPI factories for e.g. WebRequest.
  if (!web_request_api)
    return false;

  return web_request_api->MayHaveProxies();
#else
  return false;
#endif
}

void ChromeContentBrowserClient::CreateWebSocket(
    content::RenderFrameHost* frame,
    WebSocketFactory factory,
    const GURL& url,
    const net::SiteForCookies& site_for_cookies,
    const base::Optional<std::string>& user_agent,
    mojo::PendingRemote<network::mojom::WebSocketHandshakeClient>
        handshake_client) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (!frame) {
    return;
  }
  auto* web_request_api =
      extensions::BrowserContextKeyedAPIFactory<extensions::WebRequestAPI>::Get(
          frame->GetProcess()->GetBrowserContext());

  DCHECK(web_request_api);
  web_request_api->ProxyWebSocket(frame, std::move(factory), url,
                                  site_for_cookies.RepresentativeUrl(),
                                  user_agent, std::move(handshake_client));
#endif
}

bool ChromeContentBrowserClient::WillCreateRestrictedCookieManager(
    network::mojom::RestrictedCookieManagerRole role,
    content::BrowserContext* browser_context,
    const url::Origin& origin,
    const net::SiteForCookies& site_for_cookies,
    const url::Origin& top_frame_origin,
    bool is_service_worker,
    int process_id,
    int routing_id,
    mojo::PendingReceiver<network::mojom::RestrictedCookieManager>* receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (origin.scheme() == extensions::kExtensionScheme) {
    DCHECK_EQ(network::mojom::RestrictedCookieManagerRole::SCRIPT, role);
    extensions::ChromeExtensionCookies::Get(browser_context)
        ->CreateRestrictedCookieManager(origin, site_for_cookies,
                                        top_frame_origin, std::move(*receiver));
    return true;
  }
#endif
  return false;
}

void ChromeContentBrowserClient::OnNetworkServiceCreated(
    network::mojom::NetworkService* network_service) {
  PrefService* local_state;
  if (g_browser_process) {
    DCHECK(g_browser_process->local_state());
    local_state = g_browser_process->local_state();
  } else {
    DCHECK(startup_data_->chrome_feature_list_creator()->local_state());
    local_state = startup_data_->chrome_feature_list_creator()->local_state();
  }

  if (!data_use_measurement::ChromeDataUseMeasurement::GetInstance())
    data_use_measurement::ChromeDataUseMeasurement::CreateInstance(local_state);

  // Create SystemNetworkContextManager if it has not been created yet. We need
  // to set up global NetworkService state before anything else uses it and this
  // is the first opportunity to initialize SystemNetworkContextManager with the
  // NetworkService.
  if (!SystemNetworkContextManager::HasInstance())
    SystemNetworkContextManager::CreateInstance(local_state);

  SystemNetworkContextManager::GetInstance()->OnNetworkServiceCreated(
      network_service);
}

void ChromeContentBrowserClient::ConfigureNetworkContextParams(
    content::BrowserContext* context,
    bool in_memory,
    const base::FilePath& relative_partition_path,
    network::mojom::NetworkContextParams* network_context_params,
    network::mojom::CertVerifierCreationParams* cert_verifier_creation_params) {
  Profile* profile = Profile::FromBrowserContext(context);
  profile->ConfigureNetworkContextParams(in_memory, relative_partition_path,
                                         network_context_params,
                                         cert_verifier_creation_params);
}

std::vector<base::FilePath>
ChromeContentBrowserClient::GetNetworkContextsParentDirectory() {
  DCHECK(!network_contexts_parent_directory_.empty());
  return network_contexts_parent_directory_;
}

bool ChromeContentBrowserClient::AllowRenderingMhtmlOverHttp(
    content::NavigationUIData* navigation_ui_data) {
#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
  // It is OK to load the saved offline copy, in MHTML format.
  ChromeNavigationUIData* chrome_navigation_ui_data =
      static_cast<ChromeNavigationUIData*>(navigation_ui_data);
  if (!chrome_navigation_ui_data)
    return false;
  offline_pages::OfflinePageNavigationUIData* offline_page_data =
      chrome_navigation_ui_data->GetOfflinePageNavigationUIData();
  return offline_page_data && offline_page_data->is_offline_page();
#else
  return false;
#endif
}

bool ChromeContentBrowserClient::ShouldForceDownloadResource(
    const GURL& url,
    const std::string& mime_type) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Special-case user scripts to get downloaded instead of viewed.
  return extensions::UserScript::IsURLUserScript(url, mime_type);
#else
  return false;
#endif
}

void ChromeContentBrowserClient::CreateWebUsbService(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::WebUsbService> receiver) {
  if (!base::FeatureList::IsEnabled(features::kWebUsb))
    return;

  CHECK(render_frame_host);
  FrameUsbServices::CreateFrameUsbServices(render_frame_host,
                                           std::move(receiver));
}

content::BluetoothDelegate* ChromeContentBrowserClient::GetBluetoothDelegate() {
  if (!bluetooth_delegate_)
    bluetooth_delegate_ = std::make_unique<ChromeBluetoothDelegate>();
  return bluetooth_delegate_.get();
}

#if !defined(OS_ANDROID)
content::SerialDelegate* ChromeContentBrowserClient::GetSerialDelegate() {
  if (!serial_delegate_)
    serial_delegate_ = std::make_unique<ChromeSerialDelegate>();
  return serial_delegate_.get();
}

content::HidDelegate* ChromeContentBrowserClient::GetHidDelegate() {
  if (!hid_delegate_)
    hid_delegate_ = std::make_unique<ChromeHidDelegate>();
  return hid_delegate_.get();
}

std::unique_ptr<content::AuthenticatorRequestClientDelegate>
ChromeContentBrowserClient::GetWebAuthenticationRequestDelegate(
    content::RenderFrameHost* render_frame_host) {
  return AuthenticatorRequestScheduler::CreateRequestDelegate(
      render_frame_host);
}
#endif

std::unique_ptr<net::ClientCertStore>
ChromeContentBrowserClient::CreateClientCertStore(
    content::BrowserContext* browser_context) {
  return ProfileNetworkContextServiceFactory::GetForContext(browser_context)
      ->CreateClientCertStore();
}

std::unique_ptr<content::LoginDelegate>
ChromeContentBrowserClient::CreateLoginDelegate(
    const net::AuthChallengeInfo& auth_info,
    content::WebContents* web_contents,
    const content::GlobalRequestID& request_id,
    bool is_request_for_main_frame,
    const GURL& url,
    scoped_refptr<net::HttpResponseHeaders> response_headers,
    bool first_auth_attempt,
    LoginAuthRequiredCallback auth_required_callback) {
  // For subresources, create a LoginHandler directly, which may show a login
  // prompt to the user. Main frame resources go through LoginTabHelper, which
  // manages a more complicated flow to avoid confusion about which website is
  // showing the prompt.
  if (is_request_for_main_frame) {
    LoginTabHelper::CreateForWebContents(web_contents);
    return LoginTabHelper::FromWebContents(web_contents)
        ->CreateAndStartMainFrameLoginDelegate(
            auth_info, web_contents, request_id, url, response_headers,
            std::move(auth_required_callback));
  }
  std::unique_ptr<LoginHandler> login_handler = LoginHandler::Create(
      auth_info, web_contents, std::move(auth_required_callback));
  login_handler->StartSubresource(request_id, url, response_headers);
  return login_handler;
}

bool ChromeContentBrowserClient::HandleExternalProtocol(
    const GURL& url,
    content::WebContents::OnceGetter web_contents_getter,
    int child_id,
    content::NavigationUIData* navigation_data,
    bool is_main_frame,
    ui::PageTransition page_transition,
    bool has_user_gesture,
    const base::Optional<url::Origin>& initiating_origin,
    mojo::PendingRemote<network::mojom::URLLoaderFactory>* out_factory) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  // External protocols are disabled for guests. An exception is made for the
  // "mailto" protocol, so that pages that utilize it work properly in a
  // WebView.
  ChromeNavigationUIData* chrome_data =
      static_cast<ChromeNavigationUIData*>(navigation_data);
  if ((extensions::WebViewRendererState::GetInstance()->IsGuest(child_id) ||
       (chrome_data &&
        chrome_data->GetExtensionNavigationUIData()->is_web_view())) &&
      !url.SchemeIs(url::kMailToScheme)) {
    return false;
  }
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

#if defined(OS_ANDROID)
  // Main frame external protocols are handled by
  // InterceptNavigationResourceThrottle.
  if (is_main_frame)
    return false;
#endif  // defined(ANDROID)

  base::PostTask(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&LaunchURL, url, std::move(web_contents_getter),
                     page_transition, has_user_gesture, initiating_origin));
  return true;
}

std::unique_ptr<content::OverlayWindow>
ChromeContentBrowserClient::CreateWindowForPictureInPicture(
    content::PictureInPictureWindowController* controller) {
  // Note: content::OverlayWindow::Create() is defined by platform-specific
  // implementation in chrome/browser/ui/views. This layering hack, which goes
  // through //content and ContentBrowserClient, allows us to work around the
  // dependency constraints that disallow directly calling
  // chrome/browser/ui/views code either from here or from other code in
  // chrome/browser.
  return content::OverlayWindow::Create(controller);
}

void ChromeContentBrowserClient::RegisterRendererPreferenceWatcher(
    content::BrowserContext* browser_context,
    mojo::PendingRemote<blink::mojom::RendererPreferenceWatcher> watcher) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  PrefWatcher::Get(profile)->RegisterRendererPreferenceWatcher(
      std::move(watcher));
}

// Static; handles rewriting Web UI URLs.
bool ChromeContentBrowserClient::HandleWebUI(
    GURL* url,
    content::BrowserContext* browser_context) {
  // Rewrite chrome://help to chrome://settings/help.
  if (url->SchemeIs(content::kChromeUIScheme) &&
      url->host() == chrome::kChromeUIHelpHost) {
    *url = ReplaceURLHostAndPath(*url, chrome::kChromeUISettingsHost,
                                 chrome::kChromeUIHelpHost);
    return true;  // Return true to update the displayed URL.
  }

#if defined(OS_WIN)
  // TODO(crbug.com/1003960): Remove when issue is resolved.
  if (url->SchemeIs(content::kChromeUIScheme) &&
      url->host() == chrome::kChromeUIWelcomeWin10Host) {
    url::Replacements<char> replacements;
    replacements.SetHost(
        chrome::kChromeUIWelcomeHost,
        url::Component(0, strlen(chrome::kChromeUIWelcomeHost)));
    *url = url->ReplaceComponents(replacements);
    return true;
  }
#endif  // defined(OS_WIN)

  if (!ChromeWebUIControllerFactory::GetInstance()->UseWebUIForURL(
          browser_context, *url)) {
    return false;
  }

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

  if (IsSystemFeatureURLDisabled(*url)) {
    *url = ReplaceURLHostAndPath(*url, chrome::kChromeUIAppDisabledHost, "");
    return true;
  }

#endif

  return true;
}

bool ChromeContentBrowserClient::ShowPaymentHandlerWindow(
    content::BrowserContext* browser_context,
    const GURL& url,
    base::OnceCallback<void(bool, int, int)> callback) {
#if defined(OS_ANDROID)
  return false;
#else
  payments::PaymentRequestDisplayManagerFactory::GetInstance()
      ->GetForBrowserContext(browser_context)
      ->ShowPaymentHandlerWindow(url, std::move(callback));
  return true;
#endif
}

// static
bool ChromeContentBrowserClient::HandleWebUIReverse(
    GURL* url,
    content::BrowserContext* browser_context) {
#if defined(OS_WIN)
  // TODO(crbug.com/1003960): Remove when issue is resolved.
  // No need to actually reverse-rewrite the URL, but return true to update the
  // displayed URL when rewriting chrome://welcome-win10 to chrome://welcome.
  if (url->SchemeIs(content::kChromeUIScheme) &&
      url->host() == chrome::kChromeUIWelcomeHost) {
    return true;
  }
#endif  // defined(OS_WIN)

  // No need to actually reverse-rewrite the URL, but return true to update the
  // displayed URL when rewriting chrome://help to chrome://settings/help.
  return url->SchemeIs(content::kChromeUIScheme) &&
         url->host() == chrome::kChromeUISettingsHost;
}

const ui::NativeTheme* ChromeContentBrowserClient::GetWebTheme() const {
  return ui::NativeTheme::GetInstanceForWeb();
}

scoped_refptr<safe_browsing::UrlCheckerDelegate>
ChromeContentBrowserClient::GetSafeBrowsingUrlCheckerDelegate(
    bool safe_browsing_enabled_for_profile) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!safe_browsing_enabled_for_profile)
    return nullptr;

  // |safe_browsing_service_| may be unavailable in tests.
  if (safe_browsing_service_ && !safe_browsing_url_checker_delegate_) {
    safe_browsing_url_checker_delegate_ =
        base::MakeRefCounted<safe_browsing::UrlCheckerDelegateImpl>(
            safe_browsing_service_->database_manager(),
            safe_browsing_service_->ui_manager());
  }

  return safe_browsing_url_checker_delegate_;
}

base::Optional<std::string>
ChromeContentBrowserClient::GetOriginPolicyErrorPage(
    network::OriginPolicyState error_reason,
    content::NavigationHandle* handle) {
  return security_interstitials::OriginPolicyUI::GetErrorPageAsHTML(
      error_reason, handle);
}

bool ChromeContentBrowserClient::CanAcceptUntrustedExchangesIfNeeded() {
  // We require --user-data-dir flag too so that no dangerous changes are made
  // in the user's regular profile.
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kUserDataDir);
}

void ChromeContentBrowserClient::OnNetworkServiceDataUseUpdate(
    int32_t network_traffic_annotation_id_hash,
    int64_t recv_bytes,
    int64_t sent_bytes) {
  if (data_use_measurement::ChromeDataUseMeasurement::GetInstance()) {
    data_use_measurement::ChromeDataUseMeasurement::GetInstance()
        ->ReportNetworkServiceDataUse(network_traffic_annotation_id_hash,
                                      recv_bytes, sent_bytes);
  }
}

base::FilePath
ChromeContentBrowserClient::GetSandboxedStorageServiceDataDirectory() {
  return g_browser_process->profile_manager()->user_data_dir();
}

content::PreviewsState ChromeContentBrowserClient::DetermineAllowedPreviews(
    content::PreviewsState initial_state,
    content::NavigationHandle* navigation_handle,
    const GURL& current_navigation_url) {
  content::PreviewsState state = DetermineAllowedPreviewsWithoutHoldback(
      initial_state, navigation_handle, current_navigation_url);

  return previews::MaybeCoinFlipHoldbackBeforeCommit(state, navigation_handle);
}

content::PreviewsState
ChromeContentBrowserClient::DetermineAllowedPreviewsWithoutHoldback(
    content::PreviewsState initial_state,
    content::NavigationHandle* navigation_handle,
    const GURL& current_navigation_url) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!navigation_handle->HasCommitted());

  // If this is not a main frame, return the initial state. If there are no
  // previews in the state, return the state as is.
  if (!navigation_handle->IsInMainFrame() ||
      navigation_handle->IsSameDocument()) {
    return initial_state;
  }

  if (!current_navigation_url.SchemeIsHTTPOrHTTPS())
    return content::PREVIEWS_OFF;

  // Check if initial state specifies no previews should be considered.
  if (initial_state == content::PREVIEWS_OFF)
    return initial_state;

  // Do not allow previews on POST navigations since the primary opt-out
  // mechanism is to reload the page. Because POST navigations are not
  // idempotent, we do not want to show a preview on a POST navigation where
  // opting out would cause another navigation, i.e.: a reload.
  if (navigation_handle->IsPost())
    return content::PREVIEWS_OFF;

  content::WebContents* web_contents = navigation_handle->GetWebContents();
  content::WebContentsDelegate* delegate = web_contents->GetDelegate();

  auto* browser_context = web_contents->GetBrowserContext();

  PreviewsService* previews_service = PreviewsServiceFactory::GetForProfile(
      Profile::FromBrowserContext(browser_context));
  auto* data_reduction_proxy_settings =
      DataReductionProxyChromeSettingsFactory::GetForBrowserContext(
          browser_context);
  // If the profile does not support previews or Data Saver, do not turn on
  // Previews.
  if (!previews_service || !previews_service->previews_ui_service() ||
      !data_reduction_proxy_settings) {
    return content::PREVIEWS_OFF;
  }

  PreviewsUITabHelper* ui_tab_helper =
      PreviewsUITabHelper::FromWebContents(web_contents);
  // If this tab does not have a PreviewsUITabHelper, no preview should be
  // served.
  if (!ui_tab_helper)
    return content::PREVIEWS_OFF;

  DCHECK(!browser_context->IsOffTheRecord());

  // Other previews objects should all exist and be initialized if we have made
  // it past earlier checks.
  previews::PreviewsDeciderImpl* previews_decider_impl =
      previews_service->previews_ui_service()->previews_decider_impl();
  DCHECK(previews_decider_impl);

  // Start with an unspecified state.
  content::PreviewsState previews_state = content::PREVIEWS_UNSPECIFIED;

  previews::PreviewsUserData* previews_data =
      ui_tab_helper->GetPreviewsUserData(navigation_handle);

  // Certain PreviewsStates are used within URLLoaders (Offline, server
  // previews) and cannot re-evaluate PreviewsState once previews triggering
  // logic has already been run, so they should not change. Assume that
  // previews triggering logic has run when PreviewsUserData already exists.
  bool previews_triggering_logic_already_ran = false;
  if (previews_data) {
    previews_triggering_logic_already_ran = true;
  } else {
    previews_data = ui_tab_helper->CreatePreviewsUserDataForNavigationHandle(
        navigation_handle, previews_decider_impl->GeneratePageId());
  }

  DCHECK(previews_data);

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          previews::switches::kForceEnablePreviews)) {
    return content::ALL_SUPPORTED_PREVIEWS;
  }

  bool is_reload =
      navigation_handle->GetReloadType() != content::ReloadType::NONE;

  content::PreviewsState server_previews_enabled_state =
      content::SERVER_LITE_PAGE_ON;

  // For now, treat server previews types as a single decision, and do not
  // re-evaluate upon redirect. Plumbing does not exist to modify the CPAT
  // header, nor does the plumbing exist to modify the PreviewsState within the
  // URLLoader.
  if (previews_triggering_logic_already_ran) {
    // Copy the server state that was used before the redirect for the initial
    // URL.
    previews_state |=
        (previews_data->AllowedPreviewsState() & server_previews_enabled_state);
  } else {
    if (previews_decider_impl->ShouldAllowPreviewAtNavigationStart(
            previews_data, navigation_handle, is_reload,
            previews::PreviewsType::LITE_PAGE)) {
      previews_state |= server_previews_enabled_state;
    }
  }

  // Evaluate client-side previews.
  previews_state |= previews::DetermineAllowedClientPreviewsState(
      previews_data, previews_triggering_logic_already_ran,
      data_reduction_proxy_settings->IsDataReductionProxyEnabled(),
      previews_decider_impl, navigation_handle);

  if (previews_state & content::PREVIEWS_OFF) {
    previews_data->set_allowed_previews_state(content::PREVIEWS_OFF);
    return content::PREVIEWS_OFF;
  }

  if (previews_state & content::PREVIEWS_NO_TRANSFORM) {
    previews_data->set_allowed_previews_state(content::PREVIEWS_NO_TRANSFORM);
    return content::PREVIEWS_NO_TRANSFORM;
  }

  // At this point, if no Preview is allowed, don't allow previews.
  if (previews_state == content::PREVIEWS_UNSPECIFIED) {
    previews_data->set_allowed_previews_state(content::PREVIEWS_OFF);
    return content::PREVIEWS_OFF;
  }

  content::PreviewsState embedder_state = content::PREVIEWS_UNSPECIFIED;
  if (delegate) {
    delegate->AdjustPreviewsStateForNavigation(web_contents, &embedder_state);
  }

  // If the allowed previews are limited by the embedder, ensure previews honors
  // those limits.
  if (embedder_state != content::PREVIEWS_UNSPECIFIED) {
    previews_state = previews_state & embedder_state;
    // If no valid previews are left, set the state explicitly to PREVIEWS_OFF.
    if (previews_state == content::PREVIEWS_UNSPECIFIED)
      previews_state = content::PREVIEWS_OFF;
  }
  previews_data->set_allowed_previews_state(previews_state);
  return previews_state;
}

// static
content::PreviewsState
ChromeContentBrowserClient::DetermineCommittedPreviewsForURL(
    const GURL& url,
    data_reduction_proxy::DataReductionProxyData* drp_data,
    previews::PreviewsUserData* previews_user_data,
    const previews::PreviewsDecider* previews_decider,
    content::PreviewsState initial_state,
    content::NavigationHandle* navigation_handle) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!previews::HasEnabledPreviews(initial_state))
    return content::PREVIEWS_OFF;

  // Check if the server sent a preview directive.
  content::PreviewsState previews_state =
      previews::DetermineCommittedServerPreviewsState(drp_data, initial_state);

  // Check the various other client previews types.
  return previews::DetermineCommittedClientPreviewsState(
      previews_user_data, url, previews_state, previews_decider,
      navigation_handle);
}

content::PreviewsState ChromeContentBrowserClient::DetermineCommittedPreviews(
    content::PreviewsState initial_state,
    content::NavigationHandle* navigation_handle,
    const net::HttpResponseHeaders* response_headers) {
  content::PreviewsState state = DetermineCommittedPreviewsWithoutHoldback(
      initial_state, navigation_handle, response_headers);

  return previews::MaybeCoinFlipHoldbackAfterCommit(state, navigation_handle);
}

content::PreviewsState
ChromeContentBrowserClient::DetermineCommittedPreviewsWithoutHoldback(
    content::PreviewsState initial_state,
    content::NavigationHandle* navigation_handle,
    const net::HttpResponseHeaders* response_headers) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // Only support HTTP and HTTPS.
  if (navigation_handle->IsErrorPage() ||
      !navigation_handle->GetURL().SchemeIsHTTPOrHTTPS()) {
    return content::PREVIEWS_OFF;
  }

  // If this is not a main frame, return the initial state. If there are no
  // previews in the state, return the state as is.
  if (!previews::HasEnabledPreviews(initial_state) ||
      !navigation_handle->IsInMainFrame() ||
      navigation_handle->IsSameDocument()) {
    return initial_state;
  }

  // WebContents that don't have a PreviewsUITabHelper are not supported.
  PreviewsUITabHelper* ui_tab_helper =
      PreviewsUITabHelper::FromWebContents(navigation_handle->GetWebContents());
  if (!ui_tab_helper)
    return content::PREVIEWS_OFF;

  // If we did not previously create a PreviewsUserData, do not go any further.
  previews::PreviewsUserData* previews_user_data =
      ui_tab_helper->GetPreviewsUserData(navigation_handle);
  if (!previews_user_data)
    return content::PREVIEWS_OFF;

  PreviewsService* previews_service =
      PreviewsServiceFactory::GetForProfile(Profile::FromBrowserContext(
          navigation_handle->GetWebContents()->GetBrowserContext()));

  if (!previews_service || !previews_service->previews_ui_service())
    return content::PREVIEWS_OFF;

// Check if offline previews are being used and set it in the user data.
#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
  offline_pages::OfflinePageTabHelper* tab_helper =
      offline_pages::OfflinePageTabHelper::FromWebContents(
          navigation_handle->GetWebContents());

  bool is_offline_page = tab_helper && tab_helper->IsLoadingOfflinePage();
  bool is_offline_preview = tab_helper && tab_helper->GetOfflinePreviewItem();

  // If this is an offline page, but not a preview, then we should not attempt
  // any previews or surface the previews UI.
  if (is_offline_page && !is_offline_preview)
    return content::PREVIEWS_OFF;

  previews_user_data->set_offline_preview_used(is_offline_preview);
#endif  // BUILDFLAG(ENABLE_OFFLINE_PAGES)

  // Annotate request if no-transform directive found in response headers.
  if (response_headers &&
      response_headers->HasHeaderValue("cache-control", "no-transform")) {
    previews_user_data->set_cache_control_no_transform_directive();
  }

  previews::PreviewsDeciderImpl* previews_decider_impl =
      previews_service->previews_ui_service()->previews_decider_impl();
  DCHECK(previews_decider_impl);

  std::unique_ptr<data_reduction_proxy::DataReductionProxyData> drp_data;
  auto* settings =
      DataReductionProxyChromeSettingsFactory::GetForBrowserContext(
          navigation_handle->GetWebContents()->GetBrowserContext());
  if (settings) {
    // TODO(898326): |drp_data| may be incomplete because |navigation_handle|
    // does not yet have all the response information.
    drp_data = settings->CreateDataFromNavigationHandle(navigation_handle,
                                                        response_headers);
  }

  // Determine effective PreviewsState for this committed main frame response.
  content::PreviewsState committed_state = DetermineCommittedPreviewsForURL(
      navigation_handle->GetURL(), drp_data.get(), previews_user_data,
      previews_decider_impl, initial_state, navigation_handle);

  // Double check that we never serve a preview when we have a
  // cache-control:no-transform directive.
  DCHECK(!previews_user_data->cache_control_no_transform_directive() ||
         !previews::HasEnabledPreviews(committed_state));

  // TODO(robertogden): Consider moving this to after the holdback logic.
  previews_user_data->set_committed_previews_state(committed_state);

  previews::PreviewsType committed_type =
      previews::GetMainFramePreviewsType(committed_state);

  // Capture committed previews type, if any, in PreviewsUserData.
  // Note: this is for the subset of previews types that are decided upon
  // navigation commit. Previews types that are determined prior to
  // navigation (such as for offline pages or for redirecting to another
  // url), are not set here.
  previews_user_data->SetCommittedPreviewsType(committed_type);

  // Log the commit decision.
  std::vector<previews::PreviewsEligibilityReason> passed_reasons;
  previews_decider_impl->LogPreviewDecisionMade(
      (previews_user_data->cache_control_no_transform_directive()
           ? previews::PreviewsEligibilityReason::CACHE_CONTROL_NO_TRANSFORM
           : previews::PreviewsEligibilityReason::COMMITTED),
      navigation_handle->GetURL(), base::Time::Now(),
      previews_user_data->CommittedPreviewsType(), std::move(passed_reasons),
      previews_user_data);

  return committed_state;
}

void ChromeContentBrowserClient::LogWebFeatureForCurrentPage(
    content::RenderFrameHost* render_frame_host,
    blink::mojom::WebFeature feature) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  page_load_metrics::mojom::PageLoadFeatures new_features({feature}, {}, {});
  page_load_metrics::MetricsWebContentsObserver::RecordFeatureUsage(
      render_frame_host, new_features);

  // For the SameSite-by-default-cookies related features, log
  // the site engagement score for the site whose cookie was blocked. This is to
  // gauge the user impact of the cookies being blocked.
  MaybeRecordSameSiteCookieEngagementHistogram(render_frame_host, feature);
}

std::string ChromeContentBrowserClient::GetProduct() {
  return ::GetProduct();
}

std::string ChromeContentBrowserClient::GetUserAgent() {
  return ::GetUserAgent();
}

blink::UserAgentMetadata ChromeContentBrowserClient::GetUserAgentMetadata() {
  return ::GetUserAgentMetadata();
}

base::Optional<gfx::ImageSkia> ChromeContentBrowserClient::GetProductLogo() {
  // This icon is available on Android, but adds 19KiB to the APK. Since it
  // isn't used on Android we exclude it to avoid bloat.
#if !defined(OS_ANDROID)
  return base::Optional<gfx::ImageSkia>(
      *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
          IDR_PRODUCT_LOGO_256));
#else
  return base::nullopt;
#endif
}

bool ChromeContentBrowserClient::IsBuiltinComponent(
    content::BrowserContext* browser_context,
    const url::Origin& origin) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  return ChromeContentBrowserClientExtensionsPart::IsBuiltinComponent(
      browser_context, origin);
#else
  return false;
#endif
}

bool ChromeContentBrowserClient::IsRendererDebugURLBlacklisted(
    const GURL& url,
    content::BrowserContext* context) {
  PolicyBlacklistService* service =
      PolicyBlacklistFactory::GetForBrowserContext(context);

  using URLBlacklistState = policy::URLBlacklist::URLBlacklistState;
  URLBlacklistState blacklist_state = service->GetURLBlacklistState(url);
  return blacklist_state == URLBlacklistState::URL_IN_BLACKLIST;
}

ui::AXMode ChromeContentBrowserClient::GetAXModeForBrowserContext(
    content::BrowserContext* browser_context) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  return AccessibilityLabelsServiceFactory::GetForProfile(profile)->GetAXMode();
}

#if defined(OS_ANDROID)
content::ContentBrowserClient::WideColorGamutHeuristic
ChromeContentBrowserClient::GetWideColorGamutHeuristic() {
  if (viz::AlwaysUseWideColorGamut() ||
      features::IsDynamicColorGamutEnabled()) {
    return WideColorGamutHeuristic::kUseDisplay;
  }

  if (display::Display::HasForceDisplayColorProfile() &&
      display::Display::GetForcedDisplayColorProfile() ==
          gfx::ColorSpace::CreateDisplayP3D65()) {
    return WideColorGamutHeuristic::kUseDisplay;
  }

  return WideColorGamutHeuristic::kNone;
}
#endif

base::flat_set<std::string>
ChromeContentBrowserClient::GetPluginMimeTypesWithExternalHandlers(
    content::BrowserContext* browser_context) {
  base::flat_set<std::string> mime_types;
#if BUILDFLAG(ENABLE_PLUGINS)
  auto map = PluginUtils::GetMimeTypeToExtensionIdMap(browser_context);
  for (const auto& pair : map)
    mime_types.insert(pair.first);
#endif
  return mime_types;
}

void ChromeContentBrowserClient::AugmentNavigationDownloadPolicy(
    const content::WebContents* web_contents,
    const content::RenderFrameHost* frame_host,
    bool user_gesture,
    content::NavigationDownloadPolicy* download_policy) {
  const ChromeSubresourceFilterClient* client =
      ChromeSubresourceFilterClient::FromWebContents(web_contents);
  if (client && client->GetThrottleManager()->IsFrameTaggedAsAd(frame_host)) {
    download_policy->SetAllowed(content::NavigationDownloadType::kAdFrame);
    if (!user_gesture) {
      if (base::FeatureList::IsEnabled(
              blink::features::
                  kBlockingDownloadsInAdFrameWithoutUserActivation)) {
        download_policy->SetDisallowed(
            content::NavigationDownloadType::kAdFrameNoGesture);
      } else {
        download_policy->SetAllowed(
            content::NavigationDownloadType::kAdFrameNoGesture);
      }
    }
  }
}

bool ChromeContentBrowserClient::IsBluetoothScanningBlocked(
    content::BrowserContext* browser_context,
    const url::Origin& requesting_origin,
    const url::Origin& embedding_origin) {
  const HostContentSettingsMap* const content_settings =
      HostContentSettingsMapFactory::GetForProfile(
          Profile::FromBrowserContext(browser_context));

  if (content_settings->GetContentSetting(
          requesting_origin.GetURL(), embedding_origin.GetURL(),
          ContentSettingsType::BLUETOOTH_SCANNING,
          std::string()) == CONTENT_SETTING_BLOCK) {
    return true;
  }

  return false;
}

void ChromeContentBrowserClient::BlockBluetoothScanning(
    content::BrowserContext* browser_context,
    const url::Origin& requesting_origin,
    const url::Origin& embedding_origin) {
  HostContentSettingsMap* const content_settings =
      HostContentSettingsMapFactory::GetForProfile(
          Profile::FromBrowserContext(browser_context));

  content_settings->SetContentSettingDefaultScope(
      requesting_origin.GetURL(), embedding_origin.GetURL(),
      ContentSettingsType::BLUETOOTH_SCANNING, std::string(),
      CONTENT_SETTING_BLOCK);
}

bool ChromeContentBrowserClient::ShouldLoadExtraIcuDataFile() {
#if defined(OS_ANDROID)
  return extra_icu::ModuleProvider::IsModuleInstalled();
#endif
  return false;
}

bool ChromeContentBrowserClient::ArePersistentMediaDeviceIDsAllowed(
    content::BrowserContext* browser_context,
    const GURL& url,
    const GURL& site_for_cookies,
    const base::Optional<url::Origin>& top_frame_origin) {
  // Persistent MediaDevice IDs are allowed if cookies are allowed.
  return CookieSettingsFactory::GetForProfile(
             Profile::FromBrowserContext(browser_context))
      ->IsCookieAccessAllowed(url, site_for_cookies, top_frame_origin);
}

#if !defined(OS_ANDROID)
void ChromeContentBrowserClient::FetchRemoteSms(
    content::BrowserContext* browser_context,
    const url::Origin& origin,
    base::OnceCallback<void(base::Optional<std::string>)> callback) {
  ::FetchRemoteSms(browser_context, origin, std::move(callback));
}
#endif

void ChromeContentBrowserClient::IsClipboardPasteAllowed(
    content::WebContents* web_contents,
    const GURL& url,
    const ui::ClipboardFormatType& data_type,
    const std::string& data,
    IsClipboardPasteAllowedCallback callback) {
#if BUILDFLAG(FULL_SAFE_BROWSING)
  // Safe browsing does not support images, so accept without checking.
  // TODO(crbug.com/1013584): check policy on what to do about unsupported
  // types when it is implemented.
  if (data_type.Equals(ui::ClipboardFormatType::GetBitmapType())) {
    std::move(callback).Run(ClipboardPasteAllowed(true));
    return;
  }

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  safe_browsing::DeepScanningDialogDelegate::Data dialog_data;
  if (safe_browsing::DeepScanningDialogDelegate::IsEnabled(
          profile, url, &dialog_data,
          enterprise_connectors::AnalysisConnector::BULK_DATA_ENTRY)) {
    dialog_data.text.push_back(base::UTF8ToUTF16(data));
    safe_browsing::DeepScanningDialogDelegate::ShowForWebContents(
        web_contents, std::move(dialog_data),
        base::BindOnce(
            [](IsClipboardPasteAllowedCallback callback,
               const safe_browsing::DeepScanningDialogDelegate::Data& data,
               const safe_browsing::DeepScanningDialogDelegate::Result&
                   result) {
              std::move(callback).Run(
                  ClipboardPasteAllowed(result.text_results[0]));
            },
            std::move(callback)),
        safe_browsing::DeepScanAccessPoint::PASTE);
  } else {
    std::move(callback).Run(ClipboardPasteAllowed(true));
  }
#else
  std::move(callback).Run(ClipboardPasteAllowed(true));
#endif  // BUILDFLAG(FULL_SAFE_BROWSING)
}

void ChromeContentBrowserClient::
    LogUkmEventForCrossOriginFetchFromContentScript3(
        const std::string& isolated_world_host) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  const std::string& extension_id = isolated_world_host;
  ukm::SourceId source_id =
      ukm::AppSourceUrlRecorder::GetSourceIdForChromeExtension(extension_id);
  if (source_id != ukm::kInvalidSourceId) {
    ukm::builders::Extensions_CrossOriginFetchFromContentScript3 ukm_event(
        source_id);
    ukm_event.SetEventHappened(true).Record(ukm::UkmRecorder::Get());
  }
#endif
}

#if BUILDFLAG(ENABLE_PLUGINS)
bool ChromeContentBrowserClient::ShouldAllowPluginCreation(
    const url::Origin& embedder_origin,
    const content::PepperPluginInfo& plugin_info) {
#if BUILDFLAG(ENABLE_PDF)
  if (plugin_info.name == ChromeContentClient::kPDFInternalPluginName) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
    // Allow embedding the internal PDF plugin in the built-in PDF extension.
    if (embedder_origin.scheme() == extensions::kExtensionScheme &&
        embedder_origin.host() == extension_misc::kPdfExtensionId) {
      return true;
    }
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

    // Allow embedding the internal PDF plugin in chrome://print.
    if (embedder_origin == url::Origin::Create(GURL(chrome::kChromeUIPrintURL)))
      return true;

    // Only allow the PDF plugin in the known, trustworthy origins that are
    // allowlisted above.  See also https://crbug.com/520422 and
    // https://crbug.com/1027173.
    return false;
  }
#endif  // BUILDFLAG(ENABLE_PDF)

  return true;
}
#endif  // BUILDFLAG(ENABLE_PLUGINS)

#if BUILDFLAG(ENABLE_VR)
content::XrIntegrationClient*
ChromeContentBrowserClient::GetXrIntegrationClient() {
  if (!xr_integration_client_)
    xr_integration_client_ = std::make_unique<vr::ChromeXrIntegrationClient>(
        util::PassKey<ChromeContentBrowserClient>());
  return xr_integration_client_.get();
}
#endif  // BUILDFLAG(ENABLE_VR)

bool ChromeContentBrowserClient::IsOriginTrialRequiredForAppCache(
    content::BrowserContext* browser_context) {
  auto* profile = Profile::FromBrowserContext(browser_context);
  auto* prefs = profile->GetPrefs();

  if (prefs->HasPrefPath(prefs::kAppCacheForceEnabled) &&
      prefs->GetBoolean(prefs::kAppCacheForceEnabled)) {
    return false;
  }
  if (base::FeatureList::IsEnabled(
          blink::features::kAppCacheRequireOriginTrial)) {
    return true;
  }

  return false;
}

bool ChromeContentBrowserClient::
    ShouldInheritCrossOriginEmbedderPolicyImplicitly(const GURL& url) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  return url.SchemeIs(extensions::kExtensionScheme);
#else
  return false;
#endif
}
