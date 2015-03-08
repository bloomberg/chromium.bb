# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    # This turns on e.g. the filename-based detection of which
    # platforms to include source files on (e.g. files ending in
    # _mac.h or _mac.cc are only compiled on MacOSX).
    'chromium_code': 1,

    # Note: sources list duplicated in GN build. In the GN build,
    # each component has its own unit tests target defined in its
    # directory that are then linked into the final content_unittests.
    'auto_login_parser_unittest_sources': [
      'auto_login_parser/auto_login_parser_unittest.cc',
    ],
    'autofill_unittest_sources': [
      'autofill/content/browser/content_autofill_driver_unittest.cc',
      'autofill/content/browser/request_autocomplete_manager_unittest.cc',
      'autofill/content/browser/wallet/full_wallet_unittest.cc',
      'autofill/content/browser/wallet/instrument_unittest.cc',
      'autofill/content/browser/wallet/wallet_address_unittest.cc',
      'autofill/content/browser/wallet/wallet_client_unittest.cc',
      'autofill/content/browser/wallet/wallet_items_unittest.cc',
      'autofill/content/browser/wallet/wallet_service_url_unittest.cc',
      'autofill/content/browser/wallet/wallet_signin_helper_unittest.cc',
      'autofill/content/renderer/renderer_save_password_progress_logger_unittest.cc',
      'autofill/core/browser/address_field_unittest.cc',
      'autofill/core/browser/address_i18n_unittest.cc',
      'autofill/core/browser/address_unittest.cc',
      'autofill/core/browser/autocomplete_history_manager_unittest.cc',
      'autofill/core/browser/autofill_country_unittest.cc',
      'autofill/core/browser/autofill_data_model_unittest.cc',
      'autofill/core/browser/autofill_download_manager_unittest.cc',
      'autofill/core/browser/autofill_external_delegate_unittest.cc',
      'autofill/core/browser/autofill_field_unittest.cc',
      'autofill/core/browser/autofill_ie_toolbar_import_win_unittest.cc',
      'autofill/core/browser/autofill_manager_unittest.cc',
      'autofill/core/browser/autofill_merge_unittest.cc',
      'autofill/core/browser/autofill_metrics_unittest.cc',
      'autofill/core/browser/autofill_profile_unittest.cc',
      'autofill/core/browser/autofill_regexes_unittest.cc',
      'autofill/core/browser/autofill_type_unittest.cc',
      'autofill/core/browser/autofill_xml_parser_unittest.cc',
      'autofill/core/browser/contact_info_unittest.cc',
      'autofill/core/browser/credit_card_field_unittest.cc',
      'autofill/core/browser/credit_card_unittest.cc',
      'autofill/core/browser/form_field_unittest.cc',
      'autofill/core/browser/form_structure_unittest.cc',
      'autofill/core/browser/name_field_unittest.cc',
      'autofill/core/browser/password_generator_unittest.cc',
      'autofill/core/browser/personal_data_manager_unittest.cc',
      'autofill/core/browser/phone_field_unittest.cc',
      'autofill/core/browser/phone_number_i18n_unittest.cc',
      'autofill/core/browser/phone_number_unittest.cc',
      'autofill/core/browser/validation_unittest.cc',
      'autofill/core/browser/webdata/autofill_profile_syncable_service_unittest.cc',
      'autofill/core/browser/webdata/autofill_table_unittest.cc',
      'autofill/core/browser/webdata/web_data_service_unittest.cc',
      'autofill/core/common/form_data_unittest.cc',
      'autofill/core/common/form_field_data_unittest.cc',
      'autofill/core/common/password_form_fill_data_unittest.cc',
      'autofill/core/common/save_password_progress_logger_unittest.cc',
    ],
    'bookmarks_unittest_sources': [
      'bookmarks/browser/bookmark_codec_unittest.cc',
      'bookmarks/browser/bookmark_expanded_state_tracker_unittest.cc',
      'bookmarks/browser/bookmark_index_unittest.cc',
      'bookmarks/browser/bookmark_model_unittest.cc',
      'bookmarks/browser/bookmark_utils_unittest.cc',
    ],
    'browser_watcher_unittest_sources': [
      'browser_watcher/endsession_watcher_window_win_unittest.cc',
      'browser_watcher/exit_code_watcher_win_unittest.cc',
      'browser_watcher/exit_funnel_win_unittest.cc',
      'browser_watcher/watcher_client_win_unittest.cc',
      'browser_watcher/watcher_metrics_provider_win_unittest.cc',
    ],
    'captive_portal_unittest_sources': [
      'captive_portal/captive_portal_detector_unittest.cc',
    ],
    'cloud_devices_unittest_sources': [
      'cloud_devices/common/cloud_devices_urls_unittest.cc',
      'cloud_devices/common/printer_description_unittest.cc',
    ],
    'content_settings_unittest_sources': [
      'content_settings/core/browser/content_settings_mock_provider.cc',
      'content_settings/core/browser/content_settings_mock_provider.h',
      'content_settings/core/browser/content_settings_provider_unittest.cc',
      'content_settings/core/browser/content_settings_rule_unittest.cc',
      'content_settings/core/browser/content_settings_utils_unittest.cc',
      'content_settings/core/common/content_settings_pattern_parser_unittest.cc',
      'content_settings/core/common/content_settings_pattern_unittest.cc',
    ],
    'crash_unittest_sources': [
      'crash/app/crash_keys_win_unittest.cc',
    ],
    'crx_file_unittest_sources': [
      'crx_file/id_util_unittest.cc',
    ],
    'data_reduction_proxy_unittest_sources': [
      'data_reduction_proxy/content/browser/data_reduction_proxy_message_filter_unittest.cc',
      'data_reduction_proxy/core/browser/data_reduction_proxy_bypass_protocol_unittest.cc',
      'data_reduction_proxy/core/browser/data_reduction_proxy_config_unittest.cc',
      'data_reduction_proxy/core/browser/data_reduction_proxy_configurator_unittest.cc',
      'data_reduction_proxy/core/browser/data_reduction_proxy_interceptor_unittest.cc',
      'data_reduction_proxy/core/browser/data_reduction_proxy_io_data_unittest.cc',
      'data_reduction_proxy/core/browser/data_reduction_proxy_metrics_unittest.cc',
      'data_reduction_proxy/core/browser/data_reduction_proxy_network_delegate_unittest.cc',
      'data_reduction_proxy/core/browser/data_reduction_proxy_prefs_unittest.cc',
      'data_reduction_proxy/core/browser/data_reduction_proxy_request_options_unittest.cc',
      'data_reduction_proxy/core/browser/data_reduction_proxy_settings_unittest.cc',
      'data_reduction_proxy/core/browser/data_reduction_proxy_statistics_prefs_unittest.cc',
      'data_reduction_proxy/core/browser/data_reduction_proxy_tamper_detection_unittest.cc',
      'data_reduction_proxy/core/browser/data_reduction_proxy_usage_stats_unittest.cc',
      'data_reduction_proxy/core/common/data_reduction_proxy_event_store_unittest.cc',
      'data_reduction_proxy/core/common/data_reduction_proxy_headers_unittest.cc',
      'data_reduction_proxy/core/common/data_reduction_proxy_params_unittest.cc',
    ],
    'device_event_log_unittest_sources': [
      'device_event_log/device_event_log_impl_unittest.cc',
    ],
    'dom_distiller_unittest_sources': [
      'dom_distiller/content/dom_distiller_viewer_source_unittest.cc',
      'dom_distiller/content/web_contents_main_frame_observer_unittest.cc',
      'dom_distiller/core/article_entry_unittest.cc',
      'dom_distiller/core/distilled_content_store_unittest.cc',
      'dom_distiller/core/distilled_page_prefs_unittests.cc',
      'dom_distiller/core/distiller_unittest.cc',
      'dom_distiller/core/distiller_url_fetcher_unittest.cc',
      'dom_distiller/core/dom_distiller_model_unittest.cc',
      'dom_distiller/core/dom_distiller_service_unittest.cc',
      'dom_distiller/core/dom_distiller_store_unittest.cc',
      'dom_distiller/core/task_tracker_unittest.cc',
      'dom_distiller/core/url_utils_unittest.cc',
      'dom_distiller/core/viewer_unittest.cc',
    ],
    'domain_reliability_unittest_sources': [
      'domain_reliability/config_unittest.cc',
      'domain_reliability/context_unittest.cc',
      'domain_reliability/dispatcher_unittest.cc',
      'domain_reliability/monitor_unittest.cc',
      'domain_reliability/scheduler_unittest.cc',
      'domain_reliability/test_util.cc',
      'domain_reliability/test_util.h',
      'domain_reliability/uploader_unittest.cc',
      'domain_reliability/util_unittest.cc',
    ],
    'favicon_base_unittest_sources': [
      'favicon_base/select_favicon_frames_unittest.cc',
    ],

    # Note: GN tests converted to here, need to do the rest.
    'audio_modem_unittest_sources': [
      'audio_modem/audio_player_unittest.cc',
      'audio_modem/audio_recorder_unittest.cc',
      'audio_modem/modem_unittest.cc',
    ],
    'copresence_unittest_sources': [
      'copresence/copresence_state_unittest.cc',
      'copresence/handlers/audio/audio_directive_handler_unittest.cc',
      'copresence/handlers/audio/audio_directive_list_unittest.cc',
      'copresence/handlers/directive_handler_unittest.cc',
      'copresence/handlers/gcm_handler_unittest.cc',
      'copresence/rpc/http_post_unittest.cc',
      'copresence/rpc/rpc_handler_unittest.cc',
      'copresence/timed_map_unittest.cc',
    ],
    'enhanced_bookmarks_unittest_sources': [
      'enhanced_bookmarks/enhanced_bookmark_model_unittest.cc',
      'enhanced_bookmarks/image_store_ios_unittest.mm',
      'enhanced_bookmarks/image_store_unittest.cc',
      'enhanced_bookmarks/item_position_unittest.cc',
    ],
    'error_page_unittest_sources': [
      'error_page/renderer/net_error_helper_core_unittest.cc',
    ],
    'feedback_unittest_sources': [
      'feedback/feedback_common_unittest.cc',
      'feedback/feedback_data_unittest.cc',
      'feedback/feedback_uploader_chrome_unittest.cc',
      'feedback/feedback_uploader_unittest.cc',
    ],
    'gcm_driver_unittest_sources': [
      'gcm_driver/gcm_account_mapper_unittest.cc',
      'gcm_driver/gcm_channel_status_request_unittest.cc',
      'gcm_driver/gcm_client_impl_unittest.cc',
      'gcm_driver/gcm_delayed_task_controller_unittest.cc',
      'gcm_driver/gcm_driver_desktop_unittest.cc',
      'gcm_driver/gcm_stats_recorder_impl_unittest.cc',
    ],
    'google_unittest_sources': [
      'google/core/browser/google_url_tracker_unittest.cc',
      'google/core/browser/google_util_unittest.cc',
    ],
    'history_unittest_sources': [
      'history/core/browser/android/android_history_types_unittest.cc',
      'history/core/browser/history_types_unittest.cc',
      'history/core/browser/in_memory_url_index_types_unittest.cc',
      'history/core/browser/top_sites_cache_unittest.cc',
      'history/core/browser/top_sites_database_unittest.cc',
      'history/core/browser/url_database_unittest.cc',
      'history/core/browser/url_utils_unittest.cc',
      'history/core/browser/visit_database_unittest.cc',
      'history/core/browser/visit_filter_unittest.cc',
      'history/core/browser/visit_tracker_unittest.cc',
      'history/core/common/thumbnail_score_unittest.cc',
    ],
    'invalidation_unittest_sources': [
      'invalidation/fake_invalidator_unittest.cc',
      'invalidation/gcm_network_channel_unittest.cc',
      'invalidation/invalidation_logger_unittest.cc',
      'invalidation/invalidation_notifier_unittest.cc',
      'invalidation/invalidator_registrar_unittest.cc',
      'invalidation/non_blocking_invalidator_unittest.cc',
      'invalidation/object_id_invalidation_map_unittest.cc',
      'invalidation/p2p_invalidator_unittest.cc',
      'invalidation/push_client_channel_unittest.cc',
      'invalidation/registration_manager_unittest.cc',
      'invalidation/single_object_invalidation_set_unittest.cc',
      'invalidation/sync_invalidation_listener_unittest.cc',
      'invalidation/sync_system_resources_unittest.cc',
      'invalidation/ticl_invalidation_service_unittest.cc',
      'invalidation/unacked_invalidation_set_unittest.cc',
    ],
    'json_schema_unittest_sources': [
      'json_schema/json_schema_validator_unittest.cc',
      'json_schema/json_schema_validator_unittest_base.cc',
      'json_schema/json_schema_validator_unittest_base.h',
    ],
    'keyed_service_unittest_sources': [
      'keyed_service/content/browser_context_dependency_manager_unittest.cc',
      'keyed_service/core/dependency_graph_unittest.cc',
    ],
    'language_usage_metrics_unittest_sources': [
      'language_usage_metrics/language_usage_metrics_unittest.cc',
    ],
    'leveldb_proto_unittest_sources': [
      'leveldb_proto/proto_database_impl_unittest.cc',
    ],
    'login_unittest_sources': [
      'login/screens/screen_context_unittest.cc',
    ],
    'metrics_unittest_sources': [
      'metrics/compression_utils_unittest.cc',
      'metrics/daily_event_unittest.cc',
      'metrics/gpu/gpu_metrics_provider_unittest.cc',
      'metrics/histogram_encoder_unittest.cc',
      'metrics/histogram_manager_unittest.cc',
      'metrics/machine_id_provider_win_unittest.cc',
      'metrics/metrics_hashes_unittest.cc',
      'metrics/metrics_log_manager_unittest.cc',
      'metrics/metrics_log_unittest.cc',
      'metrics/metrics_reporting_scheduler_unittest.cc',
      'metrics/metrics_service_unittest.cc',
      'metrics/metrics_state_manager_unittest.cc',
      'metrics/net/net_metrics_log_uploader_unittest.cc',
      'metrics/persisted_logs_unittest.cc',
      'metrics/profiler/profiler_metrics_provider_unittest.cc',
    ],
    'nacl_unittest_sources': [
      'nacl/browser/nacl_file_host_unittest.cc',
      'nacl/browser/nacl_process_host_unittest.cc',
      'nacl/browser/nacl_validation_cache_unittest.cc',
      'nacl/browser/pnacl_host_unittest.cc',
      'nacl/browser/pnacl_translation_cache_unittest.cc',
      'nacl/browser/test_nacl_browser_delegate.cc',
      'nacl/zygote/nacl_fork_delegate_linux_unittest.cc',
    ],
    'navigation_interception_unittest_sources': [
      'navigation_interception/intercept_navigation_resource_throttle_unittest.cc',
    ],
    'network_hints_unittest_sources': [
      'network_hints/renderer/dns_prefetch_queue_unittest.cc',
      'network_hints/renderer/renderer_dns_prefetch_unittest.cc',
    ],
    'network_time_unittest_sources': [
      'network_time/network_time_tracker_unittest.cc',
    ],
    'omnibox_unittest_sources': [
      'omnibox/answers_cache_unittest.cc',
      'omnibox/autocomplete_input_unittest.cc',
      'omnibox/autocomplete_match_unittest.cc',
      'omnibox/autocomplete_result_unittest.cc',
      'omnibox/base_search_provider_unittest.cc',
      'omnibox/keyword_provider_unittest.cc',
      'omnibox/omnibox_field_trial_unittest.cc',
      'omnibox/suggestion_answer_unittest.cc',
    ],
    'os_crypt_unittest_sources': [
      'os_crypt/ie7_password_win_unittest.cc',
      'os_crypt/keychain_password_mac_unittest.mm',
      'os_crypt/os_crypt_unittest.cc',
    ],
    'ownership_unittest_sources': [
      'ownership/owner_key_util_impl_unittest.cc',
    ],
    'packed_ct_ev_whitelist_unittest_sources': [
      'packed_ct_ev_whitelist/bit_stream_reader_unittest.cc',
      'packed_ct_ev_whitelist/packed_ct_ev_whitelist_unittest.cc',
    ],
    'password_manager_unittest_sources': [
      'password_manager/content/browser/credential_manager_dispatcher_unittest.cc',
      'password_manager/content/common/credential_manager_types_unittest.cc',
      'password_manager/core/browser/affiliation_backend_unittest.cc',
      'password_manager/core/browser/affiliation_database_unittest.cc',
      'password_manager/core/browser/affiliation_fetch_throttler_unittest.cc',
      'password_manager/core/browser/affiliation_fetcher_unittest.cc',
      'password_manager/core/browser/affiliation_service_unittest.cc',
      'password_manager/core/browser/affiliation_utils_unittest.cc',
      'password_manager/core/browser/browser_save_password_progress_logger_unittest.cc',
      'password_manager/core/browser/export/csv_writer_unittest.cc',
      'password_manager/core/browser/import/csv_reader_unittest.cc',
      'password_manager/core/browser/log_router_unittest.cc',
      'password_manager/core/browser/login_database_unittest.cc',
      'password_manager/core/browser/password_autofill_manager_unittest.cc',
      'password_manager/core/browser/password_form_manager_unittest.cc',
      'password_manager/core/browser/password_generation_manager_unittest.cc',
      'password_manager/core/browser/password_manager_unittest.cc',
      'password_manager/core/browser/password_store_default_unittest.cc',
      'password_manager/core/browser/password_store_unittest.cc',
      'password_manager/core/browser/password_syncable_service_unittest.cc',
      'password_manager/core/browser/psl_matching_helper_unittest.cc',
    ],
    'policy_unittest_sources': [
      'policy/core/browser/autofill_policy_handler_unittest.cc',
      'policy/core/browser/browser_policy_connector_unittest.cc',
      'policy/core/browser/configuration_policy_handler_unittest.cc',
      'policy/core/browser/configuration_policy_pref_store_unittest.cc',
      'policy/core/browser/managed_bookmarks_tracker_unittest.cc',
      'policy/core/browser/url_blacklist_policy_handler_unittest.cc',
      'policy/core/common/async_policy_provider_unittest.cc',
      'policy/core/common/cloud/cloud_policy_client_unittest.cc',
      'policy/core/common/cloud/cloud_policy_constants_unittest.cc',
      'policy/core/common/cloud/cloud_policy_core_unittest.cc',
      'policy/core/common/cloud/cloud_policy_manager_unittest.cc',
      'policy/core/common/cloud/cloud_policy_refresh_scheduler_unittest.cc',
      'policy/core/common/cloud/cloud_policy_service_unittest.cc',
      'policy/core/common/cloud/cloud_policy_validator_unittest.cc',
      'policy/core/common/cloud/component_cloud_policy_service_unittest.cc',
      'policy/core/common/cloud/component_cloud_policy_store_unittest.cc',
      'policy/core/common/cloud/component_cloud_policy_updater_unittest.cc',
      'policy/core/common/cloud/device_management_service_unittest.cc',
      'policy/core/common/cloud/external_policy_data_fetcher_unittest.cc',
      'policy/core/common/cloud/external_policy_data_updater_unittest.cc',
      'policy/core/common/cloud/policy_header_io_helper_unittest.cc',
      'policy/core/common/cloud/policy_header_service_unittest.cc',
      'policy/core/common/cloud/resource_cache_unittest.cc',
      'policy/core/common/cloud/user_cloud_policy_manager_unittest.cc',
      'policy/core/common/cloud/user_cloud_policy_store_unittest.cc',
      'policy/core/common/cloud/user_info_fetcher_unittest.cc',
      'policy/core/common/config_dir_policy_loader_unittest.cc',
      'policy/core/common/generate_policy_source_unittest.cc',
      'policy/core/common/policy_bundle_unittest.cc',
      'policy/core/common/policy_loader_ios_unittest.mm',
      'policy/core/common/policy_loader_mac_unittest.cc',
      'policy/core/common/policy_loader_win_unittest.cc',
      'policy/core/common/policy_map_unittest.cc',
      'policy/core/common/policy_provider_android_unittest.cc',
      'policy/core/common/policy_service_impl_unittest.cc',
      'policy/core/common/policy_statistics_collector_unittest.cc',
      'policy/core/common/preg_parser_win_unittest.cc',
      'policy/core/common/registry_dict_win_unittest.cc',
      'policy/core/common/remote_commands/remote_commands_queue_unittest.cc',
      'policy/core/common/schema_map_unittest.cc',
      'policy/core/common/schema_registry_tracking_policy_provider_unittest.cc',
      'policy/core/common/schema_registry_unittest.cc',
      'policy/core/common/schema_unittest.cc',
    ],
    'power_unittest_sources': [
      'power/origin_power_map_unittest.cc',
    ],
    'precache_unittest_sources': [
      'precache/content/precache_manager_unittest.cc',
      'precache/core/precache_database_unittest.cc',
      'precache/core/precache_fetcher_unittest.cc',
      'precache/core/precache_url_table_unittest.cc',
    ],
    'proximity_auth_unittest_sources': [
      'proximity_auth/base64url_unittest.cc',
      'proximity_auth/bluetooth_connection_finder_unittest.cc',
      'proximity_auth/bluetooth_connection_unittest.cc',
      'proximity_auth/client_unittest.cc',
      'proximity_auth/connection_unittest.cc',
      'proximity_auth/cryptauth/cryptauth_account_token_fetcher_unittest.cc',
      'proximity_auth/cryptauth/cryptauth_api_call_flow_unittest.cc',
      'proximity_auth/cryptauth/cryptauth_client_unittest.cc',
      'proximity_auth/proximity_auth_system_unittest.cc',
      'proximity_auth/remote_status_update_unittest.cc',
      'proximity_auth/wire_message_unittest.cc',
    ],
    'query_parser_unittest_sources': [
      'query_parser/query_parser_unittest.cc',
      'query_parser/snippet_unittest.cc',
    ],
    'rappor_unittest_sources': [
      'rappor/bloom_filter_unittest.cc',
      'rappor/byte_vector_utils_unittest.cc',
      'rappor/log_uploader_unittest.cc',
      'rappor/rappor_metric_unittest.cc',
      'rappor/rappor_prefs_unittest.cc',
      'rappor/rappor_service_unittest.cc',
    ],
    'search_unittest_sources': [
      'search/search_android_unittest.cc',
      'search/search_unittest.cc',
      'search_engines/default_search_manager_unittest.cc',
      'search_engines/default_search_policy_handler_unittest.cc',
      'search_engines/keyword_table_unittest.cc',
      'search_engines/search_host_to_urls_map_unittest.cc',
      'search_engines/template_url_prepopulate_data_unittest.cc',
      'search_engines/template_url_service_util_unittest.cc',
      'search_engines/template_url_unittest.cc',
    ],
    'search_provider_logos_unittest_sources': [
      'search_provider_logos/logo_cache_unittest.cc',
      'search_provider_logos/logo_tracker_unittest.cc',
    ],
    'sessions_unittest_sources': [
      'sessions/content/content_serialized_navigation_builder_unittest.cc',
      'sessions/content/content_serialized_navigation_driver_unittest.cc',
      'sessions/ios/ios_serialized_navigation_builder_unittest.cc',
      'sessions/ios/ios_serialized_navigation_driver_unittest.cc',
      'sessions/serialized_navigation_entry_unittest.cc',
      'sessions/session_backend_unittest.cc',
      'sessions/session_types_unittest.cc',
    ],
    'signin_unittest_sources': [
      'signin/core/browser/account_tracker_service_unittest.cc',
      'signin/core/browser/mutable_profile_oauth2_token_service_unittest.cc',
      'signin/core/browser/refresh_token_annotation_request_unittest.cc',
      'signin/core/browser/signin_error_controller_unittest.cc',
      'signin/core/browser/webdata/token_service_table_unittest.cc',
      'signin/ios/browser/profile_oauth2_token_service_ios_unittest.mm',
    ],
    'storage_monitor_unittest_sources': [
      'storage_monitor/image_capture_device_manager_unittest.mm',
      'storage_monitor/media_storage_util_unittest.cc',
      'storage_monitor/media_transfer_protocol_device_observer_linux_unittest.cc',
      'storage_monitor/storage_info_unittest.cc',
      'storage_monitor/storage_monitor_chromeos_unittest.cc',
      'storage_monitor/storage_monitor_linux_unittest.cc',
      'storage_monitor/storage_monitor_mac_unittest.mm',
      'storage_monitor/storage_monitor_unittest.cc',
      'storage_monitor/storage_monitor_win_unittest.cc',
    ],
    'suggestions_unittest_sources': [
      'suggestions/blacklist_store_unittest.cc',
      'suggestions/image_manager_unittest.cc',
      'suggestions/suggestions_service_unittest.cc',
      'suggestions/suggestions_store_unittest.cc',
    ],
    'sync_driver_unittest_sources': [
      'sync_driver/data_type_manager_impl_unittest.cc',
      'sync_driver/device_info_data_type_controller_unittest.cc',
      'sync_driver/device_info_sync_service_unittest.cc',
      'sync_driver/generic_change_processor_unittest.cc',
      'sync_driver/model_association_manager_unittest.cc',
      'sync_driver/non_blocking_data_type_controller_unittest.cc',
      'sync_driver/non_ui_data_type_controller_unittest.cc',
      'sync_driver/shared_change_processor_unittest.cc',
      'sync_driver/sync_prefs_unittest.cc',
      'sync_driver/system_encryptor_unittest.cc',
      'sync_driver/ui_data_type_controller_unittest.cc',
    ],
    'translate_unittest_sources': [
      'translate/core/browser/language_state_unittest.cc',
      'translate/core/browser/translate_browser_metrics_unittest.cc',
      'translate/core/browser/translate_prefs_unittest.cc',
      'translate/core/browser/translate_script_unittest.cc',
      'translate/core/common/translate_metrics_unittest.cc',
      'translate/core/common/translate_util_unittest.cc',
      'translate/core/language_detection/language_detection_util_unittest.cc',
      'translate/ios/browser/js_translate_manager_unittest.mm',
      'translate/ios/browser/language_detection_controller_unittest.mm',
      'translate/ios/browser/translate_controller_unittest.mm',
    ],
    'ui_unittest_sources': [
      'ui/zoom/page_zoom_unittests.cc',
    ],
    'update_client_unittest_sources': [
      'update_client/test/component_patcher_unittest.cc',
      'update_client/test/crx_downloader_unittest.cc',
      'update_client/test/ping_manager_unittest.cc',
      'update_client/test/request_sender_unittest.cc',
      'update_client/test/update_checker_unittest.cc',
      'update_client/test/update_response_unittest.cc',
      'update_client/update_query_params_unittest.cc',
    ],
    'url_fixer_unittest_sources': [
      'url_fixer/url_fixer_unittest.cc',
    ],
    'url_matcher_unittest_sources': [
      'url_matcher/regex_set_matcher_unittest.cc',
      'url_matcher/string_pattern_unittest.cc',
      'url_matcher/substring_set_matcher_unittest.cc',
      'url_matcher/url_matcher_factory_unittest.cc',
      'url_matcher/url_matcher_unittest.cc',
    ],
    'variations_unittest_sources': [
      'variations/active_field_trials_unittest.cc',
      'variations/caching_permuted_entropy_provider_unittest.cc',
      'variations/entropy_provider_unittest.cc',
      'variations/metrics_util_unittest.cc',
      'variations/net/variations_http_header_provider_unittest.cc',
      'variations/study_filtering_unittest.cc',
      'variations/variations_associated_data_unittest.cc',
      'variations/variations_seed_processor_unittest.cc',
      'variations/variations_seed_simulator_unittest.cc',
    ],
    'visitedlink_unittest_sources': [
      'visitedlink/test/visitedlink_unittest.cc',
    ],
    'wallpaper_unittest_sources': [
      'wallpaper/wallpaper_resizer_unittest.cc',
    ],
    'web_cache_unittest_sources': [
      'web_cache/browser/web_cache_manager_unittest.cc',
    ],
    'web_modal_unittest_sources': [
      'web_modal/web_contents_modal_dialog_manager_unittest.cc',
    ],
    'web_resource_unittest_sources': [
      'web_resource/eula_accepted_notifier_unittest.cc',
      'web_resource/resource_request_allowed_notifier_unittest.cc',
    ],
    'webdata_unittest_sources': [
      'webdata/common/web_database_migration_unittest.cc',
    ],
  },
  'conditions': [
    ['android_webview_build == 0', {
      'targets': [
        {
          # GN version: //components:components_tests_pak
          'target_name': 'components_tests_pak',
          'type': 'none',
          'dependencies': [
            '../ui/resources/ui_resources.gyp:ui_resources',
            '../ui/strings/ui_strings.gyp:ui_strings',
            'components_resources.gyp:components_resources',
            'components_strings.gyp:components_strings',
          ],
          'actions': [
            {
              'action_name': 'repack_components_tests_pak',
              'variables': {
                'pak_inputs': [
                  '<(SHARED_INTERMEDIATE_DIR)/components/components_resources.pak',
                  '<(SHARED_INTERMEDIATE_DIR)/components/strings/components_strings_en-US.pak',
                  '<(SHARED_INTERMEDIATE_DIR)/ui/resources/ui_resources_100_percent.pak',
                  '<(SHARED_INTERMEDIATE_DIR)/ui/resources/webui_resources.pak',
                  '<(SHARED_INTERMEDIATE_DIR)/ui/strings/app_locale_settings_en-US.pak',
                  '<(SHARED_INTERMEDIATE_DIR)/ui/strings/ui_strings_en-US.pak',
                ],
                'pak_output': '<(PRODUCT_DIR)/components_tests_resources.pak',
              },
              'includes': [ '../build/repack_action.gypi' ],
            },
          ],
          'direct_dependent_settings': {
            'mac_bundle_resources': [
              '<(PRODUCT_DIR)/components_tests_resources.pak',
            ],
          },
        },
        {
          # GN version: //components:components_unittests
          'target_name': 'components_unittests',
          'type': '<(gtest_target_type)',
          'sources': [
            'test/run_all_unittests.cc',

            '<@(auto_login_parser_unittest_sources)',
            '<@(autofill_unittest_sources)',
            '<@(bookmarks_unittest_sources)',
            '<@(browser_watcher_unittest_sources)',
            '<@(captive_portal_unittest_sources)',
            '<@(cloud_devices_unittest_sources)',
            '<@(content_settings_unittest_sources)',
            '<@(crash_unittest_sources)',
            '<@(crx_file_unittest_sources)',
            '<@(data_reduction_proxy_unittest_sources)',
            '<@(device_event_log_unittest_sources)',
            '<@(dom_distiller_unittest_sources)',
            '<@(domain_reliability_unittest_sources)',
            '<@(enhanced_bookmarks_unittest_sources)',
            '<@(favicon_base_unittest_sources)',
            # This should be in the !android && !ios list, but has to be here
            # due to a test bug making them order-dependent. crbug.com/462352
            '<@(feedback_unittest_sources)',
            '<@(gcm_driver_unittest_sources)',
            '<@(google_unittest_sources)',
            '<@(history_unittest_sources)',
            '<@(json_schema_unittest_sources)',
            '<@(keyed_service_unittest_sources)',
            '<@(language_usage_metrics_unittest_sources)',
            '<@(leveldb_proto_unittest_sources)',
            '<@(login_unittest_sources)',
            '<@(metrics_unittest_sources)',
            '<@(network_time_unittest_sources)',
            '<@(omnibox_unittest_sources)',
            '<@(os_crypt_unittest_sources)',
            '<@(ownership_unittest_sources)',
            '<@(packed_ct_ev_whitelist_unittest_sources)',
            '<@(password_manager_unittest_sources)',
            '<@(precache_unittest_sources)',
            '<@(query_parser_unittest_sources)',
            '<@(rappor_unittest_sources)',
            '<@(search_unittest_sources)',
            '<@(search_provider_logos_unittest_sources)',
            '<@(sessions_unittest_sources)',
            '<@(signin_unittest_sources)',
            '<@(suggestions_unittest_sources)',
            '<@(sync_driver_unittest_sources)',
            '<@(translate_unittest_sources)',
            '<@(update_client_unittest_sources)',
            '<@(url_fixer_unittest_sources)',
            '<@(url_matcher_unittest_sources)',
            '<@(variations_unittest_sources)',
            '<@(wallpaper_unittest_sources)',
            '<@(web_resource_unittest_sources)',
            '<@(webdata_unittest_sources)',
          ],
          'include_dirs': [
            '..',
          ],
          'dependencies': [
            '../base/base.gyp:base',
            '../base/base.gyp:base_prefs_test_support',
            '../base/base.gyp:test_support_base',
            # TODO(blundell): Eliminate the need for this dependency in code
            # that iOS shares. crbug.com/325243
            '../content/content_shell_and_tests.gyp:test_support_content',
            '../google_apis/google_apis.gyp:google_apis_test_support',
            '../jingle/jingle.gyp:notifier_test_util',
            '../net/net.gyp:net_test_support',
            '../sql/sql.gyp:test_support_sql',
            '../sync/sync.gyp:sync',
            '../sync/sync.gyp:test_support_sync_api',
            '../testing/gmock.gyp:gmock',
            '../testing/gtest.gyp:gtest',
            '../third_party/leveldatabase/leveldatabase.gyp:leveldatabase',
            '../third_party/libaddressinput/libaddressinput.gyp:libaddressinput_util',
            '../third_party/libjingle/libjingle.gyp:libjingle',
            '../third_party/libphonenumber/libphonenumber.gyp:libphonenumber',
            '../third_party/libxml/libxml.gyp:libxml',
            '../third_party/protobuf/protobuf.gyp:protobuf_lite',
            '../ui/base/ui_base.gyp:ui_base',
            '../ui/gfx/gfx.gyp:gfx',
            '../ui/gfx/gfx.gyp:gfx_test_support',
            '../ui/resources/ui_resources.gyp:ui_resources',
            '../ui/strings/ui_strings.gyp:ui_strings',
            '../url/url.gyp:url_lib',
            'components.gyp:auto_login_parser',
            'components.gyp:autofill_core_browser',
            'components.gyp:autofill_core_common',
            'components.gyp:autofill_core_test_support',
            'components.gyp:bookmarks_browser',
            'components.gyp:bookmarks_test_support',
            'components.gyp:captive_portal_test_support',
            'components.gyp:cloud_devices_common',
            'components.gyp:content_settings_core_browser',
            'components.gyp:content_settings_core_common',
            'components.gyp:content_settings_core_test_support',
            'components.gyp:crash_test_support',
            'components.gyp:crx_file',
            'components.gyp:data_reduction_proxy_core_browser',
            'components.gyp:data_reduction_proxy_core_common',
            'components.gyp:data_reduction_proxy_test_support',
            'components.gyp:device_event_log_component',
            'components.gyp:distilled_page_proto',
            'components.gyp:dom_distiller_core',
            'components.gyp:dom_distiller_test_support',
            'components.gyp:domain_reliability',
            'components.gyp:enhanced_bookmarks',
            'components.gyp:enhanced_bookmarks_test_support',
            'components.gyp:favicon_base',
            'components.gyp:feedback_component',
            'components.gyp:gcm_driver',
            'components.gyp:gcm_driver_test_support',
            'components.gyp:google_core_browser',
            'components.gyp:history_core_browser',
            'components.gyp:history_core_common',
            'components.gyp:history_core_test_support',
            'components.gyp:invalidation',
            'components.gyp:invalidation_test_support',
            'components.gyp:json_schema',
            'components.gyp:keyed_service_core',
            'components.gyp:language_usage_metrics',
            'components.gyp:leveldb_proto',
            'components.gyp:leveldb_proto_test_support',
            'components.gyp:login',
            'components.gyp:metrics',
            'components.gyp:metrics_gpu',
            'components.gyp:metrics_net',
            'components.gyp:metrics_profiler',
            'components.gyp:metrics_test_support',
            'components.gyp:network_time',
            'components.gyp:omnibox',
            'components.gyp:omnibox_test_support',
            'components.gyp:os_crypt',
            'components.gyp:ownership',
            'components.gyp:packed_ct_ev_whitelist',
            'components.gyp:password_manager_core_browser',
            'components.gyp:password_manager_core_browser',
            'components.gyp:password_manager_core_browser_test_support',
            'components.gyp:precache_core',
            'components.gyp:pref_registry_test_support',
            'components.gyp:query_parser',
            'components.gyp:rappor',
            'components.gyp:rappor_test_support',
            'components.gyp:search',
            'components.gyp:search_engines',
            'components.gyp:search_engines_test_support',
            'components.gyp:search_provider_logos',
            'components.gyp:sessions_test_support',
            'components.gyp:signin_core_browser',
            'components.gyp:signin_core_browser_test_support',
            'components.gyp:suggestions',
            'components.gyp:sync_driver_test_support',
            'components.gyp:translate_core_browser',
            'components.gyp:translate_core_common',
            'components.gyp:translate_core_language_detection',
            'components.gyp:ui_zoom',
            'components.gyp:update_client',
            'components.gyp:update_client_test_support',
            'components.gyp:url_fixer',
            'components.gyp:variations',
            'components.gyp:variations_http_provider',
            'components.gyp:wallpaper',
            'components.gyp:web_resource',
            'components.gyp:web_resource_test_support',
            'components_resources.gyp:components_resources',
            'components_strings.gyp:components_strings',
            'components_tests_pak',
          ],
          'conditions': [
            ['toolkit_views == 1', {
              'sources': [
                'bookmarks/browser/bookmark_node_data_unittest.cc',
                'constrained_window/constrained_window_views_unittest.cc',
              ],
              'dependencies': [
                '<(DEPTH)/ui/views/views.gyp:views_test_support',
                'components.gyp:constrained_window',
              ]
            }],
            ['OS=="win"', {
              'dependencies': [
                'components.gyp:browser_watcher',
                'components.gyp:browser_watcher_client',
              ]
            }],
            ['OS=="win" and component!="shared_library" and win_use_allocator_shim==1', {
              'dependencies': [
                '<(DEPTH)/base/allocator/allocator.gyp:allocator',
              ],
            }],
            [ 'cld_version==0 or cld_version==2', {
              'dependencies': [
                # Unit tests should always use statically-linked CLD data.
                '<(DEPTH)/third_party/cld_2/cld_2.gyp:cld2_static', ],
            }],
            ['OS != "ios"', {
              'sources': [
                '<@(error_page_unittest_sources)',
                '<@(navigation_interception_unittest_sources)',
                '<@(network_hints_unittest_sources)',
                '<@(power_unittest_sources)',
                '<@(storage_monitor_unittest_sources)',
                '<@(ui_unittest_sources)',
                '<@(visitedlink_unittest_sources)',
                '<@(web_cache_unittest_sources)',
                '<@(web_modal_unittest_sources)',
              ],
              'dependencies': [
                '../skia/skia.gyp:skia',
                'components.gyp:autofill_content_browser',
                'components.gyp:autofill_content_renderer',
                'components.gyp:autofill_content_test_support',
                'components.gyp:data_reduction_proxy_content_browser',
                'components.gyp:dom_distiller_content',
                'components.gyp:error_page_renderer',
                'components.gyp:history_content_browser',
                'components.gyp:keyed_service_content',
                'components.gyp:navigation_interception',
                'components.gyp:network_hints_renderer',
                'components.gyp:password_manager_content_browser',
                'components.gyp:password_manager_content_common',
                'components.gyp:power',
                'components.gyp:precache_content',
                'components.gyp:sessions_content',
                'components.gyp:storage_monitor',
                'components.gyp:storage_monitor_test_support',
                'components.gyp:url_matcher',
                'components.gyp:visitedlink_browser',
                'components.gyp:visitedlink_renderer',
                'components.gyp:web_cache_browser',
                'components.gyp:web_modal',
                'components.gyp:web_modal_test_support',
              ],
            }, { # 'OS == "ios"'
              'sources': [
                'open_from_clipboard/clipboard_recent_content_ios_unittest.mm',
                'webp_transcode/webp_decoder_unittest.mm',
              ],
              'sources!': [
                'metrics/gpu/gpu_metrics_provider_unittest.cc',
                'signin/core/browser/mutable_profile_oauth2_token_service_unittest.cc',
                # This shouldn't be necessary, but the tests have to be in the main list
                # due to a test bug making them order-dependent. crbug.com/462352
                '<@(feedback_unittest_sources)',
              ],
              'sources/': [
                # Exclude all tests that depends on //content (based on layered-
                # component directory structure).
                ['exclude', '^[^/]*/content/'],
              ],
              'dependencies': [
                '../ios/ios_tests.gyp:test_support_ios',
                '../ios/web/ios_web.gyp:test_support_ios_web',
                '../third_party/ocmock/ocmock.gyp:ocmock',
                'components.gyp:open_from_clipboard',
                'components.gyp:sessions_ios',
                'components.gyp:signin_ios_browser',
                'components.gyp:translate_ios_browser',
                'components.gyp:webp_transcode',
              ],
              'actions': [
                {
                  'action_name': 'copy_test_data',
                  'variables': {
                    'test_data_files': [
                      'test/data',
                    ],
                    'test_data_prefix': 'components',
                  },
                  'includes': [ '../build/copy_test_data_ios.gypi' ],
                },
              ],
              'conditions': [
                ['configuration_policy==1', {
                  'sources/': [
                    ['include', '^policy/'],
                  ],
                }],
              ],
            }],
            ['disable_nacl==0', {
              'sources': [
                '<@(nacl_unittest_sources)',
              ],
              'dependencies': [
                'nacl.gyp:nacl_browser',
                'nacl.gyp:nacl_common',
              ],
            }],
            ['OS == "mac"', {
              'link_settings': {
                'libraries': [
                  '$(SDKROOT)/System/Library/Frameworks/AddressBook.framework',
                  '$(SDKROOT)/System/Library/Frameworks/Foundation.framework',
                  '$(SDKROOT)/System/Library/Frameworks/ImageCaptureCore.framework',
                ],
              },
              'sources!': [
                'password_manager/core/browser/password_store_default_unittest.cc',
              ],
            }],
            ['OS == "android"', {
              'sources': [
                'data_reduction_proxy/content/browser/data_reduction_proxy_debug_blocking_page_unittest.cc',
                'data_reduction_proxy/content/browser/data_reduction_proxy_debug_resource_throttle_unittest.cc',
                'data_reduction_proxy/content/browser/data_reduction_proxy_debug_ui_manager_unittest.cc',
                'invalidation/invalidation_logger_unittest.cc',
                'invalidation/invalidation_service_android_unittest.cc',
              ],
              'sources!': [
                # This shouldn't be necessary, but the tests have to be in the main list
                # due to a test bug making them order-dependent. crbug.com/462352
                '<@(feedback_unittest_sources)',
                'gcm_driver/gcm_account_mapper_unittest.cc',
                'gcm_driver/gcm_channel_status_request_unittest.cc',
                'gcm_driver/gcm_client_impl_unittest.cc',
                'gcm_driver/gcm_delayed_task_controller_unittest.cc',
                'gcm_driver/gcm_driver_desktop_unittest.cc',
                'gcm_driver/gcm_stats_recorder_impl_unittest.cc',
                'sessions/session_backend_unittest.cc',
                'signin/core/browser/mutable_profile_oauth2_token_service_unittest.cc',
                'storage_monitor/media_storage_util_unittest.cc',
                'storage_monitor/storage_info_unittest.cc',
                'storage_monitor/storage_monitor_unittest.cc',
                'web_modal/web_contents_modal_dialog_manager_unittest.cc',
              ],
              'dependencies': [
                'components.gyp:data_reduction_proxy_content',
                '../testing/android/native_test.gyp:native_test_native_code',
              ],
              'dependencies!': [
                'components.gyp:feedback_component',
                'components.gyp:storage_monitor',
                'components.gyp:storage_monitor_test_support',
                'components.gyp:web_modal',
                'components.gyp:web_modal_test_support',
              ],
            }],
            ['OS != "android"', {
              'sources': [
                '<@(invalidation_unittest_sources)',
              ],
            }],
            ['OS != "ios" and OS != "android"', {
              'sources': [
                '<@(audio_modem_unittest_sources)',
                '<@(copresence_unittest_sources)',
                '<@(proximity_auth_unittest_sources)',
              ],
              'dependencies': [
                '../device/bluetooth/bluetooth.gyp:device_bluetooth_mocks',
                '../google_apis/google_apis.gyp:google_apis_test_support',
                '../third_party/protobuf/protobuf.gyp:protobuf_lite',
                'components.gyp:audio_modem',
                'components.gyp:audio_modem_test_support',
                'components.gyp:copresence',
                'components.gyp:copresence_test_support',
                'components.gyp:cryptauth',
                'components.gyp:proximity_auth',
              ],
            }],
            ['chromeos==1', {
              'sources': [
                'pairing/message_buffer_unittest.cc',
                'timers/alarm_timer_unittest.cc',
                'wifi_sync/wifi_config_delegate_chromeos_unittest.cc',
                'wifi_sync/wifi_credential_syncable_service_unittest.cc',
                'wifi_sync/wifi_credential_unittest.cc',
                'wifi_sync/wifi_security_class_chromeos_unittest.cc',
                'wifi_sync/wifi_security_class_unittest.cc',
              ],
              'sources!': [
                'storage_monitor/storage_monitor_linux_unittest.cc',
              ],
              'dependencies': [
                '../chromeos/chromeos.gyp:chromeos_test_support',
                'components.gyp:pairing',
                'components.gyp:user_manager_test_support',
                'components.gyp:wifi_sync',
              ],
            }],
            ['OS=="linux"', {
              'sources': [
                'metrics/serialization/serialization_utils_unittest.cc',
              ],
              'dependencies': [
                'components.gyp:metrics_serialization',
                '../dbus/dbus.gyp:dbus',
                '../device/media_transfer_protocol/media_transfer_protocol.gyp:device_media_transfer_protocol',
              ],
            }],
            ['OS=="linux" and use_udev==0', {
              'dependencies!': [
                'components.gyp:storage_monitor',
                'components.gyp:storage_monitor_test_support',
              ],
              'sources/': [
                ['exclude', '^storage_monitor/'],
              ],
            }],
            ['OS=="win" and win_use_allocator_shim==1', {
              'dependencies': [
                '../base/allocator/allocator.gyp:allocator',
              ],
            }],
            ['OS=="linux" and component=="shared_library" and use_allocator!="none"', {
              'dependencies': [
                '<(DEPTH)/base/allocator/allocator.gyp:allocator',
              ],
              'link_settings': {
                'ldflags': ['-rdynamic'],
              },
            }],
            ['configuration_policy==1', {
              'dependencies': [
                'components.gyp:policy_component',
                'components.gyp:policy_component_test_support',
                'components.gyp:policy_test_support',
              ],
              'sources': [
                '<@(policy_unittest_sources)',
                'search_engines/default_search_policy_handler_unittest.cc',
              ],
              'conditions': [
                ['OS=="android"', {
                  'sources/': [
                    ['exclude', '^policy/core/common/async_policy_provider_unittest\\.cc'],
                  ],
                }],
                ['OS=="android" or OS=="ios"', {
                  # Note: 'sources!' is processed before any 'sources/', so the
                  # ['include', '^policy/'] on iOS above will include all of the
                  # policy source files again. Using 'source/' here too will get
                  # these files excluded as expected.
                  'sources/': [
                    ['exclude', '^policy/core/common/cloud/component_cloud_policy_service_unittest\\.cc'],
                    ['exclude', '^policy/core/common/cloud/component_cloud_policy_store_unittest\\.cc'],
                    ['exclude', '^policy/core/common/cloud/component_cloud_policy_updater_unittest\\.cc'],
                    ['exclude', '^policy/core/common/cloud/external_policy_data_fetcher_unittest\\.cc'],
                    ['exclude', '^policy/core/common/cloud/external_policy_data_updater_unittest\\.cc'],
                    ['exclude', '^policy/core/common/cloud/resource_cache_unittest\\.cc'],
                    ['exclude', '^policy/core/common/config_dir_policy_loader_unittest\\.cc'],
                  ],
                }],
                ['chromeos==1', {
                  'sources': [
                    'policy/core/common/proxy_policy_provider_unittest.cc',
                  ],
                  'sources!': [
                    'policy/core/common/cloud/user_cloud_policy_manager_unittest.cc',
                    'policy/core/common/cloud/user_cloud_policy_store_unittest.cc',
                  ],
                }],
                ['OS=="ios" or OS=="mac"', {
                  'sources': [
                    'policy/core/common/mac_util_unittest.cc',
                  ],
                }],
              ],
            }],
          ],
          # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
          'msvs_disabled_warnings': [4267, ],
        },
      ],
    }],
    ['OS != "ios" and android_webview_build == 0', {
      'targets': [
        {
          # GN: //components:components_perftests
          'target_name': 'components_perftests',
          'type': '<(gtest_target_type)',
          'dependencies': [
            '../base/base.gyp:base',
            '../base/base.gyp:test_support_perf',
            '../content/content_shell_and_tests.gyp:test_support_content',
            '../testing/gtest.gyp:gtest',
            'components.gyp:visitedlink_browser',
          ],
         'include_dirs': [
           '..',
         ],
         'sources': [
           'visitedlink/test/visitedlink_perftest.cc',
         ],
         'conditions': [
           ['OS == "android"', {
             'dependencies': [
               '../testing/android/native_test.gyp:native_test_native_code',
             ],
           }],
           ['OS=="win" and component!="shared_library" and win_use_allocator_shim==1', {
             'dependencies': [
               '<(DEPTH)/base/allocator/allocator.gyp:allocator',
             ],
           }],
         ],
         # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
         'msvs_disabled_warnings': [ 4267, ],
        },
      ],
      'conditions': [
        ['OS == "android"', {
          'targets': [
            {
              'target_name': 'components_unittests_apk',
              'type': 'none',
              'dependencies': [
                'components_unittests',
                'components.gyp:invalidation_java',
              ],
              'variables': {
                'test_suite_name': 'components_unittests',
              },
              'includes': [ '../build/apk_test.gypi' ],
            },
          ],
        }],
      ],
    }],
    ['OS!="ios"', {
      'conditions': [
        ['test_isolation_mode != "noop"', {
          'targets': [
            {
              'target_name': 'components_browsertests_run',
              'type': 'none',
              'dependencies': [ 'components_browsertests' ],
              'includes': [
                '../build/isolate.gypi',
              ],
              'sources': [
                'components_browsertests.isolate',
              ],
              'conditions': [
                ['use_x11==1', {
                  'dependencies': [
                    '../tools/xdisplaycheck/xdisplaycheck.gyp:xdisplaycheck',
                  ],
                }],
              ],
            },
          ],
        }],
      ],
      'targets': [
        {
          'target_name': 'components_browsertests',
          'type': '<(gtest_target_type)',
          'defines!': ['CONTENT_IMPLEMENTATION'],
          'dependencies': [
            '../content/content.gyp:content_common',
            '../content/content.gyp:content_gpu',
            '../content/content.gyp:content_plugin',
            '../content/content.gyp:content_renderer',
            '../content/content_shell_and_tests.gyp:content_browser_test_support',
            '../content/content_shell_and_tests.gyp:content_shell_lib',
            '../content/content_shell_and_tests.gyp:content_shell_pak',
            '../content/content_shell_and_tests.gyp:test_support_content',
            '../skia/skia.gyp:skia',
            '../testing/gmock.gyp:gmock',
            '../testing/gtest.gyp:gtest',
            'components.gyp:autofill_content_browser',
            'components.gyp:autofill_content_renderer',
            'components.gyp:dom_distiller_content',
            'components.gyp:dom_distiller_core',
            'components.gyp:password_manager_content_renderer',
            'components.gyp:pref_registry_test_support',
            'components_resources.gyp:components_resources',
            'components_strings.gyp:components_strings',
            'components_tests_pak',
          ],
          'include_dirs': [
            '..',
          ],
          'defines': [
            'HAS_OUT_OF_PROC_TEST_RUNNER',
          ],
          'sources': [
            'autofill/content/browser/risk/fingerprint_browsertest.cc',
            'autofill/content/renderer/password_form_conversion_utils_browsertest.cc',
            'dom_distiller/content/distiller_page_web_contents_browsertest.cc',
            'password_manager/content/renderer/credential_manager_client_browsertest.cc',
          ],
          'conditions': [
            ['OS == "android"', {
              'sources!': [
                'autofill/content/browser/risk/fingerprint_browsertest.cc',
              ],
            }],
            ['OS == "linux"', {
              'sources': [
                  # content_extractor_browsertest is a standalone content extraction tool built as
                  # a MANUAL component_browsertest.
                  'dom_distiller/standalone/content_extractor_browsertest.cc',
                ],
            }],
            ['OS=="win"', {
              'resource_include_dirs': [
                '<(SHARED_INTERMEDIATE_DIR)/content/app/resources',
              ],
              'sources': [
                '../content/shell/app/resource.h',
                '../content/shell/app/shell.rc',
                # TODO:  It would be nice to have these pulled in
                # automatically from direct_dependent_settings in
                # their various targets (net.gyp:net_resources, etc.),
                # but that causes errors in other targets when
                # resulting .res files get referenced multiple times.
                '<(SHARED_INTERMEDIATE_DIR)/blink/public/resources/blink_resources.rc',
                '<(SHARED_INTERMEDIATE_DIR)/content/app/strings/content_strings_en-US.rc',
                '<(SHARED_INTERMEDIATE_DIR)/net/net_resources.rc',
              ],
              'dependencies': [
                '<(DEPTH)/content/app/resources/content_resources.gyp:content_resources',
                '<(DEPTH)/content/app/strings/content_strings.gyp:content_strings',
                '<(DEPTH)/net/net.gyp:net_resources',
                '<(DEPTH)/third_party/WebKit/public/blink_resources.gyp:blink_resources',
                '<(DEPTH)/third_party/iaccessible2/iaccessible2.gyp:iaccessible2',
                '<(DEPTH)/third_party/isimpledom/isimpledom.gyp:isimpledom',
              ],
              'configurations': {
                'Debug_Base': {
                  'msvs_settings': {
                    'VCLinkerTool': {
                      'LinkIncremental': '<(msvs_large_module_debug_link_mode)',
                    },
                  },
                },
              },
              # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
              'msvs_disabled_warnings': [ 4267, ],
            }],
            ['OS=="win" and win_use_allocator_shim==1', {
              'dependencies': [
                '../base/allocator/allocator.gyp:allocator',
              ],
            }],
            ['OS=="mac"', {
              'dependencies': [
        '../content/content_shell_and_tests.gyp:content_shell',  # Needed for Content Shell.app's Helper.
              ],
            }],
            ['enable_basic_printing==1 or enable_print_preview==1', {
              'dependencies': [
                'components.gyp:printing_test_support',
              ],
              'sources' : [
                'printing/test/print_web_view_helper_browsertest.cc',
              ],
            }]
          ],
        },
      ],
    }],
    ['test_isolation_mode != "noop"', {
      'targets': [
        {
          'target_name': 'components_unittests_run',
          'type': 'none',
          'dependencies': [
            'components_unittests',
          ],
          'includes': [
            '../build/isolate.gypi',
          ],
          'sources': [
            'components_unittests.isolate',
          ],
        },
      ],
    }],
  ],
}
