# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,

    'variables': {
      'version_py_path': 'tools/build/version.py',
      'version_path': 'VERSION',
    },
    'version_py_path': '<(version_py_path)',
    'version_path': '<(version_path)',
    'version_full':
        '<!(python <(version_py_path) -f <(version_path) -t "@MAJOR@.@MINOR@.@BUILD@.@PATCH@")',
    'version_build_patch':
        '<!(python <(version_py_path) -f <(version_path) -t "@BUILD@.@PATCH@")',

    # Define the common dependencies that contain all the actual
    # Chromium functionality.  This list gets pulled in below by
    # the link of the actual chrome (or chromium) executable on
    # Linux or Mac, and into chrome.dll on Windows.
    'chromium_dependencies': [
      'common',
      'browser',
      'debugger',
      'renderer',
      'syncapi',
      'utility',
      'profile_import',
      'worker',
      '../printing/printing.gyp:printing',
      '../webkit/webkit.gyp:inspector_resources',
    ],
    'allocator_target': '../base/allocator/allocator.gyp:allocator',
    'grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)/chrome',
    'protoc_out_dir': '<(SHARED_INTERMEDIATE_DIR)/protoc_out',
    'chrome_strings_grds': [
      # Localizable resources.
      'app/resources/locale_settings.grd',
      'app/chromium_strings.grd',
      'app/generated_resources.grd',
      'app/google_chrome_strings.grd',
    ],
    'chrome_resources_grds': [
      # Data resources.
      'browser/browser_resources.grd',
      'common/common_resources.grd',
      'renderer/renderer_resources.grd',
    ],
    'grit_info_cmd': ['python', '../tools/grit/grit_info.py',],
    'repack_locales_cmd': ['python', 'tools/build/repack_locales.py',],
    # TODO: remove this helper when we have loops in GYP
    'apply_locales_cmd': ['python', 'tools/build/apply_locales.py',],
    'browser_tests_sources': [
      'browser/autocomplete/autocomplete_browsertest.cc',
      'browser/browser_browsertest.cc',
      'browser/browser_init_browsertest.cc',
      'browser/crash_recovery_browsertest.cc',
      'browser/download/save_page_browsertest.cc',
      'browser/extensions/autoupdate_interceptor.cc',
      'browser/extensions/autoupdate_interceptor.h',
      'browser/extensions/browser_action_apitest.cc',
      'browser/extensions/content_script_all_frames_apitest.cc',
      'browser/extensions/cross_origin_xhr_apitest.cc',
      'browser/extensions/execute_script_apitest.cc',
      'browser/extensions/extension_apitest.cc',
      'browser/extensions/extension_apitest.h',
      'browser/extensions/extension_bookmarks_apitest.cc',
      'browser/extensions/extension_history_apitest.cc',
      'browser/extensions/extension_javascript_url_apitest.cc',
      'browser/extensions/extension_messages_apitest.cc',
      'browser/extensions/extension_browsertest.cc',
      'browser/extensions/extension_browsertest.h',
      'browser/extensions/extension_browsertests_misc.cc',
      'browser/extensions/extension_override_apitest.cc',
      'browser/extensions/extension_toolstrip_apitest.cc',
      'browser/extensions/isolated_world_apitest.cc',
      'browser/extensions/page_action_apitest.cc',
      'browser/extensions/stubs_apitest.cc',
      'browser/gtk/bookmark_manager_browsertest.cc',
      'browser/net/cookie_policy_browsertest.cc',
      'browser/net/ftp_browsertest.cc',
      'browser/privacy_blacklist/blacklist_manager_browsertest.cc',
      'browser/ssl/ssl_browser_tests.cc',
    ],
    'browser_tests_sources_gtk_specific': [
      'browser/gtk/view_id_util_browsertest.cc',
    ],
    'browser_tests_sources_views_specific': [
      'browser/views/find_bar_host_browsertest.cc',
    ],
    'browser_tests_sources_win_specific': [
      'browser/extensions/extension_devtools_browsertest.cc',
      'browser/extensions/extension_devtools_browsertest.h',
      'browser/extensions/extension_devtools_browsertests.cc',
      'browser/extensions/extension_shelf_model_unittest.cc',
      'browser/extensions/extension_startup_unittest.cc',
      'browser/extensions/extension_storage_apitest.cc',
      'browser/extensions/extension_tabs_apitest.cc',
      'browser/extensions/extension_i18n_apitest.cc',
      'browser/extensions/extension_popup_apitest.cc',
      # TODO(jam): http://crbug.com/15101 These tests fail on Linux and Mac.
      'browser/child_process_security_policy_browser_test.cc',
      'browser/renderer_host/test/web_cache_manager_browsertest.cc',
      'browser/renderer_host/test/render_view_host_manager_browsertest.cc',
      # TODO(jcampan): once the task manager works on Mac, move this test to the
      #                non win specific section.
      'browser/task_manager_browsertest.cc',
      'browser/views/browser_views_accessibility_browsertest.cc',
    ],
    'browser_tests_sources_exclude_on_mac': [
      'browser/extensions/browser_action_apitest.cc',
      'browser/extensions/content_script_all_frames_apitest.cc',
      'browser/extensions/cross_origin_xhr_apitest.cc',
      'browser/extensions/execute_script_apitest.cc',
      'browser/extensions/extension_apitest.cc',
      'browser/extensions/extension_apitest.h',
      'browser/extensions/extension_bookmarks_apitest.cc',
      'browser/extensions/extension_history_apitest.cc',
      'browser/extensions/extension_javascript_url_apitest.cc',
      'browser/extensions/extension_messages_apitest.cc',
      'browser/extensions/extension_browsertest.cc',
      'browser/extensions/extension_browsertest.h',
      'browser/extensions/extension_browsertests_misc.cc',
      'browser/extensions/extension_override_apitest.cc',
      'browser/extensions/extension_toolstrip_apitest.cc',
      'browser/extensions/isolated_world_apitest.cc',
      'browser/extensions/page_action_apitest.cc',
      'browser/extensions/stubs_apitest.cc',
      'browser/privacy_blacklist/blacklist_manager_browsertest.cc',
      'browser/ssl/ssl_browser_tests.cc',
    ],
    # TODO(jcampan): move these vars to views.gyp.
    'views_unit_tests_sources': [
      '../views/view_unittest.cc',
      '../views/focus/focus_manager_unittest.cc',
      '../views/controls/label_unittest.cc',
      '../views/controls/table/table_view_unittest.cc',
    ],
    'views_unit_tests_sources_win_specific': [
      # TODO(jcampan): make the following tests work on Linux.
      '../views/grid_layout_unittest.cc',
    ],
    'conditions': [
      ['OS=="win"', {
        'nacl_defines': [
          'NACL_WINDOWS=1',
          'NACL_LINUX=0',
          'NACL_OSX=0',
        ],
      },],
      ['OS=="linux"', {
        'nacl_defines': [
          'NACL_WINDOWS=0',
          'NACL_LINUX=1',
          'NACL_OSX=0',
        ],
      },],
      ['OS=="mac"', {
        'tweak_info_plist_path': 'tools/build/mac/tweak_info_plist',
        'nacl_defines': [
          'NACL_WINDOWS=0',
          'NACL_LINUX=0',
          'NACL_OSX=1',
        ],
        'conditions': [
          ['branding=="Chrome"', {
            'mac_bundle_id': 'com.google.Chrome',
            'mac_creator': 'rimZ',
          }, {  # else: branding!="Chrome"
            'mac_bundle_id': 'org.chromium.Chromium',
            'mac_creator': 'Cr24',
          }],  # branding
        ],  # conditions
      }],  # OS=="mac"
      ['target_arch=="ia32"', {
        'nacl_defines': [
          # TODO(gregoryd): consider getting this from NaCl's common.gypi
          'NACL_TARGET_SUBARCH=32',
          'NACL_BUILD_SUBARCH=32',
        ],
      }],
      ['target_arch=="x64"', {
        'nacl_defines': [
          # TODO(gregoryd): consider getting this from NaCl's common.gypi
          'NACL_TARGET_SUBARCH=64',
          'NACL_BUILD_SUBARCH=64',
        ],
      }],
    ],  # conditions
  },  # variables
  'includes': [
    # Place some targets in gypi files to reduce contention on this file.
    # By using an include, we keep everything in a single xcodeproj file.
    'chrome_browser.gypi',
  ],
  'targets': [
    {
      # TODO(mark): It would be better if each static library that needed
      # to run grit would list its own .grd files, but unfortunately some
      # of the static libraries currently have circular dependencies among
      # generated headers.
      'target_name': 'chrome_resources',
      'type': 'none',
      'msvs_guid': 'B95AB527-F7DB-41E9-AD91-EB51EE0F56BE',
      'variables': {
        'chrome_resources_inputs': [
          '<!@(<(grit_info_cmd) --inputs <(chrome_resources_grds))',
        ],
      },
      'rules': [
        {
          'rule_name': 'grit',
          'extension': 'grd',
          'variables': {
            'conditions': [
              ['branding=="Chrome"', {
                # TODO(mmoss) The .grd files look for _google_chrome, but for
                # consistency they should look for GOOGLE_CHROME_BUILD like C++.
                # Clean this up when Windows moves to gyp.
                'chrome_build': '_google_chrome',
                'branded_env': 'CHROMIUM_BUILD=google_chrome',
              }, {  # else: branding!="Chrome"
                'chrome_build': '_chromium',
                'branded_env': 'CHROMIUM_BUILD=chromium',
              }],
            ],
          },
          'inputs': [
            '<@(chrome_resources_inputs)',
          ],
          'outputs': [
            '<(grit_out_dir)/grit/<(RULE_INPUT_ROOT).h',
            '<(grit_out_dir)/<(RULE_INPUT_ROOT).pak',
            # TODO(bradnelson): move to something like this instead
            #'<!@(<(grit_info_cmd) --outputs \'<(grit_out_dir)\' <(chrome_resources_grds))',
            # This currently cannot work because gyp only evaluates the
            # outputs once (rather than per case where the rule applies).
            # This means you end up with all the outputs from all the grd
            # files, which angers scons and confuses vstudio.
            # One alternative would be to turn this into several actions,
            # but that would be rather verbose.
          ],
          'action': ['python', '../tools/grit/grit.py', '-i',
            '<(RULE_INPUT_PATH)',
            'build', '-o', '<(grit_out_dir)',
            '-D', '<(chrome_build)',
            '-E', '<(branded_env)',
          ],
          'conditions': [
            ['chromeos==1 or toolkit_views==1', {
              'action': ['-D', 'chromeos'],
            }],
            ['use_titlecase_in_grd_files==1', {
              'action': ['-D', 'use_titlecase'],
            }],
          ],
          'message': 'Generating resources from <(RULE_INPUT_PATH)',
        },
      ],
      'sources': [
        '<@(chrome_resources_grds)',
        '<@(chrome_resources_inputs)',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(grit_out_dir)',
        ],
      },
      'conditions': [
        ['OS=="win"', {
          'dependencies': ['../build/win/system.gyp:cygwin'],
        }],
      ],
    },
    {
      # TODO(mark): It would be better if each static library that needed
      # to run grit would list its own .grd files, but unfortunately some
      # of the static libraries currently have circular dependencies among
      # generated headers.
      'target_name': 'chrome_strings',
      'msvs_guid': 'D9DDAF60-663F-49CC-90DC-3D08CC3D1B28',
      'conditions': [
        ['OS=="win"', {
          # HACK(nsylvain): We want to enforce a fake dependency on
          # intaller_util_string.  install_util depends on both
          # chrome_strings and installer_util_strings, but for some reasons
          # Incredibuild does not enforce it (most likely a bug). By changing
          # the type and making sure we depend on installer_util_strings, it
          # will always get built before installer_util.
          'type': 'dummy_executable',
          'dependencies': ['../build/win/system.gyp:cygwin',
                           'installer/installer.gyp:installer_util_strings',],
        }, {
          'type': 'none',
        }],
      ],
      'variables': {
        'chrome_strings_inputs': [
          '<!@(<(grit_info_cmd) --inputs <(chrome_strings_grds))',
        ],
      },
      'rules': [
        {
          'rule_name': 'grit',
          'extension': 'grd',
          'variables': {
            'conditions': [
              ['branding=="Chrome"', {
                # TODO(mmoss) The .grd files look for _google_chrome, but for
                # consistency they should look for GOOGLE_CHROME_BUILD like C++.
                # Clean this up when Windows moves to gyp.
                'chrome_build': '_google_chrome',
              }, {  # else: branding!="Chrome"
                'chrome_build': '_chromium',
              }],
            ],
          },
          'inputs': [
            '<@(chrome_strings_inputs)',
          ],
          'outputs': [
            '<(grit_out_dir)/grit/<(RULE_INPUT_ROOT).h',
            # TODO: remove this helper when we have loops in GYP
            '>!@(<(apply_locales_cmd) \'<(grit_out_dir)/<(RULE_INPUT_ROOT)_ZZLOCALE.pak\' <(locales))',
            # TODO(bradnelson): move to something like this
            #'<!@(<(grit_info_cmd) --outputs \'<(grit_out_dir)\' <(chrome_strings_grds))',
            # See comment in chrome_resources as to why.
          ],
          'action': ['python', '../tools/grit/grit.py', '-i',
                    '<(RULE_INPUT_PATH)',
                    'build', '-o', '<(grit_out_dir)',
                    '-D', '<(chrome_build)'],
          'conditions': [
            ['chromeos==1 or toolkit_views==1', {
              'action': ['-D', 'chromeos'],
            }],
            ['use_titlecase_in_grd_files==1', {
              'action': ['-D', 'use_titlecase'],
            }],
          ],
          'message': 'Generating resources from <(RULE_INPUT_PATH)',
        },
      ],
      'sources': [
        '<@(chrome_strings_grds)',
        '<@(chrome_strings_inputs)',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(grit_out_dir)',
        ],
      },
    },
    {
      # theme_resources also generates a .cc file, so it can't use the rules above.
      'target_name': 'theme_resources',
      'type': 'none',
      'msvs_guid' : 'A158FB0A-25E4-6523-6B5A-4BB294B73D31',
      'variables': {
        'grit_path': '../tools/grit/grit.py',
      },
      'actions': [
        {
          'action_name': 'theme_resources',
          'variables': {
            'input_path': 'app/theme/theme_resources.grd',
            'conditions': [
              ['branding=="Chrome"', {
                # TODO(mmoss) The .grd files look for _google_chrome, but for
                # consistency they should look for GOOGLE_CHROME_BUILD like C++.
                # Clean this up when Windows moves to gyp.
                'chrome_build': '_google_chrome',
              }, {  # else: branding!="Chrome"
                'chrome_build': '_chromium',
              }],
            ],
          },
          'inputs': [
            '<!@(<(grit_info_cmd) --inputs <(input_path))',
          ],
          'outputs': [
            '<!@(<(grit_info_cmd) --outputs \'<(grit_out_dir)\' <(input_path))',
          ],
          'action': [
            'python', '<(grit_path)',
            '-i', '<(input_path)', 'build',
            '-o', '<(grit_out_dir)',
            '-D', '<(chrome_build)'
          ],
          'conditions': [
            ['chromeos==1 or toolkit_views==1', {
              'action': ['-D', 'chromeos'],
            }],
            ['use_titlecase_in_grd_files==1', {
              'action': ['-D', 'use_titlecase'],
            }],
          ],
          'message': 'Generating resources from <(input_path)',
        },
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(grit_out_dir)',
        ],
      },
      'conditions': [
        ['OS=="win"', {
          'dependencies': ['../build/win/system.gyp:cygwin'],
        }],
      ],
    },
    {
      'target_name': 'default_extensions',
      'type': 'none',
      'msvs_guid': 'DA9BAB64-91DC-419B-AFDE-6FF8C569E83A',
      'conditions': [
        ['OS=="win"', {
          'copies': [
            {
              'destination': '<(PRODUCT_DIR)/extensions',
              'files': [
                'browser/extensions/default_extensions/external_extensions.json'
              ]
            }
          ],
        }],
      ],
    },
     {
      'target_name': 'common_constants',
      'type': '<(library)',
      'dependencies': [
        '../base/base.gyp:base',
      ],
      'conditions': [
        ['OS=="linux"', {
          'dependencies': ['../build/linux/system.gyp:gtk'],
        }],
      ],
      'sources': [
        'common/chrome_constants.cc',
        'common/chrome_constants.h',
        'common/chrome_paths.cc',
        'common/chrome_paths.h',
        'common/chrome_paths_internal.h',
        'common/chrome_paths_linux.cc',
        'common/chrome_paths_mac.mm',
        'common/chrome_paths_win.cc',
        'common/chrome_switches.cc',
        'common/chrome_switches.h',
        'common/env_vars.cc',
        'common/env_vars.h',
        'common/json_value_serializer.cc',
        'common/json_value_serializer.h',
        'common/pref_names.cc',
        'common/pref_names.h',
      ],
      'actions': [
        {
          'action_name': 'Make chrome_version.cc',
          'variables': {
            'make_version_cc_path': 'tools/build/make_version_cc.py',
          },
          'inputs': [
            '<(make_version_cc_path)',
            'VERSION',
          ],
          'outputs': [
            '<(INTERMEDIATE_DIR)/chrome_version.cc',
          ],
          'action': [
            'python',
            '<(make_version_cc_path)',
            '<@(_outputs)',
            '<(version_full)',
          ],
          'process_outputs_as_sources': 1,
        },
      ],
    },
    {
      'target_name': 'common',
      'type': '<(library)',
      'msvs_guid': '899F1280-3441-4D1F-BA04-CCD6208D9146',
      'dependencies': [
        'common_constants',
        'chrome_resources',
        'chrome_strings',
        'theme_resources',
        '../app/app.gyp:app_base',
        '../app/app.gyp:app_resources',
        '../base/base.gyp:base',
        '../base/base.gyp:base_i18n',
        '../build/temp_gyp/googleurl.gyp:googleurl',
        '../ipc/ipc.gyp:ipc',
        '../net/net.gyp:net',
        '../net/net.gyp:net_resources',
        '../skia/skia.gyp:skia',
        '../third_party/icu/icu.gyp:icui18n',
        '../third_party/icu/icu.gyp:icuuc',
        '../third_party/libxml/libxml.gyp:libxml',
        '../third_party/sqlite/sqlite.gyp:sqlite',
        '../third_party/zlib/zlib.gyp:zlib',
        '../third_party/npapi/npapi.gyp:npapi',
        '../webkit/webkit.gyp:appcache',
        '../webkit/webkit.gyp:glue',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        # All .cc, .h, and .mm files under chrome/common except for tests.
        'common/desktop_notifications/active_notification_tracker.h',
        'common/desktop_notifications/active_notification_tracker.cc',
        'common/extensions/extension.cc',
        'common/extensions/extension.h',
        'common/extensions/extension_constants.cc',
        'common/extensions/extension_constants.h',
        'common/extensions/extension_error_reporter.cc',
        'common/extensions/extension_error_reporter.h',
        'common/extensions/extension_error_utils.cc',
        'common/extensions/extension_error_utils.h',
        'common/extensions/extension_action.cc',
        'common/extensions/extension_action.h',
        'common/extensions/extension_l10n_util.cc',
        'common/extensions/extension_l10n_util.h',
        'common/extensions/extension_message_bundle.cc',
        'common/extensions/extension_message_bundle.h',
        'common/extensions/extension_resource.cc',
        'common/extensions/extension_resource.h',
        'common/extensions/extension_unpacker.cc',
        'common/extensions/extension_unpacker.h',
        'common/extensions/update_manifest.cc',
        'common/extensions/update_manifest.h',
        'common/extensions/url_pattern.cc',
        'common/extensions/url_pattern.h',
        'common/extensions/user_script.cc',
        'common/extensions/user_script.h',
        'common/gfx/utils.h',
        'common/net/dns.h',
        'common/net/net_resource_provider.cc',
        'common/net/net_resource_provider.h',
        'common/net/socket_stream.h',
        'common/net/url_request_intercept_job.cc',
        'common/net/url_request_intercept_job.h',
        'common/web_resource/web_resource_unpacker.cc',
        'common/web_resource/web_resource_unpacker.h',
        'common/appcache/appcache_backend_proxy.cc',
        'common/appcache/appcache_backend_proxy.h',
        'common/appcache/appcache_dispatcher.cc',
        'common/appcache/appcache_dispatcher.h',
        'common/appcache/appcache_dispatcher_host.cc',
        'common/appcache/appcache_dispatcher_host.h',
        'common/appcache/appcache_frontend_proxy.cc',
        'common/appcache/appcache_frontend_proxy.h',
        'common/appcache/chrome_appcache_service.cc',
        'common/appcache/chrome_appcache_service.h',
        'common/automation_constants.cc',
        'common/automation_constants.h',
        'common/bindings_policy.h',
        'common/child_process.cc',
        'common/child_process.h',
        'common/child_process_host.cc',
        'common/child_process_host.h',
        'common/child_process_info.cc',
        'common/child_process_info.h',
        'common/child_process_logging.h',
        'common/child_process_logging_linux.cc',
        'common/child_process_logging_mac.mm',
        'common/child_process_logging_win.cc',
        'common/child_thread.cc',
        'common/child_thread.h',
        'common/chrome_counters.cc',
        'common/chrome_counters.h',
        'common/chrome_descriptors.h',
        'common/chrome_plugin_api.h',
        'common/chrome_plugin_lib.cc',
        'common/chrome_plugin_lib.h',
        'common/chrome_plugin_util.cc',
        'common/chrome_plugin_util.h',
        'common/common_glue.cc',
        'common/common_param_traits.cc',
        'common/common_param_traits.h',
        'common/css_colors.h',
        'common/db_message_filter.cc',
        'common/db_message_filter.h',
        'common/debug_flags.cc',
        'common/debug_flags.h',
        'common/devtools_messages.h',
        'common/devtools_messages_internal.h',
        'common/dom_storage_type.h',
        'common/filter_policy.h',
        'common/gears_api.h',
        'common/gtk_tree.cc',
        'common/gtk_tree.h',
        'common/gtk_util.cc',
        'common/gtk_util.h',
        'common/histogram_synchronizer.cc',
        'common/histogram_synchronizer.h',
        'common/important_file_writer.cc',
        'common/important_file_writer.h',
        'common/jstemplate_builder.cc',
        'common/jstemplate_builder.h',
        'common/libxml_utils.cc',
        'common/libxml_utils.h',
        'common/logging_chrome.cc',
        'common/logging_chrome.h',
        'common/main_function_params.h',
        'common/message_router.cc',
        'common/message_router.h',
        'common/mru_cache.h',
        'common/navigation_gesture.h',
        'common/navigation_types.h',
        'common/native_web_keyboard_event.h',
        'common/native_web_keyboard_event_linux.cc',
        'common/native_web_keyboard_event_mac.mm',
        'common/native_web_keyboard_event_win.cc',
        'common/native_window_notification_source.h',
        'common/notification_details.h',
        'common/notification_observer.h',
        'common/notification_registrar.cc',
        'common/notification_registrar.h',
        'common/notification_service.cc',
        'common/notification_service.h',
        'common/notification_source.h',
        'common/notification_type.h',
        'common/owned_widget_gtk.cc',
        'common/owned_widget_gtk.h',
        'common/page_transition_types.h',
        'common/page_zoom.h',
        'common/platform_util.h',
        'common/platform_util_linux.cc',
        'common/platform_util_mac.mm',
        'common/platform_util_win.cc',
        'common/plugin_carbon_interpose_constants_mac.h',
        'common/plugin_carbon_interpose_constants_mac.cc',
        'common/plugin_messages.h',
        'common/plugin_messages_internal.h',
        'common/pref_member.cc',
        'common/pref_member.h',
        'common/pref_service.cc',
        'common/pref_service.h',
        'common/process_watcher_posix.cc',
        'common/process_watcher_win.cc',
        'common/process_watcher.h',
        'common/property_bag.cc',
        'common/property_bag.h',
        'common/ref_counted_util.h',
        'common/render_messages.h',
        'common/render_messages_internal.h',
        'common/renderer_preferences.h',
        'common/resource_dispatcher.cc',
        'common/resource_dispatcher.h',
        'common/result_codes.h',
        'common/sandbox_init_wrapper.h',
        'common/sandbox_init_wrapper_linux.cc',
        'common/sandbox_init_wrapper_mac.cc',
        'common/sandbox_init_wrapper_win.cc',
        'common/sandbox_mac.h',
        'common/sandbox_mac.mm',
        'common/spellcheck_common.cc',
        'common/spellcheck_common.h',
        'common/security_filter_peer.cc',
        'common/security_filter_peer.h',
        'common/nacl_messages.h',
        'common/nacl_messages_internal.h',
        'common/sqlite_compiled_statement.cc',
        'common/sqlite_compiled_statement.h',
        'common/sqlite_utils.cc',
        'common/sqlite_utils.h',
        'common/task_queue.cc',
        'common/task_queue.h',
        'common/temp_scaffolding_stubs.cc',
        'common/temp_scaffolding_stubs.h',
        'common/thumbnail_score.cc',
        'common/thumbnail_score.h',
        'common/time_format.cc',
        'common/time_format.h',
        'common/transport_dib.h',
        'common/transport_dib_linux.cc',
        'common/transport_dib_mac.cc',
        'common/transport_dib_win.cc',
        'common/url_constants.cc',
        'common/url_constants.h',
        'common/view_types.cc',
        'common/view_types.h',
        'common/visitedlink_common.cc',
        'common/visitedlink_common.h',
        'common/webkit_param_traits.h',
        'common/webmessageportchannel_impl.cc',
        'common/webmessageportchannel_impl.h',
        'common/win_safe_util.cc',
        'common/win_safe_util.h',
        'common/worker_messages.h',
        'common/worker_messages_internal.h',
        'common/worker_thread_ticker.cc',
        'common/worker_thread_ticker.h',
        'common/x11_util.cc',
        'common/x11_util.h',
        'common/x11_util_internal.h',
        'common/zip.cc',  # Requires zlib directly.
        'common/zip.h',
        'third_party/xdg_user_dirs/xdg_user_dir_lookup.cc',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '..',
        ],
      },
      'export_dependent_settings': [
        '../app/app.gyp:app_base',
      ],
      'conditions': [
        ['OS=="linux"', {
          'dependencies': [
            '../build/linux/system.gyp:gtk',
          ],
          'export_dependent_settings': [
            '../third_party/sqlite/sqlite.gyp:sqlite',
          ],
          'link_settings': {
            'libraries': [
              '-lX11',
              '-lXrender',
              '-lXext',
            ],
          },
        }, { # else: 'OS!="linux"'
          'sources!': [
            'third_party/xdg_user_dirs/xdg_user_dir_lookup.cc',
          ],
        }],
        ['OS=="linux" and selinux==1', {
          'dependencies': [
            '../build/linux/system.gyp:selinux',
          ],
        }],
        ['OS=="win"', {
          'include_dirs': [
            'third_party/wtl/include',
          ],
        }, { # else: OS != "win"
          'sources!': [
            'common/hi_res_timer_manager.cc',
            'common/hi_res_timer_manager.h',
            'common/temp_scaffolding_stubs.h',
          ],
        }],
        ['OS=="win" or OS=="linux"', {
          'sources!': [
            'common/temp_scaffolding_stubs.cc',
          ],
        }],
      ],
    },
    {
      'target_name': 'debugger',
      'type': '<(library)',
      'msvs_guid': '57823D8C-A317-4713-9125-2C91FDFD12D6',
      'dependencies': [
        'chrome_resources',
        'chrome_strings',
        'theme_resources',
        '../skia/skia.gyp:skia',
        '../third_party/icu/icu.gyp:icui18n',
        '../third_party/icu/icu.gyp:icuuc',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'browser/debugger/debugger_remote_service.cc',
        'browser/debugger/debugger_remote_service.h',
        'browser/debugger/debugger_wrapper.cc',
        'browser/debugger/debugger_wrapper.h',
        'browser/debugger/devtools_client_host.h',
        'browser/debugger/devtools_manager.cc',
        'browser/debugger/devtools_manager.h',
        'browser/debugger/devtools_protocol_handler.cc',
        'browser/debugger/devtools_protocol_handler.h',
        'browser/debugger/devtools_remote.h',
        'browser/debugger/devtools_remote_listen_socket.cc',
        'browser/debugger/devtools_remote_listen_socket.h',
        'browser/debugger/devtools_remote_message.cc',
        'browser/debugger/devtools_remote_message.h',
        'browser/debugger/devtools_remote_service.cc',
        'browser/debugger/devtools_remote_service.h',
        'browser/debugger/devtools_window.cc',
        'browser/debugger/devtools_window.h',
        'browser/debugger/extension_ports_remote_service.cc',
        'browser/debugger/extension_ports_remote_service.h',
        'browser/debugger/inspectable_tab_proxy.cc',
        'browser/debugger/inspectable_tab_proxy.h',
      ],
      'conditions': [
        ['OS=="linux" or OS=="freebsd"', {
          'dependencies': [
            '../build/linux/system.gyp:gtk',
          ],
        }],
      ],
    },
    {
      'target_name': 'plugin',
      'type': '<(library)',
      'msvs_guid': '20A560A0-2CD0-4D9E-A58B-1F24B99C087A',
      'dependencies': [
        'common',
        'chrome_resources',
        'chrome_strings',
        '../media/media.gyp:media',
        '../skia/skia.gyp:skia',
        '../third_party/icu/icu.gyp:icui18n',
        '../third_party/icu/icu.gyp:icuuc',
        '../third_party/libxml/libxml.gyp:libxml',
        '../third_party/npapi/npapi.gyp:npapi',
        '../third_party/hunspell/hunspell.gyp:hunspell',
        '../webkit/webkit.gyp:glue',
      ],
      'include_dirs': [
        '<(INTERMEDIATE_DIR)',
      ],
      'sources': [
        # All .cc, .h, .m, and .mm files under plugins except for tests and
        # mocks.
        'plugin/chrome_plugin_host.cc',
        'plugin/chrome_plugin_host.h',
        'plugin/npobject_proxy.cc',
        'plugin/npobject_proxy.h',
        'plugin/npobject_stub.cc',
        'plugin/npobject_stub.h',
        'plugin/npobject_util.cc',
        'plugin/npobject_util.h',
        'plugin/plugin_channel.cc',
        'plugin/plugin_channel.h',
        'plugin/plugin_channel_base.cc',
        'plugin/plugin_channel_base.h',
        'plugin/plugin_main.cc',
        'plugin/plugin_thread.cc',
        'plugin/plugin_thread.h',
        'plugin/webplugin_delegate_stub.cc',
        'plugin/webplugin_delegate_stub.h',
        'plugin/webplugin_proxy.cc',
        'plugin/webplugin_proxy.h',
      ],
      # These are layered in conditionals in the event other platforms
      # end up using this module as well.
      'conditions': [
        ['OS=="win"', {
          'defines': [
            '__STD_C',
            '_CRT_SECURE_NO_DEPRECATE',
            '_SCL_SECURE_NO_DEPRECATE',
          ],
          'include_dirs': [
            'third_party/wtl/include',
          ],
        },],
      ],
    },
    {
      'target_name': 'nacl',
      'type': '<(library)',
      'msvs_guid': '83E86DAF-5763-4711-AD34-5FDAE395560C',
      'dependencies': [
        'common',
        'chrome_resources',
        'chrome_strings',
        '../third_party/npapi/npapi.gyp:npapi',
        '../webkit/webkit.gyp:glue',
        '../native_client/src/trusted/plugin/plugin.gyp:npGoogleNaClPluginChrome',
        '../native_client/src/trusted/service_runtime/service_runtime.gyp:sel',
        '../native_client/src/trusted/validator_x86/validator_x86.gyp:ncvalidate',
        '../native_client/src/trusted/platform_qualify/platform_qualify.gyp:platform_qual_lib',
      ],
      'include_dirs': [
        '<(INTERMEDIATE_DIR)',
      ],
      'defines': [
        'NACL_BLOCK_SHIFT=5',
        'NACL_BLOCK_SIZE=32',
        '<@(nacl_defines)',
      ],
      'sources': [
        # All .cc, .h, .m, and .mm files under nacl except for tests and
        # mocks.
        'nacl/sel_main.cc',
        'nacl/nacl_main.cc',
        'nacl/nacl_thread.cc',
        'nacl/nacl_thread.h',
      ],
      # TODO(gregoryd): consider switching NaCl to use Chrome OS defines
      'conditions': [
        ['OS=="win"', {
          'defines': [
            '__STD_C',
            '_CRT_SECURE_NO_DEPRECATE',
            '_SCL_SECURE_NO_DEPRECATE',
          ],
          'include_dirs': [
            'third_party/wtl/include',
          ],
        },],
      ],
    },
    {
      'target_name': 'renderer',
      'type': '<(library)',
      'msvs_guid': '9301A569-5D2B-4D11-9332-B1E30AEACB8D',
      'dependencies': [
        'common',
        'plugin',
        'chrome_resources',
        'chrome_strings',
        '../printing/printing.gyp:printing',
        '../skia/skia.gyp:skia',
        '../third_party/hunspell/hunspell.gyp:hunspell',
        '../third_party/icu/icu.gyp:icui18n',
        '../third_party/icu/icu.gyp:icuuc',
        '../third_party/npapi/npapi.gyp:npapi',
        # TODO(yaar): Should only stop depending on webcore and depend
        # only on webkit.
        '../third_party/WebKit/WebCore/WebCore.gyp/WebCore.gyp:webcore',
        '../third_party/WebKit/WebKit/chromium/WebKit.gyp:webkit',
        '../webkit/webkit.gyp:glue',
      ],
      'include_dirs': [
        '..',
      ],
      'defines': [
        '<@(nacl_defines)',
      ],
      'sources': [
        # TODO(jrg): to link ipc_tests, these files need to be in renderer.a.
        # But app/ is the wrong directory for them.
        # Better is to remove the dep of *_tests on renderer, but in the
        # short term I'd like the build to work.
        'renderer/automation/dom_automation_controller.cc',
        'renderer/automation/dom_automation_controller.h',
        'renderer/extensions/bindings_utils.cc',
        'renderer/extensions/bindings_utils.h',
        'renderer/extensions/event_bindings.cc',
        'renderer/extensions/event_bindings.h',
        'renderer/extensions/extension_process_bindings.cc',
        'renderer/extensions/extension_process_bindings.h',
        'renderer/extensions/js_only_v8_extensions.cc',
        'renderer/extensions/js_only_v8_extensions.h',
        'renderer/extensions/renderer_extension_bindings.cc',
        'renderer/extensions/renderer_extension_bindings.h',
        'renderer/loadtimes_extension_bindings.h',
        'renderer/loadtimes_extension_bindings.cc',
        'renderer/media/audio_renderer_impl.cc',
        'renderer/media/audio_renderer_impl.h',
        'renderer/net/render_dns_master.cc',
        'renderer/net/render_dns_master.h',
        'renderer/net/render_dns_queue.cc',
        'renderer/net/render_dns_queue.h',
        'renderer/resources/event_bindings.js',
        'renderer/resources/extension_apitest.js',
        'renderer/resources/extension_process_bindings.js',
        'renderer/resources/greasemonkey_api.js',
        'renderer/resources/json_schema.js',
        'renderer/resources/renderer_extension_bindings.js',
        'renderer/about_handler.cc',
        'renderer/about_handler.h',
        'renderer/audio_message_filter.cc',
        'renderer/audio_message_filter.h',
        'renderer/devtools_agent.cc',
        'renderer/devtools_agent.h',
        'renderer/devtools_agent_filter.cc',
        'renderer/devtools_agent_filter.h',
        'renderer/devtools_client.cc',
        'renderer/devtools_client.h',
        'renderer/dom_ui_bindings.cc',
        'renderer/dom_ui_bindings.h',
        'renderer/extension_groups.h',
        'renderer/external_host_bindings.cc',
        'renderer/external_host_bindings.h',
        'renderer/external_extension.cc',
        'renderer/external_extension.h',
        'renderer/localized_error.cc',
        'renderer/localized_error.h',
        'renderer/navigation_state.h',
        'renderer/notification_provider.cc',
        'renderer/notification_provider.h',
        'renderer/paint_aggregator.cc',
        'renderer/plugin_channel_host.cc',
        'renderer/plugin_channel_host.h',
        'renderer/print_web_view_helper.cc',
        'renderer/print_web_view_helper.h',
        'renderer/print_web_view_helper_linux.cc',
        'renderer/print_web_view_helper_mac.mm',
        'renderer/print_web_view_helper_win.cc',
        'renderer/render_process.cc',
        'renderer/render_process.h',
        'renderer/render_thread.cc',
        'renderer/render_thread.h',
        'renderer/render_view.cc',
        'renderer/render_view_linux.cc',
        'renderer/render_view.h',
        'renderer/render_widget.cc',
        'renderer/render_widget.h',
        'renderer/renderer_glue.cc',
        'renderer/renderer_histogram_snapshots.cc',
        'renderer/renderer_histogram_snapshots.h',
        'renderer/renderer_main.cc',
        'renderer/renderer_main_platform_delegate.h',
        'renderer/renderer_main_platform_delegate_linux.cc',
        'renderer/renderer_main_platform_delegate_mac.mm',
        'renderer/renderer_main_platform_delegate_win.cc',
        'renderer/renderer_sandbox_support_linux.cc',
        'renderer/renderer_sandbox_support_linux.h',
        'renderer/renderer_webkitclient_impl.cc',
        'renderer/renderer_webkitclient_impl.h',
        'renderer/renderer_webstoragearea_impl.cc',
        'renderer/renderer_webstoragearea_impl.h',
        'renderer/renderer_webstoragenamespace_impl.cc',
        'renderer/renderer_webstoragenamespace_impl.h',
        'renderer/renderer_web_database_observer.cc',
        'renderer/renderer_web_database_observer.h',
        'renderer/socket_stream_dispatcher.cc',
        'renderer/socket_stream_dispatcher.h',
        'renderer/spellchecker/spellcheck.cc',
        'renderer/spellchecker/spellcheck.h',
        'renderer/spellchecker/spellcheck_worditerator.cc',
        'renderer/spellchecker/spellcheck_worditerator.h',
        'renderer/user_script_idle_scheduler.cc',
        'renderer/user_script_idle_scheduler.h',
        'renderer/user_script_slave.cc',
        'renderer/user_script_slave.h',
        'renderer/visitedlink_slave.cc',
        'renderer/visitedlink_slave.h',
        'renderer/webplugin_delegate_proxy.cc',
        'renderer/webplugin_delegate_proxy.h',
        'renderer/webplugin_delegate_pepper.cc',
        'renderer/webplugin_delegate_pepper.h',
        'renderer/websharedworker_proxy.cc',
        'renderer/websharedworker_proxy.h',
        'renderer/websharedworkerrepository_impl.cc',
        'renderer/websharedworkerrepository_impl.h',
        'renderer/webworker_base.cc',
        'renderer/webworker_base.h',
        'renderer/webworker_proxy.cc',
        'renderer/webworker_proxy.h',
      ],
      'link_settings': {
        'mac_bundle_resources': [
          'renderer/renderer.sb',
        ],
      },
      'conditions': [
        ['enable_pepper==1', {
          'dependencies': [
            '../gpu/gpu.gyp:gpu_plugin',
          ],
        }],
        ['disable_nacl!=1', {
          'dependencies': [
            'nacl',
          ],
        }],
        # Linux-specific rules.
        ['OS=="linux"', {
          'dependencies': [
            '../build/linux/system.gyp:gtk',
            '../sandbox/sandbox.gyp:sandbox',
          ],
        }],
        # Windows-specific rules.
        ['OS=="win"', {
          'include_dirs': [
            'third_party/wtl/include',
          ],
          'dependencies': [
            '<(allocator_target)',
          ],
          'export_dependent_settings': [
            '<(allocator_target)',
          ],
        },],
      ],
    },
    {
      'target_name': 'utility',
      'type': '<(library)',
      'msvs_guid': '4D2B38E6-65FF-4F97-B88A-E441DF54EBF7',
      'dependencies': [
        '../base/base.gyp:base',
        '../skia/skia.gyp:skia',
      ],
      'sources': [
        'utility/utility_main.cc',
        'utility/utility_thread.cc',
        'utility/utility_thread.h',
      ],
      'include_dirs': [
        '..',
      ],
      'conditions': [
        ['OS=="linux"', {
          'dependencies': [
            '../build/linux/system.gyp:gtk',
          ],
        }],
      ],
    },
    {
      'target_name': 'profile_import',
      'type': '<(library)',
      'dependencies': [
        '../base/base.gyp:base',
      ],
      'sources': [
        'profile_import/profile_import_main.cc',
        'profile_import/profile_import_thread.cc',
        'profile_import/profile_import_thread.h',
      ],
    },
    {
      'target_name': 'worker',
      'type': '<(library)',
      'msvs_guid': 'C78D02D0-A366-4EC6-A248-AA8E64C4BA18',
      'dependencies': [
        '../base/base.gyp:base',
        '../third_party/WebKit/WebKit/chromium/WebKit.gyp:webkit',
      ],
      'sources': [
        'worker/nativewebworker_impl.cc',
        'worker/nativewebworker_impl.h',
        'worker/nativewebworker_stub.cc',
        'worker/nativewebworker_stub.h',
        'worker/websharedworker_stub.cc',
        'worker/websharedworker_stub.h',
        'worker/webworker_stub_base.cc',
        'worker/webworker_stub_base.h',
        'worker/webworker_stub.cc',
        'worker/webworker_stub.h',
        'worker/webworkerclient_proxy.cc',
        'worker/webworkerclient_proxy.h',
        'worker/worker_main.cc',
        'worker/worker_thread.cc',
        'worker/worker_thread.h',
        'worker/worker_webkitclient_impl.cc',
        'worker/worker_webkitclient_impl.h',
      ],
      'include_dirs': [
        '..',
      ],
    },
    {
      'target_name': 'chrome',
      'type': 'executable',
      'mac_bundle': 1,
      'msvs_guid': '7B219FAA-E360-43C8-B341-804A94EEFFAC',
      'sources': [
        # All .cc, .h, .m, and .mm files under app except for tests.
        'app/breakpad_win.cc',
        'app/breakpad_win.h',
        'app/chrome_exe_main.cc',
        'app/chrome_exe_main.mm',
        'app/chrome_exe_main_gtk.cc',
        'app/chrome_exe_resource.h',
        'app/client_util.cc',
        'app/client_util.h',
        'app/hard_error_handler_win.cc',
        'app/hard_error_handler_win.h',
        'app/scoped_ole_initializer.h',
      ],
      'mac_bundle_resources': [
        'app/app-Info.plist',
      ],
      # TODO(mark): Come up with a fancier way to do this.  It should only
      # be necessary to list app-Info.plist once, not the three times it is
      # listed here.
      'mac_bundle_resources!': [
        'app/app-Info.plist',
      ],
      'xcode_settings': {
        'INFOPLIST_FILE': 'app/app-Info.plist',
      },
      'conditions': [
        ['chrome_frame_define==1 and OS=="win"', {
          'dependencies': [
            '../chrome_frame/chrome_frame.gyp:npchrome_tab',
          ],
        }],
        ['OS=="linux" or OS=="freebsd"', {
          'actions': [
            {
              'action_name': 'manpage',
              'conditions': [
                [ 'branding == "Chrome"', {
                  'variables': {
                    'name': 'Google Chrome',
                    'filename': 'google-chrome',
                    'confdir': 'google-chrome',
                  },
                }, { # else branding!="Chrome"
                  'variables': {
                    'name': 'Chromium',
                    'filename': 'chromium-browser',
                    'confdir': 'chromium',
                  },
                }],
              ],
              'inputs': [
                'tools/build/linux/sed.sh',
                'app/resources/manpage.1.in',
              ],
              'outputs': [
                '<(PRODUCT_DIR)/chrome.1',
              ],
              'action': [
                'tools/build/linux/sed.sh',
                'app/resources/manpage.1.in',
                '<@(_outputs)',
                '-e', 's/@@NAME@@/<(name)/',
                '-e', 's/@@FILENAME@@/<(filename)/',
                '-e', 's/@@CONFDIR@@/<(confdir)/',
              ],
              'message': 'Generating manpage'
            },
          ],
          'conditions': [
            [ 'linux_use_tcmalloc==1', {
                'dependencies': [
                  '<(allocator_target)',
                ],
              },
            ],
          ],
          'dependencies': [
            # On Linux, link the dependencies (libraries) that make up actual
            # Chromium functionality directly into the executable.
            '<@(chromium_dependencies)',
            # Needed for chrome_dll_main.cc #include of gtk/gtk.h
            '../build/linux/system.gyp:gtk',
          ],
          'sources': [
            'app/chrome_dll_main.cc',
            'app/chrome_dll_resource.h',
          ],
          'copies': [
            {
              'destination': '<(PRODUCT_DIR)',
              'files': ['<(INTERMEDIATE_DIR)/repack/chrome.pak',
                        'tools/build/linux/chrome-wrapper',
                        '../third_party/xdg-utils/scripts/xdg-settings',
                        ],
              # The wrapper script above may need to generate a .desktop file,
              # which requires an icon. So, copy one next to the script.
              'conditions': [
                ['branding=="Chrome"', {
                  'files': ['app/theme/google_chrome/product_logo_48.png']
                }, { # else: 'branding!="Chrome"
                  'files': ['app/theme/chromium/product_logo_48.png']
                }],
              ],
            },
            {
              'destination': '<(PRODUCT_DIR)/locales',
              'files': [
                '>!@(<(repack_locales_cmd) -o -g \'<(grit_out_dir)\' -s \'<(SHARED_INTERMEDIATE_DIR)\' -x \'<(INTERMEDIATE_DIR)\' <(locales))',
              ],
            },
          ],
        }],
        ['OS=="linux" and (toolkit_views==1 or chromeos==1)', {
          'dependencies': [
            '../views/views.gyp:views',
          ],
        }],
        ['OS=="mac"', {
          'variables': {
            'mac_packaging_dir':
                '<(PRODUCT_DIR)/<(mac_product_name) Packaging',
            # <(PRODUCT_DIR) expands to $(BUILT_PRODUCTS_DIR), which doesn't
            # work properly in a shell script, where ${BUILT_PRODUCTS_DIR} is
            # needed.
            'mac_packaging_sh_dir':
                '${BUILT_PRODUCTS_DIR}/<(mac_product_name) Packaging',
          },
          # 'branding' is a variable defined in common.gypi
          # (e.g. "Chromium", "Chrome")
          'conditions': [
            ['branding=="Chrome"', {
              'mac_bundle_resources': [
                'app/theme/google_chrome/app.icns',
                'app/theme/google_chrome/document.icns',
              ],
            }, {  # else: 'branding!="Chrome"
              'mac_bundle_resources': [
                'app/theme/chromium/app.icns',
                'app/theme/chromium/document.icns',
              ],
            }],
            ['mac_breakpad==1', {
              'variables': {
                # A real .dSYM is needed for dump_syms to operate on.
                'mac_real_dsym': 1,
              },
              'dependencies': [
                '../breakpad/breakpad.gyp:dump_syms',
                '../breakpad/breakpad.gyp:symupload',
              ],
              # The "Dump Symbols" post-build step is in a target_conditions
              # block so that it will follow the "Strip If Needed" step if that
              # is also being used.  There is no standard configuration where
              # both of these steps occur together, but Mark likes to use this
              # configuraiton sometimes when testing Breakpad-enabled builds
              # without the time overhead of creating real .dSYM files.  When
              # both "Dump Symbols" and "Strip If Needed" are present, "Dump
              # Symbols" must come second because "Strip If Needed" creates
              # a fake .dSYM that dump_syms needs to fake dump.  Since
              # "Strip If Needed" is added in a target_conditions block in
              # common.gypi, "Dump Symbols" needs to be in an (always true)
              # target_conditions block.
              'target_conditions': [
                ['1 == 1', {
                  'postbuilds': [
                    {
                      'postbuild_name': 'Dump Symbols',
                      'variables': {
                        'dump_product_syms_path':
                            'tools/build/mac/dump_product_syms',
                      },
                      'action': ['<(dump_product_syms_path)',
                                 '<(branding)'],
                    },
                  ],
                }],
              ],
            }],  # mac_breakpad
            ['mac_keystone==1', {
              'copies': [
                {
                  # Put keystone_install.sh where the packaging system will
                  # find it.  The packager will copy this script to the
                  # correct location on the disk image.
                  'destination': '<(mac_packaging_dir)',
                  'files': [
                    'tools/build/mac/keystone_install.sh',
                  ],
                },
              ],
            }],  # mac_keystone
            ['buildtype=="Official"', {
              'actions': [
                {
                  # Create sign.sh, the script that the packaging system will
                  # use to sign the .app bundle.
                  'action_name': 'Make sign.sh',
                  'variables': {
                    'make_sign_sh_path': 'tools/build/mac/make_sign_sh',
                    'sign_sh_in_path': 'tools/build/mac/sign.sh.in',
                    'app_resource_rules_in_path':
                        'tools/build/mac/app_resource_rules.plist.in',
                  },
                  'inputs': [
                    '<(make_sign_sh_path)',
                    '<(sign_sh_in_path)',
                    '<(app_resource_rules_in_path)',
                    '<(version_path)',
                  ],
                  'outputs': [
                    '<(mac_packaging_dir)/sign.sh',
                    '<(mac_packaging_dir)/app_resource_rules.plist',
                  ],
                  'action': [
                    '<(make_sign_sh_path)',
                    '<(mac_packaging_sh_dir)',
                    '<(mac_product_name)',
                    '<(version_full)',
                  ],
                },
              ],
            }],  # buildtype=="Official"
          ],
          'product_name': '<(mac_product_name)',
          'xcode_settings': {
            # chrome/app/app-Info.plist has:
            #   CFBundleIdentifier of CHROMIUM_BUNDLE_ID
            #   CFBundleName of CHROMIUM_SHORT_NAME
            #   CFBundleSignature of CHROMIUM_CREATOR
            # Xcode then replaces these values with the branded values we set
            # as settings on the target.
            'CHROMIUM_BUNDLE_ID': '<(mac_bundle_id)',
            'CHROMIUM_CREATOR': '<(mac_creator)',
            'CHROMIUM_SHORT_NAME': '<(branding)',
          },
          'dependencies': [
            'helper_app',
            'infoplist_strings_tool',
            # This library provides the real implementation for NaClSyscallSeg
            '../native_client/src/trusted/service_runtime/arch/x86_32/service_runtime_x86_32.gyp:service_runtime_x86_32_chrome'
          ],
          'actions': [
            {
              # Generate the InfoPlist.strings file
              'action_name': 'Generating InfoPlist.strings files',
              'variables': {
                'tool_path': '<(PRODUCT_DIR)/infoplist_strings_tool',
                # Unique dir to write to so the [lang].lproj/InfoPlist.strings
                # for the main app and the helper app don't name collide.
                'output_path': '<(INTERMEDIATE_DIR)/app_infoplist_strings',
              },
              'conditions': [
                [ 'branding == "Chrome"', {
                  'variables': {
                     'branding_name': 'google_chrome_strings',
                  },
                }, { # else branding!="Chrome"
                  'variables': {
                     'branding_name': 'chromium_strings',
                  },
                }],
              ],
              'inputs': [
                '<(tool_path)',
                '<(version_path)',
                # TODO: remove this helper when we have loops in GYP
                '>!@(<(apply_locales_cmd) \'<(grit_out_dir)/<(branding_name)_ZZLOCALE.pak\' <(locales))',
              ],
              'outputs': [
                # TODO: remove this helper when we have loops in GYP
                '>!@(<(apply_locales_cmd) -d \'<(output_path)/ZZLOCALE.lproj/InfoPlist.strings\' <(locales))',
              ],
              'action': [
                '<(tool_path)',
                '-b', '<(branding_name)',
                '-v', '<(version_path)',
                '-g', '<(grit_out_dir)',
                '-o', '<(output_path)',
                '-t', 'main',
                '<@(locales)',
              ],
              'message': 'Generating the language InfoPlist.strings files',
              'process_outputs_as_mac_bundle_resources': 1,
            },
          ],
          'copies': [
            {
              'destination': '<(PRODUCT_DIR)/<(mac_product_name).app/Contents/Versions/<(version_full)',
              'files': [
                '<(PRODUCT_DIR)/<(mac_product_name) Helper.app',
              ],
            },
          ],
          'postbuilds': [
            {
              'postbuild_name': 'Copy <(mac_product_name) Framework.framework',
              'action': [
                'tools/build/mac/copy_framework_unversioned',
                '${BUILT_PRODUCTS_DIR}/<(mac_product_name) Framework.framework',
                '${BUILT_PRODUCTS_DIR}/${CONTENTS_FOLDER_PATH}/Versions/<(version_full)',
              ],
            },
            {
              # Modify the Info.plist as needed.  The script explains why this
              # is needed.  This is also done in the helper_app and chrome_dll
              # targets.  Use -b0 and -k0 to not include any Breakpad or
              # Keystone information; that all goes into the framework's
              # Info.plist.  Use -s1 to include Subversion information.
              'postbuild_name': 'Tweak Info.plist',
              'action': ['<(tweak_info_plist_path)',
                         '-b0',
                         '-k0',
                         '-s1',
                         '<(branding)',
                         '<(mac_bundle_id)'],
            },
            {
              'postbuild_name': 'Clean up old versions',
              'action': [
                'tools/build/mac/clean_up_old_versions',
                '<(version_full)'
              ],
            },
          ],  # postbuilds
        }, { # else: OS != "mac"
          'conditions': [
            ['branding=="Chrome"', {
              'product_name': 'chrome'
            }, {  # else: Branding!="Chrome"
              # TODO:  change to:
              #   'product_name': 'chromium'
              # whenever we convert the rest of the infrastructure
              # (buildbots etc.) to use "gyp -Dbranding=Chrome".
              # NOTE: chrome/app/theme/chromium/BRANDING and
              # chrome/app/theme/google_chrome/BRANDING have the short names,
              # etc.; should we try to extract from there instead?
              'product_name': 'chrome'
            }],
          ],
        }],
        ['OS=="linux"', {
          'conditions': [
            ['branding=="Chrome"', {
              'dependencies': [
                'installer/installer.gyp:linux_installer_configs',
              ],
            }],
            ['selinux==0', {
              'dependencies': [
                '../sandbox/sandbox.gyp:sandbox',
              ],
            }],
            ['linux_sandbox_path != ""', {
              'defines': [
                'LINUX_SANDBOX_PATH="<(linux_sandbox_path)"',
              ],
            }],
          ],
        }],
        ['OS=="mac" or OS=="win"', {
          'dependencies': [
            # On Windows and Mac, make sure we've built chrome_dll, which
            # contains all of the library code with Chromium functionality.
            'chrome_dll',
          ],
        }],
        ['OS=="win"', {
          'dependencies': [
            'installer/installer.gyp:installer_util',
            'installer/installer.gyp:installer_util_strings',
            '../breakpad/breakpad.gyp:breakpad_handler',
            '../breakpad/breakpad.gyp:breakpad_sender',
            '../sandbox/sandbox.gyp:sandbox',
            'app/locales/locales.gyp:*',
          ],
          'sources': [
            'app/chrome_exe.rc',
            'app/chrome_exe_version.rc.version',
          ],
          'include_dirs': [
            '<(SHARED_INTERMEDIATE_DIR)/chrome',
          ],
          'msvs_settings': {
            'VCLinkerTool': {
              'DelayLoadDLLs': [
                'dbghelp.dll',
                'dwmapi.dll',
                'uxtheme.dll',
                'ole32.dll',
                'oleaut32.dll',
              ],
              'ImportLibrary': '$(OutDir)\\lib\\chrome_exe.lib',
              'ProgramDatabaseFile': '$(OutDir)\\chrome_exe.pdb',
              # Set /SUBSYSTEM:WINDOWS for chrome.exe itself.
              'SubSystem': '2',
            },
            'VCManifestTool': {
              'AdditionalManifestFiles': '$(ProjectDir)\\app\\chrome.exe.manifest',
            },
          },
          'actions': [
            {
              'action_name': 'version',
              'variables': {
                'template_input_path': 'app/chrome_exe_version.rc.version',
              },
              'conditions': [
                [ 'branding == "Chrome"', {
                  'variables': {
                     'branding_path': 'app/theme/google_chrome/BRANDING',
                  },
                }, { # else branding!="Chrome"
                  'variables': {
                     'branding_path': 'app/theme/chromium/BRANDING',
                  },
                }],
              ],
              'inputs': [
                '<(template_input_path)',
                '<(version_path)',
                '<(branding_path)',
              ],
              'outputs': [
                '<(SHARED_INTERMEDIATE_DIR)/chrome/chrome_exe_version.rc',
              ],
              'action': [
                'python',
                '<(version_py_path)',
                '-f', '<(version_path)',
                '-f', '<(branding_path)',
                '<(template_input_path)',
                '<@(_outputs)',
              ],
              'process_outputs_as_sources': 1,
              'message': 'Generating version information in <(_outputs)'
            },
            {
              'action_name': 'first_run',
              'inputs': [
                  'app/FirstRun',
              ],
              'outputs': [
                  '<(PRODUCT_DIR)/First Run',
              ],
              'action': ['cp', '-f', '<@(_inputs)', '<@(_outputs)'],
              'message': 'Copy first run complete sentinel file',
            },
          ],
        }, {  # 'OS!="win"
          'sources!': [
            'app/chrome_exe_main.cc',
            'app/client_util.cc',
          ]
        }],
        ['OS=="linux" or OS=="freebsd"', {
          'variables': {
            'repack_path': '../tools/data_pack/repack.py',
          },
          'actions': [
            # TODO(mark): These actions are duplicated for the Mac in the
            # chrome_dll target.  Can they be unified?
            {
              'action_name': 'repack_chrome',
              'variables': {
                'pak_inputs': [
                  '<(grit_out_dir)/browser_resources.pak',
                  '<(grit_out_dir)/common_resources.pak',
                  '<(grit_out_dir)/renderer_resources.pak',
                  '<(grit_out_dir)/theme_resources.pak',
                  '<(SHARED_INTERMEDIATE_DIR)/app/app_resources/app_resources.pak',
                  '<(SHARED_INTERMEDIATE_DIR)/net/net_resources.pak',
                  '<(SHARED_INTERMEDIATE_DIR)/webkit/webkit_resources.pak',
                ],
              },
              'inputs': [
                '<(repack_path)',
                '<@(pak_inputs)',
              ],
              'outputs': [
                '<(INTERMEDIATE_DIR)/repack/chrome.pak',
              ],
              'action': ['python', '<(repack_path)', '<@(_outputs)',
                         '<@(pak_inputs)'],
            },
            {
              'action_name': 'repack_locales',
              'variables': {
                'conditions': [
                  ['branding=="Chrome"', {
                    'branding_flag': ['-b', 'google_chrome',],
                  }, {  # else: branding!="Chrome"
                    'branding_flag': ['-b', 'chromium',],
                  }],
                ],
              },
              'inputs': [
                'tools/build/repack_locales.py',
                # NOTE: Ideally the common command args would be shared amongst
                # inputs/outputs/action, but the args include shell variables
                # which need to be passed intact, and command expansion wants
                # to expand the shell variables. Adding the explicit quoting
                # here was the only way it seemed to work.
                '>!@(<(repack_locales_cmd) -i <(branding_flag) -g \'<(grit_out_dir)\' -s \'<(SHARED_INTERMEDIATE_DIR)\' -x \'<(INTERMEDIATE_DIR)\' <(locales))',
              ],
              'outputs': [
                '>!@(<(repack_locales_cmd) -o -g \'<(grit_out_dir)\' -s \'<(SHARED_INTERMEDIATE_DIR)\' -x \'<(INTERMEDIATE_DIR)\' <(locales))',
              ],
              'action': [
                '<@(repack_locales_cmd)',
                '<@(branding_flag)',
                '-g', '<(grit_out_dir)',
                '-s', '<(SHARED_INTERMEDIATE_DIR)',
                '-x', '<(INTERMEDIATE_DIR)',
                '<@(locales)',
              ],
            },
          ],
        }],
      ],
    },
    {
      # This target contains mocks and test utilities that don't belong in
      # production libraries but are used by more than one test executable.
      'target_name': 'test_support_common',
      'type': '<(library)',
      'dependencies': [
        'browser',
        'common',
        'renderer',
        'chrome_resources',
        'chrome_strings',
        'theme_resources',
        '../base/base.gyp:test_support_base',
        '../skia/skia.gyp:skia',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
      ],
      'export_dependent_settings': [
        'renderer',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        # The only thing used from browser is Browser::Type.
        'browser/browser.h',
        # TODO:  these should live here but are currently used by
        # production code code in libbrowser (above).
        #'browser/net/url_request_mock_http_job.cc',
        #'browser/net/url_request_mock_http_job.h',
        'browser/net/url_request_mock_net_error_job.cc',
        'browser/net/url_request_mock_net_error_job.h',
        'browser/renderer_host/mock_render_process_host.cc',
        'browser/renderer_host/mock_render_process_host.h',
        'browser/renderer_host/test/test_render_view_host.cc',
        'browser/renderer_host/test/test_render_view_host.h',
        'browser/tab_contents/test_tab_contents.cc',
        'browser/tab_contents/test_tab_contents.h',
        'common/ipc_test_sink.cc',
        'common/ipc_test_sink.h',
        'renderer/mock_keyboard.cc',
        'renderer/mock_keyboard.h',
        'renderer/mock_keyboard_driver_win.cc',
        'renderer/mock_keyboard_driver_win.h',
        'renderer/mock_printer.cc',
        'renderer/mock_printer.h',
        'renderer/mock_render_process.h',
        'renderer/mock_render_thread.cc',
        'renderer/mock_render_thread.h',
        'test/automation/autocomplete_edit_proxy.cc',
        'test/automation/autocomplete_edit_proxy.h',
        'test/automation/automation_constants.h',
        'test/automation/automation_handle_tracker.cc',
        'test/automation/automation_handle_tracker.h',
        'test/automation/automation_proxy.cc',
        'test/automation/automation_proxy.h',
        'test/automation/browser_proxy.cc',
        'test/automation/browser_proxy.h',
        'test/automation/tab_proxy.cc',
        'test/automation/tab_proxy.h',
        'test/automation/window_proxy.cc',
        'test/automation/window_proxy.h',
        'test/chrome_process_util.cc',
        'test/chrome_process_util.h',
        'test/chrome_process_util_linux.cc',
        'test/chrome_process_util_mac.cc',
        'test/chrome_process_util_win.cc',
        'test/test_browser_window.h',
        'test/testing_profile.cc',
        'test/testing_profile.h',
        'test/ui_test_utils.cc',
        'test/ui_test_utils.h',
        'test/ui_test_utils_linux.cc',
        'test/ui_test_utils_mac.cc',
        'test/ui_test_utils_win.cc',
      ],
      'conditions': [
        ['OS=="linux"', {
          'dependencies': [
            '../build/linux/system.gyp:gtk',
          ],
        }],
        ['OS=="win"', {
          'include_dirs': [
            'third_party/wtl/include',
          ],
        }],
      ],
    },
    {
      'target_name': 'test_support_ui',
      'type': '<(library)',
      'dependencies': [
        'test_support_common',
        'chrome_resources',
        'chrome_strings',
        'theme_resources',
        '../skia/skia.gyp:skia',
        '../testing/gtest.gyp:gtest',
      ],
      'export_dependent_settings': [
        'test_support_common',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'test/automated_ui_tests/automated_ui_test_base.cc',
        'test/automated_ui_tests/automated_ui_test_base.h',
        'test/testing_browser_process.h',
        'test/ui/javascript_test_util.cc',
        'test/ui/npapi_test_helper.cc',
        'test/ui/npapi_test_helper.h',
        'test/ui/run_all_unittests.cc',
        'test/ui/ui_layout_test.cc',
        'test/ui/ui_layout_test.h',
        'test/ui/ui_test.cc',
        'test/ui/ui_test.h',
        'test/ui/ui_test_suite.cc',
        'test/ui/ui_test_suite.h',
      ],
      'conditions': [
        ['OS=="linux"', {
          'dependencies': [
            '../build/linux/system.gyp:gtk',
          ],
        }],
        ['OS!="win"', {
          'sources!': [
            'test/ui/npapi_test_helper.cc',
          ],
        }],
      ],
    },
    {
      'target_name': 'test_support_unit',
      'type': '<(library)',
      'dependencies': [
        'test_support_common',
        'chrome_resources',
        'chrome_strings',
        '../skia/skia.gyp:skia',
        '../testing/gtest.gyp:gtest',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'test/unit/run_all_unittests.cc',
      ],
      'conditions': [
        ['OS=="linux"', {
          'dependencies': [
            # Needed for the following #include chain:
            #   test/unit/run_all_unittests.cc
            #   test/unit/chrome_test_suite.h
            #   gtk/gtk.h
            '../build/linux/system.gyp:gtk',
          ],
        }],
      ],
     },
    {
      'target_name': 'automated_ui_tests',
      'type': 'executable',
      'msvs_guid': 'D2250C20-3A94-4FB9-AF73-11BC5B73884B',
      'dependencies': [
        'browser',
        'renderer',
        'test_support_common',
        'test_support_ui',
        'theme_resources',
        '../base/base.gyp:base',
        '../skia/skia.gyp:skia',
        '../third_party/libxml/libxml.gyp:libxml',
        '../testing/gtest.gyp:gtest',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'test/automated_ui_tests/automated_ui_test_interactive_test.cc',
        'test/automated_ui_tests/automated_ui_tests.cc',
        'test/automated_ui_tests/automated_ui_tests.h',
      ],
      'conditions': [
        ['OS=="linux"', {
          'dependencies': [
            '../tools/xdisplaycheck/xdisplaycheck.gyp:xdisplaycheck',
          ],
        }],
        ['OS=="win"', {
          'include_dirs': [
            'third_party/wtl/include',
          ],
          'dependencies': [
            '<(allocator_target)',
          ],
        }],
      ],
    },
    {
      'target_name': 'ui_tests',
      'type': 'executable',
      'msvs_guid': '76235B67-1C27-4627-8A33-4B2E1EF93EDE',
      'dependencies': [
        'chrome',
        'browser',
        'debugger',
        'common',
        'chrome_resources',
        'chrome_strings',
        'syncapi',
        'test_support_ui',
        '../base/base.gyp:base',
        '../net/net.gyp:net',
        '../build/temp_gyp/googleurl.gyp:googleurl',
        '../skia/skia.gyp:skia',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
        '../third_party/icu/icu.gyp:icui18n',
        '../third_party/icu/icu.gyp:icuuc',
        '../third_party/libxml/libxml.gyp:libxml',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'app/chrome_main_uitest.cc',
        'browser/browser_encoding_uitest.cc',
        'browser/browser_uitest.cc',
        'browser/dom_ui/new_tab_ui_uitest.cc',
        'browser/download/download_uitest.cc',
        'browser/download/save_page_uitest.cc',
        'browser/errorpage_uitest.cc',
        'browser/extensions/extension_uitest.cc',
        'browser/history/redirect_uitest.cc',
        'browser/iframe_uitest.cc',
        'browser/images_uitest.cc',
        'browser/in_process_webkit/dom_storage_uitest.cc',
        'browser/locale_tests_uitest.cc',
        'browser/login_prompt_uitest.cc',
        'browser/media_uitest.cc',
        'browser/metrics/metrics_service_uitest.cc',
        'browser/notifications/notifications_uitest.cc',
        'browser/printing/printing_layout_uitest.cc',
        'browser/process_singleton_linux_uitest.cc',
        'browser/renderer_host/resource_dispatcher_host_uitest.cc',
        'browser/sanity_uitest.cc',
        'browser/session_history_uitest.cc',
        'browser/sessions/session_restore_uitest.cc',
        'browser/tab_contents/view_source_uitest.cc',
        'browser/tab_restore_uitest.cc',
        'browser/unload_uitest.cc',
        'browser/views/find_bar_host_uitest.cc',
        'common/logging_chrome_uitest.cc',
        'common/pref_service_uitest.cc',
        'test/automation/automation_proxy_uitest.cc',
        'test/automated_ui_tests/automated_ui_test_test.cc',
        'test/chrome_process_util_uitest.cc',
        'test/ui/dom_checker_uitest.cc',
        'test/ui/dromaeo_benchmark_uitest.cc',
        'test/ui/fast_shutdown_uitest.cc',
        'test/ui/history_uitest.cc',
        'test/ui/layout_plugin_uitest.cc',
        'test/ui/mouseleave_uitest.cc',
        'test/ui/npapi_uitest.cc',
        'test/ui/omnibox_uitest.cc',
        'test/ui/sandbox_uitests.cc',
        'test/ui/sunspider_uitest.cc',
        'test/ui/v8_benchmark_uitest.cc',
        'worker/worker_uitest.cc',
      ],
      'conditions': [
        ['OS=="linux"', {
          'dependencies': [
            '../build/linux/system.gyp:gtk',
            '../tools/xdisplaycheck/xdisplaycheck.gyp:xdisplaycheck',
          ],
          'sources!': [
            # TODO(port)
            'browser/login_prompt_uitest.cc',
            'test/ui/layout_plugin_uitest.cc',
          ],
        }, { # else: OS != "linux"
          'sources!': [
            'browser/process_singleton_linux_uitest.cc',
          ],
        }],
        ['OS=="linux" and toolkit_views==1', {
          'dependencies': [
            '../views/views.gyp:views',
          ],
        }],
        ['OS=="mac"', {
          'sources': [
            'app/breakpad_mac_stubs.mm',
          ],
          'sources!': [
            # TODO(port)
            'app/chrome_main_uitest.cc',
            'browser/login_prompt_uitest.cc',
            'browser/metrics/metrics_service_uitest.cc',
            # leaves an extra window on screen after test completes.
            'browser/sessions/session_restore_uitest.cc',
            # puts up modal dialogs.
            'browser/unload_uitest.cc',
            'test/ui/fast_shutdown_uitest.cc',
            'test/ui/layout_plugin_uitest.cc',
            'test/ui/omnibox_uitest.cc',
          ],
        }],
        ['OS=="win"', {
          'include_dirs': [
            'third_party/wtl/include',
          ],
          'dependencies': [
            'crash_service',  # run time dependency
            'security_tests',  # run time dependency
            'test_support_common',
            '../google_update/google_update.gyp:google_update',
            '../views/views.gyp:views',
            # run time dependency
            '../webkit/tools/test_shell/test_shell.gyp:npapi_test_plugin',
            '<(allocator_target)',
          ],
          'link_settings': {
            'libraries': [
              '-lOleAcc.lib',
            ],
          },
          'configurations': {
            'Debug': {
              'msvs_settings': {
                'VCLinkerTool': {
                  'LinkIncremental': '<(msvs_large_module_debug_link_mode)',
                },
              },
            },
          },
        }, { # else: OS != "win"
          'sources!': [
            # TODO(port)?  (Most of these include windows.h or similar.)
            'browser/extensions/extension_uitest.cc',
            'browser/media_uitest.cc',
            'browser/printing/printing_layout_uitest.cc',
            'browser/views/find_bar_host_uitest.cc',
            'common/logging_chrome_uitest.cc',
            'test/ui/npapi_uitest.cc',
            'test/ui/sandbox_uitests.cc',
          ],
        }],
      ],
    },
    {
      'target_name': 'nacl_ui_tests',
      'type': 'executable',
      'msvs_guid': '43E2004F-CD62-4595-A8A6-31E9BFA1EE5E',
      'dependencies': [
        'chrome',
        'browser',
        'debugger',
        'common',
        'chrome_resources',
        'chrome_strings',
        'syncapi',
        'test_support_ui',
        '../base/base.gyp:base',
        '../net/net.gyp:net',
        '../build/temp_gyp/googleurl.gyp:googleurl',
        '../skia/skia.gyp:skia',
        '../testing/gtest.gyp:gtest',
        '../third_party/icu/icu.gyp:icui18n',
        '../third_party/icu/icu.gyp:icuuc',
        '../third_party/libxml/libxml.gyp:libxml',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'test/nacl/nacl_test.cc',
      ],
      'conditions': [
        # TODO(gregoryd): This test will run on Windows only at first.
        # Refer to ui_tests target above if any problems appear when trying
        # to enable it on other platforms.
        ['OS=="win"', {
          'dependencies': [
            'crash_service',  # run time dependency
            'security_tests',  # run time dependency
            'test_support_common',
            '../google_update/google_update.gyp:google_update',
            '../views/views.gyp:views',
            # run time dependency
            '../webkit/tools/test_shell/test_shell.gyp:npapi_test_plugin',
            '<(allocator_target)',
          ],
          'link_settings': {
            'libraries': [
              '-lOleAcc.lib',
            ],
          },
          'configurations': {
            'Debug': {
              'msvs_settings': {
                'VCLinkerTool': {
                  'LinkIncremental': '<(msvs_large_module_debug_link_mode)',
                },
              },
            },
          },
        }],
      ],
    },
    {
      'target_name': 'unit_tests',
      'type': 'executable',
      'msvs_guid': 'ECFC2BEC-9FC0-4AD9-9649-5F26793F65FC',
      'dependencies': [
        'chrome',
        'browser',
        'chrome_resources',
        'chrome_strings',
        'common',
        'debugger',
        'renderer',
        'syncapi',
        'test_support_unit',
        'utility',
        '../app/app.gyp:app_base',
        '../app/app.gyp:app_resources',
        '../ipc/ipc.gyp:ipc',
        '../net/net.gyp:net_resources',
        '../net/net.gyp:net_test_support',
        '../printing/printing.gyp:printing',
        '../webkit/webkit.gyp:webkit_resources',
        '../skia/skia.gyp:skia',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
        '../third_party/bzip2/bzip2.gyp:bzip2',
        '../third_party/icu/icu.gyp:icui18n',
        '../third_party/icu/icu.gyp:icuuc',
        '../third_party/libxml/libxml.gyp:libxml',
        '../third_party/npapi/npapi.gyp:npapi',
        '../third_party/WebKit/WebKit/chromium/WebKit.gyp:webkit',
      ],
      'include_dirs': [
        '..',
      ],
      'defines': [
        'CLD_WINDOWS',
      ],
      'direct_dependent_settings': {
        'defines': [
          'CLD_WINDOWS',
        ],
      },
      'sources': [
        'app/breakpad_mac_stubs.mm',
        # All unittests in browser, common, and renderer.
        'browser/app_controller_mac_unittest.mm',
        'browser/autocomplete/autocomplete_edit_view_mac_unittest.mm',
        'browser/autocomplete/autocomplete_unittest.cc',
        'browser/autocomplete/autocomplete_popup_view_mac_unittest.mm',
        'browser/autocomplete/history_contents_provider_unittest.cc',
        'browser/autocomplete/history_url_provider_unittest.cc',
        'browser/autocomplete/keyword_provider_unittest.cc',
        'browser/autocomplete/search_provider_unittest.cc',
        'browser/back_forward_menu_model_unittest.cc',
        'browser/blocked_popup_container_unittest.cc',
        'browser/bookmarks/bookmark_codec_unittest.cc',
        'browser/bookmarks/bookmark_drag_data_unittest.cc',
        'browser/bookmarks/bookmark_folder_tree_model_unittest.cc',
        'browser/bookmarks/bookmark_html_writer_unittest.cc',
        'browser/bookmarks/bookmark_index_unittest.cc',
        'browser/bookmarks/bookmark_model_test_utils.cc',
        'browser/bookmarks/bookmark_model_test_utils.h',
        'browser/bookmarks/bookmark_model_unittest.cc',
        'browser/bookmarks/bookmark_table_model_unittest.cc',
        'browser/bookmarks/bookmark_utils_unittest.cc',
        'browser/browser_commands_unittest.cc',
        'browser/browser_theme_provider_unittest.cc',
        'browser/browser_unittest.cc',
        'browser/chrome_browser_application_mac_unittest.mm',
        'browser/debugger/devtools_remote_message_unittest.cc',
        'browser/debugger/devtools_remote_listen_socket_unittest.cc',
        'browser/debugger/devtools_remote_listen_socket_unittest.h',
        'browser/diagnostics/diagnostics_model_unittest.cc',
        'browser/child_process_security_policy_unittest.cc',
        'browser/chrome_thread_unittest.cc',
        'browser/chromeos/external_cookie_handler_unittest.cc',
        'browser/chromeos/external_metrics_unittest.cc',
        'browser/chromeos/gview_request_interceptor_unittest.cc',
        'browser/chromeos/pipe_reader_unittest.cc',
        'browser/chromeos/version_loader_unittest.cc',
        # It is safe to list */cocoa/* files in the "common" file list
        # without an explicit exclusion since gyp is smart enough to
        # exclude them from non-Mac builds.
        'browser/cocoa/about_ipc_controller_unittest.mm',
        'browser/cocoa/about_window_controller_unittest.mm',
        'browser/cocoa/animatable_view_unittest.mm',
        'browser/cocoa/autocomplete_text_field_cell_unittest.mm',
        'browser/cocoa/autocomplete_text_field_editor_unittest.mm',
        'browser/cocoa/autocomplete_text_field_unittest.mm',
        'browser/cocoa/autocomplete_text_field_unittest_helper.mm',
        'browser/cocoa/base_view_unittest.mm',
        'browser/cocoa/background_gradient_view_unittest.mm',
        'browser/cocoa/background_tile_view_unittest.mm',
        'browser/cocoa/blocked_popup_container_controller_unittest.mm',
        'browser/cocoa/bookmark_all_tabs_controller_unittest.mm',
        'browser/cocoa/bookmark_bar_bridge_unittest.mm',
        'browser/cocoa/bookmark_bar_controller_unittest.mm',
        'browser/cocoa/bookmark_bar_toolbar_view_unittest.mm',
        'browser/cocoa/bookmark_bar_view_unittest.mm',
        'browser/cocoa/bookmark_bubble_controller_unittest.mm',
        'browser/cocoa/bookmark_button_unittest.mm',
        'browser/cocoa/bookmark_button_cell_unittest.mm',
        'browser/cocoa/bookmark_editor_base_controller_unittest.mm',
        'browser/cocoa/bookmark_editor_controller_unittest.mm',
        'browser/cocoa/bookmark_menu_unittest.mm',
        'browser/cocoa/bookmark_menu_bridge_unittest.mm',
        'browser/cocoa/bookmark_menu_cocoa_controller_unittest.mm',
        'browser/cocoa/bookmark_name_folder_controller_unittest.mm',
        'browser/cocoa/bookmark_tree_browser_cell_unittest.mm',
        'browser/cocoa/browser_frame_view_unittest.mm',
        'browser/cocoa/browser_window_cocoa_unittest.mm',
        'browser/cocoa/browser_window_controller_unittest.mm',
        'browser/cocoa/bubble_view_unittest.mm',
        'browser/cocoa/bug_report_window_controller_unittest.mm',
        'browser/cocoa/chrome_browser_window_unittest.mm',
        'browser/cocoa/chrome_event_processing_window_unittest.mm',
        'browser/cocoa/clear_browsing_data_controller_unittest.mm',
        'browser/cocoa/clickhold_button_cell_unittest.mm',
        'browser/cocoa/cocoa_test_helper.h',
        'browser/cocoa/cocoa_test_helper.mm',
        'browser/cocoa/command_observer_bridge_unittest.mm',
        'browser/cocoa/custom_home_pages_model_unittest.mm',
        'browser/cocoa/delayedmenu_button_unittest.mm',
        'browser/cocoa/download_shelf_mac_unittest.mm',
        'browser/cocoa/download_shelf_view_unittest.mm',
        'browser/cocoa/download_util_mac_unittest.mm',
        'browser/cocoa/edit_search_engine_cocoa_controller_unittest.mm',
        'browser/cocoa/extension_shelf_controller_unittest.mm',
        'browser/cocoa/extensions/extension_popup_controller_unittest.mm',
        'browser/cocoa/event_utils_unittest.mm',
        'browser/cocoa/fast_resize_view_unittest.mm',
        'browser/cocoa/find_bar_bridge_unittest.mm',
        'browser/cocoa/find_bar_cocoa_controller_unittest.mm',
        'browser/cocoa/find_bar_text_field_unittest.mm',
        'browser/cocoa/find_bar_text_field_cell_unittest.mm',
        'browser/cocoa/find_bar_view_unittest.mm',
        'browser/cocoa/find_pasteboard_unittest.mm',
        'browser/cocoa/focus_tracker_unittest.mm',
        'browser/cocoa/fullscreen_window_unittest.mm',
        'browser/cocoa/html_dialog_window_controller_unittest.mm',
        'browser/cocoa/hung_renderer_controller_unittest.mm',
        'browser/cocoa/info_bubble_view_unittest.mm',
        'browser/cocoa/info_bubble_window_unittest.mm',
        'browser/cocoa/infobar_container_controller_unittest.mm',
        'browser/cocoa/infobar_controller_unittest.mm',
        'browser/cocoa/infobar_gradient_view_unittest.mm',
        'browser/cocoa/infobar_text_field_unittest.mm',
        'browser/cocoa/keystone_glue_unittest.mm',
        'browser/cocoa/keyword_editor_cocoa_controller_unittest.mm',
        'browser/cocoa/location_bar_view_mac_unittest.mm',
        'browser/cocoa/gradient_button_cell_unittest.mm',
        'browser/cocoa/history_menu_bridge_unittest.mm',
        'browser/cocoa/history_menu_cocoa_controller_unittest.mm',
        'browser/cocoa/hyperlink_button_cell_unittest.mm',
        'browser/cocoa/menu_button_unittest.mm',
        'browser/cocoa/nsimage_cache_unittest.mm',
        'browser/cocoa/nsmenuitem_additions_unittest.mm',
        'browser/cocoa/nswindow_local_state_unittest.mm',
        'browser/cocoa/objc_method_swizzle_unittest.mm',
        'browser/cocoa/page_info_window_controller_unittest.mm',
        'browser/cocoa/preferences_window_controller_unittest.mm',
        'browser/cocoa/rwhvm_editcommand_helper_unittest.mm',
        'browser/cocoa/sad_tab_view_unittest.mm',
        'browser/cocoa/search_engine_list_model_unittest.mm',
        'browser/cocoa/status_bubble_mac_unittest.mm',
        'browser/cocoa/styled_text_field_test_helper.h',
        'browser/cocoa/styled_text_field_test_helper.mm',
        'browser/cocoa/styled_text_field_unittest.mm',
        'browser/cocoa/styled_text_field_cell_unittest.mm',
        'browser/cocoa/tab_controller_unittest.mm',
        'browser/cocoa/tab_strip_controller_unittest.mm',
        'browser/cocoa/tab_strip_view_unittest.mm',
        'browser/cocoa/tab_view_unittest.mm',
        'browser/cocoa/task_manager_mac_unittest.mm',
        'browser/cocoa/test_event_utils.h',
        'browser/cocoa/test_event_utils.mm',
        'browser/cocoa/throbber_view_unittest.mm',
        'browser/cocoa/toolbar_button_cell_unittest.mm',
        'browser/cocoa/toolbar_controller_unittest.mm',
        'browser/cocoa/toolbar_view_unittest.mm',
        'browser/cocoa/view_resizer_pong.h',
        'browser/cocoa/view_resizer_pong.mm',
        'browser/cocoa/web_drop_target_unittest.mm',
        'browser/command_updater_unittest.cc',
        'browser/cookies_tree_model_unittest.cc',
        'browser/debugger/devtools_manager_unittest.cc',
        'browser/dom_ui/dom_ui_theme_source_unittest.cc',
        'browser/dom_ui/dom_ui_unittest.cc',
        'browser/dom_ui/shown_sections_handler_unittest.cc',
        'browser/download/download_manager_unittest.cc',
        'browser/download/download_request_infobar_delegate_unittest.cc',
        'browser/download/download_request_manager_unittest.cc',
        'browser/download/save_package_unittest.cc',
        'browser/encoding_menu_controller_unittest.cc',
        'browser/extensions/convert_user_script_unittest.cc',
        'browser/extensions/extension_file_util_unittest.cc',
        'browser/extensions/extension_messages_unittest.cc',
        'browser/extensions/extension_process_manager_unittest.cc',
        'browser/extensions/extension_ui_unittest.cc',
        'browser/extensions/extension_updater_unittest.cc',
        'browser/extensions/extensions_service_unittest.cc',
        'browser/extensions/file_reader_unittest.cc',
        'browser/extensions/sandboxed_extension_unpacker_unittest.cc',
        'browser/extensions/user_script_listener_unittest.cc',
        'browser/extensions/user_script_master_unittest.cc',
        'browser/find_backend_unittest.cc',
        'browser/global_keyboard_shortcuts_mac_unittest.cc',
        'browser/google_url_tracker_unittest.cc',
        'browser/google_update_settings_posix_unittest.cc',
        'browser/gtk/bookmark_bar_gtk_unittest.cc',
        'browser/gtk/bookmark_editor_gtk_unittest.cc',
        'browser/gtk/gtk_theme_provider_unittest.cc',
        'browser/gtk/go_button_gtk_unittest.cc',
        'browser/gtk/keyword_editor_view_unittest.cc',
        'browser/gtk/options/cookies_view_unittest.cc',
        'browser/gtk/options/languages_page_gtk_unittest.cc',
        'browser/gtk/tabs/tab_renderer_gtk_unittest.cc',
        'browser/history/expire_history_backend_unittest.cc',
        'browser/history/history_backend_unittest.cc',
        'browser/history/history_querying_unittest.cc',
        'browser/history/history_types_unittest.cc',
        'browser/history/history_unittest.cc',
        'browser/history/query_parser_unittest.cc',
        'browser/history/snippet_unittest.cc',
        'browser/history/starred_url_database_unittest.cc',
        'browser/history/text_database_manager_unittest.cc',
        'browser/history/text_database_unittest.cc',
        'browser/history/thumbnail_database_unittest.cc',
        'browser/history/top_sites_unittest.cc',
        'browser/thumbnail_store_unittest.cc',
        'browser/history/url_database_unittest.cc',
        'browser/history/visit_database_unittest.cc',
        'browser/history/visit_tracker_unittest.cc',
        'browser/importer/firefox_importer_unittest_messages_internal.h',
        'browser/importer/firefox_importer_unittest.cc',
        'browser/importer/firefox_importer_unittest_utils.h',
        'browser/importer/firefox_importer_unittest_utils_mac.cc',
        'browser/importer/importer_unittest.cc',
        'browser/importer/safari_importer_unittest.mm',
        'browser/importer/toolbar_importer_unittest.cc',
        'browser/importer/firefox_profile_lock_unittest.cc',
        'browser/in_process_webkit/dom_storage_dispatcher_host_unittest.cc',
        'browser/in_process_webkit/webkit_context_unittest.cc',
        'browser/in_process_webkit/webkit_thread_unittest.cc',
        'browser/keychain_mock_mac.cc',
        'browser/keychain_mock_mac.h',
        'browser/login_prompt_unittest.cc',
        'browser/meta_table_helper_unittest.cc',
        'browser/metrics/metrics_log_unittest.cc',
        'browser/metrics/metrics_response_unittest.cc',
        'browser/metrics/metrics_service_unittest.cc',
        'browser/net/chrome_url_request_context_unittest.cc',
        'browser/net/dns_host_info_unittest.cc',
        'browser/net/dns_master_unittest.cc',
        'browser/net/resolve_proxy_msg_helper_unittest.cc',
        'browser/net/test_url_fetcher_factory.cc',
        'browser/net/test_url_fetcher_factory.h',
        'browser/net/url_fetcher_unittest.cc',
        'browser/net/url_fixer_upper_unittest.cc',
        'browser/notifications/desktop_notifications_unittest.cc',
        'browser/notifications/desktop_notifications_unittest.h',
        'browser/password_manager/encryptor_unittest.cc',
        'browser/password_manager/login_database_unittest.cc',
        'browser/password_manager/password_form_manager_unittest.cc',
        'browser/password_manager/password_store_mac_unittest.cc',
        'browser/printing/print_job_unittest.cc',
        'browser/privacy_blacklist/blacklist_manager_unittest.cc',
        'browser/privacy_blacklist/blacklist_unittest.cc',
        'browser/privacy_blacklist/blacklist_io_unittest.cc',
        'browser/process_info_snapshot_mac_unittest.cc',
        'browser/profile_manager_unittest.cc',
        'browser/renderer_host/audio_renderer_host_unittest.cc',
        'browser/renderer_host/render_widget_host_unittest.cc',
        'browser/renderer_host/resource_dispatcher_host_unittest.cc',
        'browser/renderer_host/test/render_view_host_unittest.cc',
        'browser/renderer_host/test/site_instance_unittest.cc',
        'browser/renderer_host/web_cache_manager_unittest.cc',
        'browser/rlz/rlz_unittest.cc',
        'browser/safe_browsing/bloom_filter_unittest.cc',
        'browser/safe_browsing/chunk_range_unittest.cc',
        'browser/safe_browsing/protocol_manager_unittest.cc',
        'browser/safe_browsing/protocol_parser_unittest.cc',
        'browser/safe_browsing/safe_browsing_blocking_page_unittest.cc',
        'browser/safe_browsing/safe_browsing_database_unittest.cc',
        'browser/safe_browsing/safe_browsing_util_unittest.cc',
        'browser/search_engines/keyword_editor_controller_unittest.cc',
        'browser/search_engines/template_url_model_unittest.cc',
        'browser/search_engines/template_url_parser_unittest.cc',
        'browser/search_engines/template_url_prepopulate_data_unittest.cc',
        'browser/search_engines/template_url_scraper_unittest.cc',
        'browser/search_engines/template_url_unittest.cc',
        'browser/sessions/session_backend_unittest.cc',
        'browser/sessions/session_service_test_helper.cc',
        'browser/sessions/session_service_test_helper.h',
        'browser/sessions/session_service_unittest.cc',
        'browser/sessions/tab_restore_service_unittest.cc',
        'browser/shell_integration_unittest.cc',
        'browser/spellchecker_platform_engine_unittest.cc',
        'browser/ssl/ssl_host_state_unittest.cc',
        'browser/sync/glue/bookmark_model_worker_unittest.cc',
        'browser/sync/glue/http_bridge_unittest.cc',
        'browser/sync/profile_sync_service_unittest.cc',
        'browser/sync/sync_setup_wizard_unittest.cc',
        'browser/sync/sync_ui_util_mac_unittest.mm',
        'browser/tab_contents/navigation_controller_unittest.cc',
        'browser/tab_contents/navigation_entry_unittest.cc',
        'browser/tab_contents/render_view_host_manager_unittest.cc',
        'browser/tab_contents/thumbnail_generator_unittest.cc',
        'browser/tab_contents/web_contents_unittest.cc',
        'browser/tabs/tab_strip_model_unittest.cc',
        'browser/task_manager_unittest.cc',
        'browser/theme_resources_util_unittest.cc',
        'browser/views/bookmark_context_menu_test.cc',
        'browser/views/bookmark_editor_view_unittest.cc',
        'browser/visitedlink_unittest.cc',
        'browser/webdata/web_data_service_unittest.cc',
        'browser/webdata/web_database_unittest.cc',
        'browser/window_sizer_unittest.cc',
        'common/bzip2_unittest.cc',
        'common/child_process_logging_mac_unittest.mm',
        'common/chrome_plugin_unittest.cc',
        'common/common_param_traits_unittest.cc',
        'common/extensions/extension_resource_unittest.cc',
        'common/extensions/extension_unittest.cc',
        'common/extensions/extension_action_unittest.cc',
        'common/extensions/extension_l10n_util_unittest.cc',
        'common/extensions/extension_message_bundle_unittest.cc',
        'common/extensions/extension_unpacker_unittest.cc',
        'common/extensions/update_manifest_unittest.cc',
        'common/extensions/url_pattern_unittest.cc',
        'common/extensions/user_script_unittest.cc',
        'common/important_file_writer_unittest.cc',
        'common/json_value_serializer_unittest.cc',
        'common/mru_cache_unittest.cc',
        'common/net/url_util_unittest.cc',
        'common/notification_service_unittest.cc',
        'common/pref_member_unittest.cc',
        'common/pref_service_unittest.cc',
        'common/property_bag_unittest.cc',
        'common/resource_dispatcher_unittest.cc',
        'common/thumbnail_score_unittest.cc',
        'common/time_format_unittest.cc',
        'common/worker_thread_ticker_unittest.cc',
        'common/zip_unittest.cc',
        'renderer/audio_message_filter_unittest.cc',
        'renderer/media/audio_renderer_impl_unittest.cc',
        'renderer/extensions/extension_api_client_unittest.cc',
        'renderer/extensions/json_schema_unittest.cc',
        'renderer/net/render_dns_master_unittest.cc',
        'renderer/net/render_dns_queue_unittest.cc',
        'renderer/paint_aggregator_unittest.cc',
        'renderer/render_process_unittest.cc',
        'renderer/render_thread_unittest.cc',
        'renderer/render_view_unittest.cc',
        'renderer/render_view_unittest_mac.mm',
        'renderer/render_widget_unittest.cc',
        'renderer/renderer_main_unittest.cc',
        'renderer/spellchecker/spellcheck_unittest.cc',
        'test/browser_with_test_window_test.cc',
        'test/browser_with_test_window_test.h',
        'test/file_test_utils.cc',
        'test/file_test_utils.h',
        'test/render_view_test.cc',
        'test/render_view_test.h',
        'test/sync/test_http_bridge_factory.h',
        'test/test_notification_tracker.cc',
        'test/test_notification_tracker.h',
        'test/v8_unit_test.cc',
        'test/v8_unit_test.h',
        '../third_party/cld/bar/toolbar/cld/i18n/encodings/compact_lang_det/compact_lang_det_unittest_small.cc',
      ],
      'conditions': [
        ['chromeos==0 and toolkit_views==0', {
          'sources/': [
            ['exclude', '^browser/chromeos'],
          ],
        }],
        ['OS=="linux" and selinux==0', {
          'dependencies': [
            '../sandbox/sandbox.gyp:*',
          ],
        }],
        ['OS=="linux"', {
          'conditions': [
            [ 'gcc_version==44', {
              # Avoid gcc 4.4 strict aliasing issues in stl_tree.h when
              # building mru_cache_unittest.cc.
              'cflags': [
                '-fno-strict-aliasing',
              ],
            }],
          ],
          'dependencies': [
            '../build/linux/system.gyp:gtk',
            '../build/linux/system.gyp:nss',
            '../tools/xdisplaycheck/xdisplaycheck.gyp:xdisplaycheck',
          ],
          'sources': [
            'browser/renderer_host/gtk_key_bindings_handler_unittest.cc',
          ],
          'sources!': [
            'browser/views/bookmark_context_menu_test.cc',
            # Compact Language Detection (cld) is not supported in linux yet.
            '../third_party/cld/bar/toolbar/cld/i18n/encodings/compact_lang_det/compact_lang_det_unittest_small.cc',
          ],
        }],
        ['OS=="linux" and (toolkit_views==1 or chromeos==1)', {
          'dependencies': [
            '../views/views.gyp:views',
          ],
          'sources': [
            '<@(views_unit_tests_sources)',
          ],
          # We must use 'sources/' instead of 'source!' as there is a
          # target-default 'sources/' including gtk_unittest and 'source/' takes
          # precedence over 'sources!'.
          'sources/': [
             ['exclude', 'browser/gtk/bookmark_bar_gtk_unittest\\.cc$'],
             ['exclude', 'browser/gtk/bookmark_editor_gtk_unittest\\.cc$'],
             ['exclude', 'browser/gtk/go_button_gtk_unittest\\.cc$'],
             ['exclude', 'browser/gtk/tabs/tab_renderer_gtk_unittest\\.cc$'],
             ['exclude', 'browser/gtk/options/cookies_view_unittest\\.cc$'],
             ['exclude', 'browser/gtk/options/languages_page_gtk_unittest\\.cc$'],
             ['exclude', 'browser/gtk/gtk_theme_provider_unittest\\.cc$'],
          ],
          'conditions': [
            ['chromeos==0', {
              'sources/': [
                 ['include', 'browser/views/bookmark_context_menu_test.cc$'],
              ],
            }]
          ],
        }],
        ['OS=="mac"', {
           # The test fetches resources which means Mac need the app bundle to
           # exist on disk so it can pull from it.
          'dependencies': [
            'chrome',
            '../third_party/ocmock/ocmock.gyp:ocmock',
          ],
          'include_dirs': [
            '../third_party/GTM',
            '../third_party/GTM/AppKit',
          ],
          'sources!': [
            'browser/views/bookmark_context_menu_test.cc',
            'browser/back_forward_menu_model_unittest.cc',
            'browser/gtk/go_button_gtk_unittest.cc',
            'browser/gtk/tabs/tab_renderer_gtk_unittest.cc',
            'browser/tab_contents/navigation_controller_unittest.cc',
            'browser/task_manager_unittest.cc',
            '../third_party/hunspell/google/hunspell_tests.cc',

            # Compact Language Detection (cld) is not supported in mac yet.
            '../third_party/cld/bar/toolbar/cld/i18n/encodings/compact_lang_det/compact_lang_det_unittest_small.cc',
          ],
          # TODO(mark): We really want this for all non-static library targets,
          # but when we tried to pull it up to the common.gypi level, it broke
          # other things like the ui, startup, and page_cycler tests. *shrug*
          'xcode_settings': {'OTHER_LDFLAGS': ['-Wl,-ObjC']},
        }, { # OS != "mac"
          'dependencies': [
            'convert_dict_lib',
            '../third_party/hunspell/hunspell.gyp:hunspell',
          ],
        }],
        ['OS=="win"', {
          'defines': [
            '__STD_C',
            '_CRT_SECURE_NO_DEPRECATE',
            '_SCL_SECURE_NO_DEPRECATE',
          ],
          'dependencies': [
            'chrome_dll_version',
            'installer/installer.gyp:installer_util_strings',
            '../views/views.gyp:views',
            'test_chrome_plugin',  # run time dependency
            '<(allocator_target)',
          ],
          'include_dirs': [
            'third_party/wtl/include',
          ],
          'sources': [
            'app/chrome_dll.rc',
            'test/data/resource.rc',

            '<@(views_unit_tests_sources)',
            '<@(views_unit_tests_sources_win_specific)',

            # TODO:  It would be nice to have these pulled in
            # automatically from direct_dependent_settings in
            # their various targets (net.gyp:net_resources, etc.),
            # but that causes errors in other targets when
            # resulting .res files get referenced multiple times.
            '<(SHARED_INTERMEDIATE_DIR)/app/app_resources/app_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/browser_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/common_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/renderer_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/theme_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/net/net_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/webkit/webkit_resources.rc',
          ],
          'sources!': [
            'browser/gtk/tabs/tab_renderer_gtk_unittest.cc',
            'common/net/url_util_unittest.cc',
          ],
          'link_settings': {
            'libraries': [
              '-lcomsupp.lib',
              '-loleacc.lib',
              '-lrpcrt4.lib',
              '-lurlmon.lib',
              '-lwinmm.lib',
            ],
          },
          'configurations': {
            'Debug': {
              'msvs_settings': {
                'VCLinkerTool': {
                  'LinkIncremental': '<(msvs_large_module_debug_link_mode)',
                },
              },
            },
          },
        }, { # else: OS != "win"
          'sources!': [
            'browser/bookmarks/bookmark_codec_unittest.cc',
            'browser/bookmarks/bookmark_drag_data_unittest.cc',
            'browser/bookmarks/bookmark_folder_tree_model_unittest.cc',
            'browser/bookmarks/bookmark_table_model_unittest.cc',
            'browser/browser_unittest.cc',
            'browser/extensions/extension_process_manager_unittest.cc',
            'browser/importer/importer_unittest.cc',
            'browser/login_prompt_unittest.cc',
            'browser/printing/print_job_unittest.cc',
            'browser/rlz/rlz_unittest.cc',
            'browser/safe_browsing/safe_browsing_blocking_page_unittest.cc',
            'browser/search_engines/template_url_scraper_unittest.cc',
            'browser/views/bookmark_editor_view_unittest.cc',
            'browser/views/find_bar_host_unittest.cc',
            'browser/views/keyword_editor_view_unittest.cc',
            'common/chrome_plugin_unittest.cc',
            'common/net/url_util_unittest.cc',
          ],
        }],
      ],
    },
    {
      # Executable that runs each browser test in a new process.
      'target_name': 'browser_tests',
      'type': 'executable',
      'msvs_guid': 'D7589D0D-304E-4589-85A4-153B7D84B07F',
      'dependencies': [
        'browser',
        'chrome',
        'chrome_resources',
        'chrome_strings',
        'debugger',
        'syncapi',
        'renderer',
        'test_support_common',
        '../app/app.gyp:app_base',
        '../base/base.gyp:base',
        '../base/base.gyp:base_i18n',
        '../base/base.gyp:test_support_base',
        '../skia/skia.gyp:skia',
        '../testing/gtest.gyp:gtest',
        '../third_party/icu/icu.gyp:icui18n',
        '../third_party/icu/icu.gyp:icuuc',
      ],
      'include_dirs': [
        '..',
      ],
      'defines': [ 'ALLOW_IN_PROC_BROWSER_TEST' ],
      'sources': [
        'test/in_process_browser_test.cc',
        'test/in_process_browser_test.h',
        'test/test_launcher/out_of_proc_test_runner.cc',
        'test/test_launcher/test_runner.cc',
        'test/test_launcher/test_runner.h',
        'test/test_launcher/run_all_unittests.cc',
        'test/unit/chrome_test_suite.h',
        # browser_tests_sources is defined in 'variables' at the top of the
        # file.
        '<@(browser_tests_sources)',
      ],
      'conditions': [
        ['OS=="win"', {
          'sources': [
            'app/chrome_dll.rc',
            'app/chrome_dll_resource.h',
            'app/chrome_dll_version.rc.version',
            '<(SHARED_INTERMEDIATE_DIR)/app/app_resources/app_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/browser_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome_dll_version/chrome_dll_version.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/common_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/theme_resources.rc',
            '<@(browser_tests_sources_win_specific)',
            '<@(browser_tests_sources_views_specific)'
          ],
          'include_dirs': [
            'third_party/wtl/include',
          ],
          'dependencies': [
            'chrome_dll_version',
            'installer/installer.gyp:installer_util_strings',
            '<(allocator_target)',
          ],
          'configurations': {
            'Debug': {
              'msvs_settings': {
                'VCLinkerTool': {
                  'LinkIncremental': '<(msvs_large_module_debug_link_mode)',
                },
              },
            },
          }
        }],
        ['OS=="linux"', {
          'dependencies': [
            '../build/linux/system.gyp:gtk',
            '../tools/xdisplaycheck/xdisplaycheck.gyp:xdisplaycheck',
          ],
        }],
        ['OS=="linux" and (toolkit_views==1 or chromeos==1)', {
          'dependencies': [
            '../views/views.gyp:views',
          ],
        }],
        ['OS=="linux" and toolkit_views==1', {
          'sources': [
            '<@(browser_tests_sources_views_specific)',
          ],
        }],
        ['OS=="linux" and toolkit_views==0', {
          'sources': [
            '<@(browser_tests_sources_gtk_specific)',
          ],
        }],
        ['OS=="mac"', {
          'sources': [
            'app/breakpad_mac_stubs.mm',
          ],
          'sources!': [
            '<@(browser_tests_sources_exclude_on_mac)',
          ],
          # TODO(mark): We really want this for all non-static library
          # targets, but when we tried to pull it up to the common.gypi
          # level, it broke other things like the ui, startup, and
          # page_cycler tests. *shrug*
          'xcode_settings': {
            'OTHER_LDFLAGS': [
              '-Wl,-ObjC',
            ],
          },
        }],
      ],  # conditions
    },  # target browser_tests
    {
      'target_name': 'startup_tests',
      'type': 'executable',
      'msvs_guid': 'D3E6C0FD-54C7-4FF2-9AE1-72F2DAFD820C',
      'dependencies': [
        'chrome',
        'browser',
        'debugger',
        'common',
        'chrome_resources',
        'chrome_strings',
        'test_support_ui',
        '../app/app.gyp:app_base',
        '../base/base.gyp:base',
        '../skia/skia.gyp:skia',
        '../testing/gtest.gyp:gtest',
      ],
      'sources': [
        'test/startup/feature_startup_test.cc',
        'test/startup/startup_test.cc',
      ],
      'conditions': [
        ['OS=="linux"', {
          'dependencies': [
            '../build/linux/system.gyp:gtk',
            '../tools/xdisplaycheck/xdisplaycheck.gyp:xdisplaycheck',
          ],
        }],
        ['OS=="linux" and toolkit_views==1', {
          'dependencies': [
            '../views/views.gyp:views',
          ],
        }],
        ['OS=="win"', {
          'dependencies': [
            '<(allocator_target)',
          ],
        },],
      ],
    },
    {
      # To run the tests from page_load_test.cc on Linux, we need to:
      #
      #   a) Build with Breakpad (GYP_DEFINES="linux_chromium_breakpad=1")
      #   b) Run with CHROME_HEADLESS=1 to generate crash dumps.
      #   c) Strip the binary if it's a debug build. (binary may be over 2GB)
      'target_name': 'reliability_tests',
      'type': 'executable',
      'msvs_guid': '8A3E1774-1DE9-445C-982D-3EE37C8A752A',
      'dependencies': [
        'browser',
        'chrome',
        'test_support_common',
        'test_support_ui',
        'theme_resources',
        '../skia/skia.gyp:skia',
        '../testing/gtest.gyp:gtest',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'test/reliability/page_load_test.cc',
        'test/reliability/page_load_test.h',
        'test/reliability/reliability_test_suite.h',
        'test/reliability/run_all_unittests.cc',
      ],
      'conditions': [
        ['OS=="win"', {
          'dependencies': [
            '<(allocator_target)',
          ],
        },],
        ['OS=="linux"', {
          'dependencies': [
            '../build/linux/system.gyp:gtk',
          ],
        },],
      ],
    },
    {
      # Provides a syncapi dynamic library target from checked-in binaries,
      # or from compiling a stub implementation.
      'target_name': 'syncapi',
      'type': '<(library)',
      'sources': [
        'browser/sync/engine/syncapi.cc',
      ],
      'include_dirs': [
        '..',
        '<(protoc_out_dir)',
      ],
      'defines' : [
        '_CRT_SECURE_NO_WARNINGS',
        '_USE_32BIT_TIME_T',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../build/temp_gyp/googleurl.gyp:googleurl',
        '../net/net.gyp:net_base',
        '../third_party/icu/icu.gyp:icuuc',
        '../third_party/libjingle/libjingle.gyp:libjingle',
        '../third_party/sqlite/sqlite.gyp:sqlite',
        'common_constants',
        'notifier',
        'sync',
        'sync_proto',
      ],
    },
    {
      'target_name': 'page_cycler_tests',
      'type': 'executable',
      'msvs_guid': 'C9E0BD1D-B175-4A91-8380-3FDC81FAB9D7',
      'dependencies': [
        'chrome',
        'chrome_resources',
        'chrome_strings',
        'debugger',
        'test_support_common',
        'test_support_ui',
        '../base/base.gyp:base',
        '../skia/skia.gyp:skia',
        '../testing/gtest.gyp:gtest',
      ],
      'sources': [
        'test/page_cycler/page_cycler_test.cc',
      ],
      'conditions': [
        ['OS=="linux"', {
          'dependencies': [
            '../build/linux/system.gyp:gtk',
            '../tools/xdisplaycheck/xdisplaycheck.gyp:xdisplaycheck',
          ],
        }],
        ['OS=="win" or (OS=="linux" and toolkit_views==1)', {
          'dependencies': [
            '../views/views.gyp:views',
          ],
        }],
      ],
    },
    {
      'target_name': 'tab_switching_test',
      'type': 'executable',
      'msvs_guid': 'A34770EA-A574-43E8-9327-F79C04770E98',
      'run_as': {
        'action': ['$(TargetPath)', '--gtest_print_time', '-enable-logging',
                   '-dump-histograms-on-exit', '-log-level=0'],
      },
      'dependencies': [
        'chrome',
        'debugger',
        'test_support_common',
        'test_support_ui',
        'theme_resources',
        '../base/base.gyp:base',
        '../skia/skia.gyp:skia',
        '../testing/gtest.gyp:gtest',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'test/tab_switching/tab_switching_test.cc',
      ],
      'conditions': [
        ['OS=="linux"', {
          'dependencies': [
            '../build/linux/system.gyp:gtk',
            '../tools/xdisplaycheck/xdisplaycheck.gyp:xdisplaycheck',
          ],
        }],
        ['OS=="win"', {
          'dependencies': [
            '<(allocator_target)',
          ],
        },],
      ],
    },
    {
      'target_name': 'memory_test',
      'type': 'executable',
      'msvs_guid': 'A5F831FD-9B9C-4FEF-9FBA-554817B734CE',
      'dependencies': [
        'chrome',
        'debugger',
        'test_support_common',
        'test_support_ui',
        'theme_resources',
        '../base/base.gyp:base',
        '../skia/skia.gyp:skia',
        '../testing/gtest.gyp:gtest',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'test/memory_test/memory_test.cc',
      ],
      'conditions': [
        ['OS=="linux"', {
          'dependencies': [
            '../build/linux/system.gyp:gtk',
            '../tools/xdisplaycheck/xdisplaycheck.gyp:xdisplaycheck',
          ],
        }],
      ],
    },
    {
      'target_name': 'url_fetch_test',
      'type': 'executable',
      'msvs_guid': '7EFD0C91-198E-4043-9E71-4A4C7879B929',
      'dependencies': [
        'chrome',
        'debugger',
        'test_support_common',
        'test_support_ui',
        'theme_resources',
        '../base/base.gyp:base',
        '../net/net.gyp:net',
        '../skia/skia.gyp:skia',
        '../testing/gtest.gyp:gtest',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'test/url_fetch_test/url_fetch_test.cc',
      ],
      'conditions': [
        ['OS=="win"', {
          'include_dirs': [
            'third_party/wtl/include',
          ],
          'dependencies': [
            '<(allocator_target)',
          ],
        }], # OS="win"
      ], # conditions
    },
    {
      # Protobuf compiler / generate rule for sync.proto
      'target_name': 'sync_proto',
      'type': 'none',
      'actions': [
        {
          'action_name': 'compiling sync.proto',
          'inputs': [
            '<(PRODUCT_DIR)/<(EXECUTABLE_PREFIX)protoc<(EXECUTABLE_SUFFIX)',
            'browser/sync/protocol/sync.proto',
          ],
          'outputs': [
            '<(protoc_out_dir)/chrome/browser/sync/protocol/sync.pb.cc',
            '<(protoc_out_dir)/chrome/browser/sync/protocol/sync.pb.h',
          ],
          'action': [
            '<(PRODUCT_DIR)/<(EXECUTABLE_PREFIX)protoc<(EXECUTABLE_SUFFIX)',
            '--proto_path=browser/sync/protocol',
            'browser/sync/protocol/sync.proto',
            '--cpp_out=<(protoc_out_dir)/chrome/browser/sync/protocol',
          ],
        },
      ],
      'dependencies': [
        '../third_party/protobuf2/protobuf.gyp:protobuf_lite',
        '../third_party/protobuf2/protobuf.gyp:protoc#host',
      ],
      'export_dependent_settings': [
        '../third_party/protobuf2/protobuf.gyp:protobuf_lite',
      ],
    },
    {
      'target_name': 'notifier',
      'type': '<(library)',
      'sources': [
        'browser/sync/notifier/base/async_dns_lookup.cc',
        'browser/sync/notifier/base/async_dns_lookup.h',
        'browser/sync/notifier/base/async_network_alive.h',
        'browser/sync/notifier/base/fastalloc.h',
        'browser/sync/notifier/base/linux/network_status_detector_task_linux.cc',
        'browser/sync/notifier/base/mac/network_status_detector_task_mac.cc',
        'browser/sync/notifier/base/mac/time_mac.cc',
        'browser/sync/notifier/base/nethelpers.cc',
        'browser/sync/notifier/base/nethelpers.h',
        'browser/sync/notifier/base/network_status_detector_task.cc',
        'browser/sync/notifier/base/network_status_detector_task.h',
        'browser/sync/notifier/base/network_status_detector_task_mt.cc',
        'browser/sync/notifier/base/network_status_detector_task_mt.h',
        'browser/sync/notifier/base/posix/time_posix.cc',
        'browser/sync/notifier/base/signal_thread_task.h',
        'browser/sync/notifier/base/static_assert.h',
        'browser/sync/notifier/base/task_pump.cc',
        'browser/sync/notifier/base/task_pump.h',
        'browser/sync/notifier/base/time.cc',
        'browser/sync/notifier/base/time.h',
        'browser/sync/notifier/base/timer.cc',
        'browser/sync/notifier/base/timer.h',
        'browser/sync/notifier/base/utils.h',
        'browser/sync/notifier/base/win/async_network_alive_win32.cc',
        'browser/sync/notifier/base/win/time_win32.cc',
        'browser/sync/notifier/communicator/auth_task.cc',
        'browser/sync/notifier/communicator/auth_task.h',
        'browser/sync/notifier/communicator/auto_reconnect.cc',
        'browser/sync/notifier/communicator/auto_reconnect.h',
        'browser/sync/notifier/communicator/connection_options.cc',
        'browser/sync/notifier/communicator/connection_options.h',
        'browser/sync/notifier/communicator/connection_settings.cc',
        'browser/sync/notifier/communicator/connection_settings.h',
        'browser/sync/notifier/communicator/const_communicator.h',
        'browser/sync/notifier/communicator/login.cc',
        'browser/sync/notifier/communicator/login.h',
        'browser/sync/notifier/communicator/login_failure.cc',
        'browser/sync/notifier/communicator/login_failure.h',
        'browser/sync/notifier/communicator/login_settings.cc',
        'browser/sync/notifier/communicator/login_settings.h',
        'browser/sync/notifier/communicator/product_info.cc',
        'browser/sync/notifier/communicator/product_info.h',
        'browser/sync/notifier/communicator/single_login_attempt.cc',
        'browser/sync/notifier/communicator/single_login_attempt.h',
        'browser/sync/notifier/communicator/ssl_socket_adapter.cc',
        'browser/sync/notifier/communicator/ssl_socket_adapter.h',
        'browser/sync/notifier/communicator/talk_auth_task.cc',
        'browser/sync/notifier/communicator/talk_auth_task.h',
        'browser/sync/notifier/communicator/xmpp_connection_generator.cc',
        'browser/sync/notifier/communicator/xmpp_connection_generator.h',
        'browser/sync/notifier/communicator/xmpp_log.cc',
        'browser/sync/notifier/communicator/xmpp_log.h',
        'browser/sync/notifier/communicator/xmpp_socket_adapter.cc',
        'browser/sync/notifier/communicator/xmpp_socket_adapter.h',
        'browser/sync/notifier/gaia_auth/gaiaauth.cc',
        'browser/sync/notifier/gaia_auth/gaiaauth.h',
        'browser/sync/notifier/gaia_auth/gaiahelper.cc',
        'browser/sync/notifier/gaia_auth/gaiahelper.h',
        'browser/sync/notifier/gaia_auth/inet_aton.h',
        'browser/sync/notifier/gaia_auth/sigslotrepeater.h',
        'browser/sync/notifier/gaia_auth/win/win32window.cc',
        'browser/sync/notifier/listener/listen_task.cc',
        'browser/sync/notifier/listener/listen_task.h',
        'browser/sync/notifier/listener/mediator_thread.h',
        'browser/sync/notifier/listener/mediator_thread_impl.cc',
        'browser/sync/notifier/listener/mediator_thread_impl.h',
        'browser/sync/notifier/listener/mediator_thread_mock.h',
        'browser/sync/notifier/listener/send_update_task.cc',
        'browser/sync/notifier/listener/send_update_task.h',
        'browser/sync/notifier/listener/subscribe_task.cc',
        'browser/sync/notifier/listener/subscribe_task.h',
        'browser/sync/notifier/listener/talk_mediator.h',
        'browser/sync/notifier/listener/talk_mediator_impl.cc',
        'browser/sync/notifier/listener/talk_mediator_impl.h',
      ],
      'include_dirs': [
        '..',
        '<(protoc_out_dir)',
      ],
      'defines' : [
        '_CRT_SECURE_NO_WARNINGS',
        '_USE_32BIT_TIME_T',
        'kXmppProductName="chromium-sync"',
      ],
      'dependencies': [
        '../third_party/expat/expat.gyp:expat',
        '../third_party/libjingle/libjingle.gyp:libjingle',
        'sync_proto',
      ],
      'conditions': [
        ['OS=="linux"', {
          'sources!': [
            'browser/sync/notifier/base/network_status_detector_task_mt.cc',
          ],
          'dependencies': [
            '../build/linux/system.gyp:gtk'
          ],
        }],
      ],
    },
    {
      'target_name': 'sync_unit_tests',
      'type': 'executable',
      'sources': [
        'app/breakpad_mac_stubs.mm',
        'browser/sync/engine/all_status_unittest.cc',
        'browser/sync/engine/apply_updates_command_unittest.cc',
        'browser/sync/engine/auth_watcher_unittest.cc',
        'browser/sync/engine/net/gaia_authenticator_unittest.cc',
        'browser/sync/engine/syncer_proto_util_unittest.cc',
        'browser/sync/engine/syncer_thread_unittest.cc',
        'browser/sync/engine/syncer_unittest.cc',
        'browser/sync/engine/syncproto_unittest.cc',
        'browser/sync/notifier/listener/talk_mediator_unittest.cc',
        'browser/sync/sessions/status_controller_unittest.cc',
        'browser/sync/sessions/sync_session_unittest.cc',
        'browser/sync/syncable/syncable_id_unittest.cc',
        'browser/sync/syncable/syncable_unittest.cc',
        'browser/sync/util/crypto_helpers_unittest.cc',
        'browser/sync/util/event_sys_unittest.cc',
        'browser/sync/util/extensions_activity_monitor_unittest.cc',
        'browser/sync/util/query_helpers_unittest.cc',
        'browser/sync/util/user_settings_unittest.cc',
        'test/file_test_utils.cc',
        'test/sync/engine/mock_gaia_authenticator.cc',
        'test/sync/engine/mock_gaia_authenticator.h',
        'test/sync/engine/mock_gaia_authenticator_unittest.cc',
        'test/sync/engine/mock_server_connection.cc',
        'test/sync/engine/mock_server_connection.h',
        'test/sync/engine/test_directory_setter_upper.cc',
        'test/sync/engine/test_directory_setter_upper.h',
        'test/sync/engine/test_id_factory.h',
        'test/sync/engine/test_syncable_utils.cc',
        'test/sync/engine/test_syncable_utils.h',
      ],
      'include_dirs': [
        '..',
        '<(protoc_out_dir)',
      ],
      'defines' : [
        'SYNC_ENGINE_VERSION_STRING="Unknown"',
        '_CRT_SECURE_NO_WARNINGS',
        '_USE_32BIT_TIME_T',
      ],
      'dependencies': [
       'common',
       'debugger',
       '../skia/skia.gyp:skia',
       '../testing/gtest.gyp:gtest',
       '../third_party/libjingle/libjingle.gyp:libjingle',
       'syncapi',
       'sync_proto',
       'test_support_unit',
      ],
      'conditions': [
        ['OS=="win"', {
          'sources' : [
            'browser/sync/util/data_encryption_unittest.cc',
          ],
          'dependencies': [
            '<(allocator_target)',
          ],
          'link_settings': {
            'libraries': [
              '-lcrypt32.lib',
              '-lws2_32.lib',
              '-lsecur32.lib',
            ],
          },
        }, { # else: OS != "win"
          'sources!': [
            'browser/sync/util/data_encryption_unittest.cc',
          ],
        }],
        ['OS=="linux"', {
          'dependencies': [
            '../build/linux/system.gyp:gtk'
          ],
        }],
      ],
    },
    {
      'target_name': 'sync',
      'type': '<(library)',
      'sources': [
        '<(protoc_out_dir)/chrome/browser/sync/protocol/sync.pb.cc',
        '<(protoc_out_dir)/chrome/browser/sync/protocol/sync.pb.h',
        'browser/sync/engine/all_status.cc',
        'browser/sync/engine/all_status.h',
        'browser/sync/engine/apply_updates_command.cc',
        'browser/sync/engine/apply_updates_command.h',
        'browser/sync/engine/auth_watcher.cc',
        'browser/sync/engine/auth_watcher.h',
        'browser/sync/engine/authenticator.cc',
        'browser/sync/engine/authenticator.h',
        'browser/sync/engine/build_and_process_conflict_sets_command.cc',
        'browser/sync/engine/build_and_process_conflict_sets_command.h',
        'browser/sync/engine/build_commit_command.cc',
        'browser/sync/engine/build_commit_command.h',
        'browser/sync/engine/change_reorder_buffer.cc',
        'browser/sync/engine/change_reorder_buffer.h',
        'browser/sync/engine/conflict_resolver.cc',
        'browser/sync/engine/conflict_resolver.h',
        'browser/sync/engine/download_updates_command.cc',
        'browser/sync/engine/download_updates_command.h',
        'browser/sync/engine/get_commit_ids_command.cc',
        'browser/sync/engine/get_commit_ids_command.h',
        'browser/sync/engine/model_changing_syncer_command.cc',
        'browser/sync/engine/model_changing_syncer_command.h',
        'browser/sync/engine/model_safe_worker.h',
        'browser/sync/engine/net/gaia_authenticator.cc',
        'browser/sync/engine/net/gaia_authenticator.h',
        'browser/sync/engine/net/http_return.h',
        'browser/sync/engine/net/server_connection_manager.cc',
        'browser/sync/engine/net/server_connection_manager.h',
        'browser/sync/engine/net/syncapi_server_connection_manager.cc',
        'browser/sync/engine/net/syncapi_server_connection_manager.h',
        'browser/sync/engine/net/url_translator.cc',
        'browser/sync/engine/net/url_translator.h',
        'browser/sync/engine/post_commit_message_command.cc',
        'browser/sync/engine/post_commit_message_command.h',
        'browser/sync/engine/process_commit_response_command.cc',
        'browser/sync/engine/process_commit_response_command.h',
        'browser/sync/engine/process_updates_command.cc',
        'browser/sync/engine/process_updates_command.h',
        'browser/sync/engine/resolve_conflicts_command.cc',
        'browser/sync/engine/resolve_conflicts_command.h',
        'browser/sync/engine/syncapi.h',
        'browser/sync/engine/syncer.cc',
        'browser/sync/engine/syncer.h',
        'browser/sync/engine/syncer_command.cc',
        'browser/sync/engine/syncer_command.h',
        'browser/sync/engine/syncer_end_command.cc',
        'browser/sync/engine/syncer_end_command.h',
        'browser/sync/engine/syncer_proto_util.cc',
        'browser/sync/engine/syncer_proto_util.h',
        'browser/sync/engine/syncer_thread.cc',
        'browser/sync/engine/syncer_thread.h',
        'browser/sync/engine/syncer_types.h',
        'browser/sync/engine/syncer_util.cc',
        'browser/sync/engine/syncer_util.h',
        'browser/sync/engine/syncproto.h',
        'browser/sync/engine/update_applicator.cc',
        'browser/sync/engine/update_applicator.h',
        'browser/sync/engine/verify_updates_command.cc',
        'browser/sync/engine/verify_updates_command.h',
        'browser/sync/protocol/service_constants.h',
        'browser/sync/sessions/session_state.cc',
        'browser/sync/sessions/session_state.h',
        'browser/sync/sessions/status_controller.cc',
        'browser/sync/sessions/status_controller.h',
        'browser/sync/sessions/sync_session.cc',
        'browser/sync/sessions/sync_session.h',
        'browser/sync/sessions/sync_session_context.h',
        'browser/sync/syncable/blob.h',
        'browser/sync/syncable/dir_open_result.h',
        'browser/sync/syncable/directory_backing_store.cc',
        'browser/sync/syncable/directory_backing_store.h',
        'browser/sync/syncable/directory_event.h',
        'browser/sync/syncable/directory_manager.cc',
        'browser/sync/syncable/directory_manager.h',
        'browser/sync/syncable/path_name_cmp.h',
        'browser/sync/syncable/syncable-inl.h',
        'browser/sync/syncable/syncable.cc',
        'browser/sync/syncable/syncable.h',
        'browser/sync/syncable/syncable_changes_version.h',
        'browser/sync/syncable/syncable_columns.h',
        'browser/sync/syncable/syncable_id.cc',
        'browser/sync/syncable/syncable_id.h',
        'browser/sync/util/character_set_converters.h',
        'browser/sync/util/character_set_converters_posix.cc',
        'browser/sync/util/character_set_converters_win.cc',
        'browser/sync/util/closure.h',
        'browser/sync/util/crypto_helpers.cc',
        'browser/sync/util/crypto_helpers.h',
        'browser/sync/util/dbgq.h',
        'browser/sync/util/event_sys-inl.h',
        'browser/sync/util/event_sys.h',
        'browser/sync/util/extensions_activity_monitor.cc',
        'browser/sync/util/extensions_activity_monitor.h',
        'browser/sync/util/fast_dump.h',
        'browser/sync/util/query_helpers.cc',
        'browser/sync/util/query_helpers.h',
        'browser/sync/util/row_iterator.h',
        'browser/sync/util/signin.h',
        'browser/sync/util/sync_types.h',
        'browser/sync/util/user_settings.cc',
        'browser/sync/util/user_settings.h',
        'browser/sync/util/user_settings_posix.cc',
        'browser/sync/util/user_settings_win.cc',
      ],
      'include_dirs': [
        '..',
        '<(protoc_out_dir)',
      ],
      'defines' : [
        'SYNC_ENGINE_VERSION_STRING="Unknown"',
        '_CRT_SECURE_NO_WARNINGS',
        '_USE_32BIT_TIME_T',
      ],
      'dependencies': [
        '../skia/skia.gyp:skia',
        '../third_party/libjingle/libjingle.gyp:libjingle',
        'sync_proto',
      ],
      'conditions': [
        ['OS=="win"', {
          'sources' : [
            'browser/sync/util/data_encryption.cc',
            'browser/sync/util/data_encryption.h',
          ],
        }],
        ['OS=="linux"', {
          'dependencies': [
            '../build/linux/system.gyp:gtk'
          ],
        }],
        ['OS=="mac"', {
          'link_settings': {
            'libraries': [
              '$(SDKROOT)/System/Library/Frameworks/IOKit.framework',
            ],
          },
        }],
      ],
    },
  ],
  'conditions': [
    ['OS=="mac" or OS=="win"', {
      'targets': [
        {
          'target_name': 'chrome_dll',
          'type': 'shared_library',
          'dependencies': [
            '<@(chromium_dependencies)',
          ],
          'conditions': [
            ['OS=="win"', {
              'product_name': 'chrome',
              'msvs_guid': 'C0A7EE2C-2A6D-45BE-BA78-6D006FDF52D9',
              'include_dirs': [
                'third_party/wtl/include',
              ],
              'dependencies': [
                # On Windows, link the dependencies (libraries) that make
                # up actual Chromium functionality into this .dll.
                'chrome_dll_version',
                'chrome_resources',
                'installer/installer.gyp:installer_util_strings',
                'worker',
                '../printing/printing.gyp:printing',
                '../net/net.gyp:net_resources',
                '../build/util/support/support.gyp:*',
                '../third_party/cld/cld.gyp:cld',
                '../views/views.gyp:views',
                '../webkit/webkit.gyp:webkit_resources',
                '../gears/gears.gyp:gears',
              ],
              'defines': [
                'CHROME_DLL',
                'BROWSER_DLL',
                'RENDERER_DLL',
                'PLUGIN_DLL',
              ],
              'sources': [
                'app/chrome_dll.rc',
                'app/chrome_dll_main.cc',
                'app/chrome_dll_resource.h',
                '<(SHARED_INTERMEDIATE_DIR)/chrome_dll_version/chrome_dll_version.rc',

                '../webkit/glue/resources/aliasb.cur',
                '../webkit/glue/resources/cell.cur',
                '../webkit/glue/resources/col_resize.cur',
                '../webkit/glue/resources/copy.cur',
                '../webkit/glue/resources/row_resize.cur',
                '../webkit/glue/resources/vertical_text.cur',
                '../webkit/glue/resources/zoom_in.cur',
                '../webkit/glue/resources/zoom_out.cur',

                # TODO:  It would be nice to have these pulled in
                # automatically from direct_dependent_settings in
                # their various targets (net.gyp:net_resources, etc.),
                # but that causes errors in other targets when
                # resulting .res files get referenced multiple times.
                '<(SHARED_INTERMEDIATE_DIR)/app/app_resources/app_resources.rc',
                '<(SHARED_INTERMEDIATE_DIR)/chrome/browser_resources.rc',
                '<(SHARED_INTERMEDIATE_DIR)/chrome/common_resources.rc',
                '<(SHARED_INTERMEDIATE_DIR)/chrome/renderer_resources.rc',
                '<(SHARED_INTERMEDIATE_DIR)/chrome/theme_resources.rc',
                '<(SHARED_INTERMEDIATE_DIR)/net/net_resources.rc',
                '<(SHARED_INTERMEDIATE_DIR)/webkit/webkit_resources.rc',

                # TODO(sgk):  left-over from pre-gyp build, figure out
                # if we still need them and/or how to update to gyp.
                #'app/check_dependents.bat',
                #'app/chrome.dll.deps',
              ],
              'msvs_settings': {
                'VCLinkerTool': {
                  'BaseAddress': '0x01c30000',
                  'DelayLoadDLLs': [
                    'crypt32.dll',
                    'cryptui.dll',
                    'winhttp.dll',
                    'wininet.dll',
                    'wsock32.dll',
                    'ws2_32.dll',
                    'winspool.drv',
                    'comdlg32.dll',
                    'imagehlp.dll',
                    'urlmon.dll',
                    'imm32.dll',
                    'iphlpapi.dll',
                  ],
                  'ImportLibrary': '$(OutDir)\\lib\\chrome_dll.lib',
                  'ProgramDatabaseFile': '$(OutDir)\\chrome_dll.pdb',
                  # Set /SUBSYSTEM:WINDOWS for chrome.dll (for consistency).
                  'SubSystem': '2',
                },
                'VCManifestTool': {
                  'AdditionalManifestFiles': '$(ProjectDir)\\app\\chrome.dll.manifest',
                },
              },
              'configurations': {
                'Debug': {
                  'msvs_settings': {
                    'VCLinkerTool': {
                      'LinkIncremental': '<(msvs_large_module_debug_link_mode)',
                    },
                  },
                },
              },
            }],  # OS=="win"
            ['OS=="mac"', {
              # The main browser executable's name is <(mac_product_name).
              # Certain things will get confused if two modules in the
              # executable share the same name, so append " Framework" to the
              # product_name used for the framework.  This will result in
              # a name like "Chromium Framework.framework".
              'product_name': '<(mac_product_name) Framework',
              'mac_bundle': 1,
              'xcode_settings': {
                'CHROMIUM_BUNDLE_ID': '<(mac_bundle_id)',

                # The dylib versions are of the form a[.b[.c]], where a is a
                # 16-bit unsigned integer, and b and c are 8-bit unsigned
                # integers.  Any missing component is taken to be 0.  The
                # best mapping from product version numbers into this scheme
                # is to just use the build and patch numbers.  There is no
                # ambiguity in this scheme because the build number is
                # guaranteed unique even across distinct major and minor
                # version numbers.  These settings correspond to
                # -compatibility_version and -current_version.
                'DYLIB_COMPATIBILITY_VERSION': '<(version_build_patch)',
                'DYLIB_CURRENT_VERSION': '<(version_build_patch)',

                # The framework is placed within the .app's versioned
                # directory.  DYLIB_INSTALL_NAME_BASE and
                # LD_DYLIB_INSTALL_NAME affect -install_name.
                'DYLIB_INSTALL_NAME_BASE':
                    '@executable_path/../Versions/<(version_full)',
                # See tools/build/mac/copy_framework_unversioned for
                # information on LD_DYLIB_INSTALL_NAME.
                'LD_DYLIB_INSTALL_NAME':
                    '$(DYLIB_INSTALL_NAME_BASE:standardizepath)/$(WRAPPER_NAME)/$(PRODUCT_NAME)',

                'INFOPLIST_FILE': 'app/framework-Info.plist',

                # Define the order of symbols within the framework.  This
                # sets -order_file.
                'ORDER_FILE': 'app/framework.order',
              },
              'sources': [
                'app/chrome_dll_main.cc',
                'app/chrome_dll_resource.h',
                'app/chrome_exe_main.mm',
              ],
              # TODO(mark): Come up with a fancier way to do this.  It should
              # only be necessary to list framework-Info.plist once, not the
              # three times it is listed here.
              'mac_bundle_resources': [
                'app/framework-Info.plist',
                'app/nibs/About.xib',
                'app/nibs/AboutIPC.xib',
                'app/nibs/BookmarkAllTabs.xib',
                'app/nibs/BookmarkBar.xib',
                'app/nibs/BookmarkBubble.xib',
                'app/nibs/BookmarkEditor.xib',
                'app/nibs/BookmarkNameFolder.xib',
                'app/nibs/BrowserWindow.xib',
                'app/nibs/ClearBrowsingData.xib',
                'app/nibs/DownloadItem.xib',
                'app/nibs/DownloadShelf.xib',
                'app/nibs/EditSearchEngine.xib',
                'app/nibs/ExtensionShelf.xib',
                'app/nibs/FindBar.xib',
                'app/nibs/FirstRunDialog.xib',
                'app/nibs/HungRendererDialog.xib',
                'app/nibs/HttpAuthLoginSheet.xib',
                'app/nibs/ImportSettingsDialog.xib',
                'app/nibs/InfoBar.xib',
                'app/nibs/InfoBarContainer.xib',
                'app/nibs/ImportProgressDialog.xib',
                'app/nibs/KeywordEditor.xib',
                'app/nibs/MainMenu.xib',
                'app/nibs/PageInfo.xib',
                'app/nibs/Preferences.xib',
                'app/nibs/ReportBug.xib',
                'app/nibs/SaveAccessoryView.xib',
                'app/nibs/TabContents.xib',
                'app/nibs/TabView.xib',
                'app/nibs/TaskManager.xib',
                'app/nibs/Toolbar.xib',
                'app/theme/back_Template.pdf',
                'app/theme/chevron.png',  # TODO(jrg): get and use a pdf version
                'app/theme/close_bar.pdf',
                'app/theme/close_bar_h.pdf',
                'app/theme/close_bar_p.pdf',
                'app/theme/find_next_Template.pdf',
                'app/theme/find_prev_Template.pdf',
                'app/theme/forward_Template.pdf',
                'app/theme/go_Template.pdf',
                'app/theme/home_Template.pdf',
                'app/theme/menu_chrome_rtl_Template.pdf',
                'app/theme/menu_chrome_Template.pdf',
                'app/theme/menu_page_rtl_Template.pdf',
                'app/theme/menu_page_Template.pdf',
                'app/theme/nav.pdf',
                'app/theme/newtab.pdf',
                'app/theme/newtab_h.pdf',
                'app/theme/newtab_p.pdf',
                'app/theme/otr_icon.pdf',
                'app/theme/reload_Template.pdf',
                'app/theme/star_Template.pdf',
                'app/theme/starred.pdf',
                'app/theme/stop_Template.pdf',
              ],
              'mac_bundle_resources!': [
                'app/framework-Info.plist',
              ],
              'dependencies': [
                # Bring in pdfsqueeze and run it on all pdfs
                '../build/temp_gyp/pdfsqueeze.gyp:pdfsqueeze',
                '../build/util/support/support.gyp:*',
              ],
              'rules': [
                {
                  'rule_name': 'pdfsqueeze',
                  'extension': 'pdf',
                  'inputs': [
                    '<(PRODUCT_DIR)/pdfsqueeze',
                  ],
                  'outputs': [
                    '<(INTERMEDIATE_DIR)/pdfsqueeze/<(RULE_INPUT_ROOT).pdf',
                  ],
                  'action': ['<(PRODUCT_DIR)/pdfsqueeze',
                             '<(RULE_INPUT_PATH)', '<@(_outputs)'],
                  'message': 'Running pdfsqueeze on <(RULE_INPUT_PATH)',
                },
              ],
              'variables': {
                'repack_path': '../tools/data_pack/repack.py',
              },
              'actions': [
                # TODO(mark): These actions are duplicated for Linux and
                # FreeBSD in the chrome target.  Can they be unified?
                {
                  'action_name': 'repack_chrome',
                  'variables': {
                    'pak_inputs': [
                      '<(grit_out_dir)/browser_resources.pak',
                      '<(grit_out_dir)/common_resources.pak',
                      '<(grit_out_dir)/renderer_resources.pak',
                      '<(grit_out_dir)/theme_resources.pak',
                      '<(SHARED_INTERMEDIATE_DIR)/app/app_resources/app_resources.pak',
                      '<(SHARED_INTERMEDIATE_DIR)/net/net_resources.pak',
                      '<(SHARED_INTERMEDIATE_DIR)/webkit/webkit_resources.pak',
                    ],
                  },
                  'inputs': [
                    '<(repack_path)',
                    '<@(pak_inputs)',
                  ],
                  'outputs': [
                    '<(INTERMEDIATE_DIR)/repack/chrome.pak',
                  ],
                  'action': ['python', '<(repack_path)', '<@(_outputs)',
                             '<@(pak_inputs)'],
                  'process_outputs_as_mac_bundle_resources': 1,
                },
                {
                  'action_name': 'repack_locales',
                  'process_outputs_as_mac_bundle_resources': 1,
                  'variables': {
                    'conditions': [
                      ['branding=="Chrome"', {
                        'branding_flag': ['-b', 'google_chrome',],
                      }, {  # else: branding!="Chrome"
                        'branding_flag': ['-b', 'chromium',],
                      }],
                    ],
                  },
                  'inputs': [
                    'tools/build/repack_locales.py',
                    # NOTE: Ideally the common command args would be shared
                    # amongst inputs/outputs/action, but the args include shell
                    # variables which need to be passed intact, and command
                    # expansion wants to expand the shell variables. Adding the
                    # explicit quoting here was the only way it seemed to work.
                    '>!@(<(repack_locales_cmd) -i <(branding_flag) -g \'<(grit_out_dir)\' -s \'<(SHARED_INTERMEDIATE_DIR)\' -x \'<(INTERMEDIATE_DIR)\' <(locales))',
                  ],
                  'outputs': [
                    '>!@(<(repack_locales_cmd) -o -g \'<(grit_out_dir)\' -s \'<(SHARED_INTERMEDIATE_DIR)\' -x \'<(INTERMEDIATE_DIR)\' <(locales))',
                  ],
                  'action': [
                    '<@(repack_locales_cmd)',
                    '<@(branding_flag)',
                    '-g', '<(grit_out_dir)',
                    '-s', '<(SHARED_INTERMEDIATE_DIR)',
                    '-x', '<(INTERMEDIATE_DIR)',
                    '<@(locales)',
                  ],
                },
              ],
              'postbuilds': [
                {
                  # This step causes an error to be raised if the .order file
                  # does not account for all global text symbols.  It
                  # validates the completeness of the .order file.
                  'postbuild_name': 'Verify global text symbol order',
                  'variables': {
                    'verify_order_path': 'tools/build/mac/verify_order',
                  },
                  'action': [
                    '<(verify_order_path)',
                    '_ChromeMain',
                    '${BUILT_PRODUCTS_DIR}/${EXECUTABLE_PATH}',
                  ],
                },
                {
                  # Modify the Info.plist as needed.  The script explains why
                  # this is needed.  This is also done in the chrome target.
                  # The framework needs the Breakpad and Keystone keys if
                  # those features are enabled.  It doesn't currently use the
                  # Subversion keys for anything, but this seems like a really
                  # good place to store them.
                  'postbuild_name': 'Tweak Info.plist',
                  'action': ['<(tweak_info_plist_path)',
                             '-b<(mac_breakpad)',
                             '-k<(mac_keystone)',
                             '-s1',
                             '<(branding)',
                             '<(mac_bundle_id)'],
                },
                {
                  'postbuild_name': 'Symlink Libraries',
                  'action': [
                    'ln',
                    '-fhs',
                    'Versions/Current/Libraries',
                    '${BUILT_PRODUCTS_DIR}/${WRAPPER_NAME}/Libraries'
                  ],
                },
              ],
              'copies': [
                {
                  'destination': '<(PRODUCT_DIR)/$(CONTENTS_FOLDER_PATH)/Resources',
                  'files': [
                    '<(PRODUCT_DIR)/resources/inspector/'
                  ],
                  'conditions': [
                    ['mac_breakpad==1', {
                      'files': [
                        '<(PRODUCT_DIR)/crash_inspector',
                        '<(PRODUCT_DIR)/crash_report_sender.app'
                      ],
                    }],
                  ],
                },
                {
                  'destination':
                      '<(PRODUCT_DIR)/$(CONTENTS_FOLDER_PATH)/Libraries',
                  'files': [
                    # TODO(ajwong): Find a way to share this path with
                    # ffmpeg.gyp so they don't diverge. (BUG=23602)
                    '<(PRODUCT_DIR)/libffmpegsumo.dylib',
                  ],
                },
              ],
              'conditions': [
                ['mac_breakpad==1', {
                  'variables': {
                    # A real .dSYM is needed for dump_syms to operate on.
                    'mac_real_dsym': 1,
                  },
                  'sources': [
                    'app/breakpad_mac.mm',
                    'app/breakpad_mac.h',
                  ],
                  'dependencies': [
                    '../breakpad/breakpad.gyp:breakpad',
                  ],
                }, {  # else: mac_breakpad!=1
                  # No Breakpad, put in the stubs.
                  'sources': [
                    'app/breakpad_mac_stubs.mm',
                    'app/breakpad_mac.h',
                  ],
                }],  # mac_breakpad
                ['mac_keystone==1', {
                  'variables': {
                    'conditions': [
                      ['branding=="Chrome"', {
                        'theme_dir_name': 'google_chrome',
                      }, {  # else: 'branding!="Chrome"
                        'theme_dir_name': 'chromium',
                      }],
                    ],
                  },
                  'mac_bundle_resources': [
                    # This image is used to badge the lock icon in the
                    # promotion authentication dialog.  It needs to exist as
                    # a file on disk and not just something in a resource
                    # bundle because that's the interface that Authorization
                    # Services uses.  Also, Authorization Services can't deal
                    # with .icns files.
                    'app/theme/<(theme_dir_name)/product_logo_32.png',

                    'browser/cocoa/keystone_promote_preflight.sh',
                    'browser/cocoa/keystone_promote_postflight.sh',
                  ],
                  'postbuilds': [
                    {
                      'postbuild_name': 'Copy KeystoneRegistration.framework',
                      'action': [
                        'tools/build/mac/copy_framework_unversioned',
                        '../third_party/googlemac/Releases/Keystone/KeystoneRegistration.framework',
                        '${BUILT_PRODUCTS_DIR}/${CONTENTS_FOLDER_PATH}/Frameworks',
                      ],
                    },
                    {
                      'postbuild_name': 'Symlink Frameworks',
                      'action': [
                        'ln',
                        '-fhs',
                        'Versions/Current/Frameworks',
                        '${BUILT_PRODUCTS_DIR}/${WRAPPER_NAME}/Frameworks'
                      ],
                    },
                  ],
                }],  # mac_keystone
              ],  # conditions
            }],  # OS=="mac"
          ],  # conditions
        },  # target chrome_dll
      ],  # targets
    }],  # OS=="mac" or OS=="win"
    ['OS=="mac"',
      { 'targets': [
        {
          'target_name': 'helper_app',
          'type': 'executable',
          'product_name': '<(mac_product_name) Helper',
          'mac_bundle': 1,
          'dependencies': [
            'chrome_dll',
            'interpose_dependency_shim',
            'infoplist_strings_tool',
          ],
          'sources': [
            # chrome_exe_main.mm's main() is the entry point for the "chrome"
            # (browser app) target.  All it does is jump to chrome_dll's
            # ChromeMain.  This is appropriate for helper processes too,
            # because the logic to discriminate between process types at run
            # time is actually directed by the --type command line argument
            # processed by ChromeMain.  Sharing chrome_exe_main.mm with the
            # browser app will suffice for now.
            'app/chrome_exe_main.mm',
            'app/helper-Info.plist',
          ],
          # TODO(mark): Come up with a fancier way to do this.  It should only
          # be necessary to list helper-Info.plist once, not the three times it
          # is listed here.
          'mac_bundle_resources!': [
            'app/helper-Info.plist',
          ],
          # TODO(mark): For now, don't put any resources into this app.  Its
          # resources directory will be a symbolic link to the browser app's
          # resources directory.
          'mac_bundle_resources/': [
            ['exclude', '.*'],
          ],
          'xcode_settings': {
            'CHROMIUM_BUNDLE_ID': '<(mac_bundle_id)',
            'CHROMIUM_SHORT_NAME': '<(branding)',
            'INFOPLIST_FILE': 'app/helper-Info.plist',
          },
          'copies': [
            {
              'destination': '<(PRODUCT_DIR)/<(mac_product_name) Helper.app/Contents/MacOS',
              'files': [
                '<(PRODUCT_DIR)/libplugin_carbon_interpose.dylib',
              ],
            },
          ],
          'actions': [
            {
              # Generate the InfoPlist.strings file
              'action_name': 'Generating InfoPlist.strings files',
              'variables': {
                'tool_path': '<(PRODUCT_DIR)/infoplist_strings_tool',
                # Unique dir to write to so the [lang].lproj/InfoPlist.strings
                # for the main app and the helper app don't name collide.
                'output_path': '<(INTERMEDIATE_DIR)/helper_infoplist_strings',
              },
              'conditions': [
                [ 'branding == "Chrome"', {
                  'variables': {
                     'branding_name': 'google_chrome_strings',
                  },
                }, { # else branding!="Chrome"
                  'variables': {
                     'branding_name': 'chromium_strings',
                  },
                }],
              ],
              'inputs': [
                '<(tool_path)',
                '<(version_path)',
                # TODO: remove this helper when we have loops in GYP
                '>!@(<(apply_locales_cmd) \'<(grit_out_dir)/<(branding_name)_ZZLOCALE.pak\' <(locales))',
              ],
              'outputs': [
                # TODO: remove this helper when we have loops in GYP
                '>!@(<(apply_locales_cmd) -d \'<(output_path)/ZZLOCALE.lproj/InfoPlist.strings\' <(locales))',
              ],
              'action': [
                '<(tool_path)',
                '-b', '<(branding_name)',
                '-v', '<(version_path)',
                '-g', '<(grit_out_dir)',
                '-o', '<(output_path)',
                '-t', 'helper',
                '<@(locales)',
              ],
              'message': 'Generating the language InfoPlist.strings files',
              'process_outputs_as_mac_bundle_resources': 1,
            },
          ],
          'postbuilds': [
            {
              # The framework (chrome_dll) defines its load-time path
              # (DYLIB_INSTALL_NAME_BASE) relative to the main executable
              # (chrome).  A different relative path needs to be used in
              # helper_app.
              'postbuild_name': 'Fix Framework Link',
              'action': [
                'install_name_tool',
                '-change',
                '@executable_path/../Versions/<(version_full)/<(mac_product_name) Framework.framework/<(mac_product_name) Framework',
                '@executable_path/../../../<(mac_product_name) Framework.framework/<(mac_product_name) Framework',
                '${BUILT_PRODUCTS_DIR}/${EXECUTABLE_PATH}'
              ],
            },
            {
              # Modify the Info.plist as needed.  The script explains why this
              # is needed.  This is also done in the chrome and chrome_dll
              # targets.  In this case, -b0 and -k0 are used because Breakpad
              # and Keystone keys are never placed into the helper, only into
              # the framework.  -s0 is used because Subversion keys are only
              # placed into the main app.
              'postbuild_name': 'Tweak Info.plist',
              'action': ['<(tweak_info_plist_path)',
                         '-b0',
                         '-k0',
                         '-s0',
                         '<(branding)',
                         '<(mac_bundle_id)'],
            },
          ],
          'conditions': [
            ['mac_breakpad==1', {
              'variables': {
                # A real .dSYM is needed for dump_syms to operate on.
                'mac_real_dsym': 1,
              },
            }],
          ],
        },  # target helper_app
        {
          # Convenience target to build a disk image.
          'target_name': 'build_app_dmg',
          # Don't place this in the 'all' list; most won't want it.
          # In GYP, booleans are 0/1, not True/False.
          'suppress_wildcard': 1,
          'type': 'none',
          'dependencies': [
            'chrome',
          ],
          'variables': {
            'build_app_dmg_script_path': 'tools/build/mac/build_app_dmg',
          },
          'actions': [
            {
              'inputs': [
                '<(build_app_dmg_script_path)',
                '<(PRODUCT_DIR)/<(branding).app',
              ],
              'outputs': [
                '<(PRODUCT_DIR)/<(branding).dmg',
              ],
              'action_name': 'build_app_dmg',
              'action': ['<(build_app_dmg_script_path)', '<@(branding)'],
            },
          ],  # 'actions'
        },
        {
          # Dummy target to allow chrome to require plugin_carbon_interpose to
          # build without actually linking to the resulting library.
          'target_name': 'interpose_dependency_shim',
          'type': 'executable',
          'dependencies': [
            'plugin_carbon_interpose',
          ],
          # In release, we end up with a strip step that is unhappy if there is
          # no binary. Rather than check in a new file for this temporary hack,
          # just generate a source file on the fly.
          'actions': [
            {
              'action_name': 'generate_stub_main',
              'process_outputs_as_sources': 1,
              'inputs': [],
              'outputs': [ '<(INTERMEDIATE_DIR)/dummy_main.c' ],
              'action': [
                'bash', '-c',
                'echo "int main() { return 0; }" > <(INTERMEDIATE_DIR)/dummy_main.c'
              ],
            },
          ],
        },
        {
          # dylib for interposing Carbon calls in the plugin process.
          'target_name': 'plugin_carbon_interpose',
          'type': 'shared_library',
          'dependencies': [
            'chrome_dll',
          ],
          'sources': [
            'browser/plugin_carbon_interpose_mac.cc',
          ],
          'include_dirs': [
            '..',
          ],
          'link_settings': {
            'libraries': [
              '$(SDKROOT)/System/Library/Frameworks/Carbon.framework',
            ],
          },
          'xcode_settings': {
            'DYLIB_COMPATIBILITY_VERSION': '<(version_build_patch)',
            'DYLIB_CURRENT_VERSION': '<(version_build_patch)',
            'DYLIB_INSTALL_NAME_BASE': '@executable_path',
          },
          'postbuilds': [
            {
              # The framework (chrome_dll) defines its load-time path
              # (DYLIB_INSTALL_NAME_BASE) relative to the main executable
              # (chrome).  A different relative path needs to be used in
              # plugin_carbon_interpose, which runs in the helper_app.
              'postbuild_name': 'Fix Framework Link',
              'action': [
                'install_name_tool',
                '-change',
                '@executable_path/../Versions/<(version_full)/<(mac_product_name) Framework.framework/<(mac_product_name) Framework',
                '@executable_path/../../../<(mac_product_name) Framework.framework/<(mac_product_name) Framework',
                '${BUILT_PRODUCTS_DIR}/${EXECUTABLE_PATH}'
              ],
            },
          ],
        },
        {
          'target_name': 'infoplist_strings_tool',
          'type': 'executable',
          'dependencies': [
            'chrome_strings',
            '../base/base.gyp:base',
          ],
          'include_dirs': [
            '<(grit_out_dir)',
          ],
          'sources': [
            'tools/mac_helpers/infoplist_strings_util.mm',
          ],
        },
      ],  # targets
    }, { # else: OS != "mac"
      'targets': [
        {
          'target_name': 'convert_dict',
          'type': 'executable',
          'msvs_guid': '42ECD5EC-722F-41DE-B6B8-83764C8016DF',
          'dependencies': [
            '../base/base.gyp:base',
            '../base/base.gyp:base_i18n',
            'convert_dict_lib',
            '../third_party/hunspell/hunspell.gyp:hunspell',
          ],
          'sources': [
            'tools/convert_dict/convert_dict.cc',
          ],
        },
        {
          'target_name': 'convert_dict_lib',
          'product_name': 'convert_dict',
          'type': 'static_library',
          'msvs_guid': '1F669F6B-3F4A-4308-E496-EE480BDF0B89',
          'include_dirs': [
            '..',
          ],
          'sources': [
            'tools/convert_dict/aff_reader.cc',
            'tools/convert_dict/aff_reader.h',
            'tools/convert_dict/dic_reader.cc',
            'tools/convert_dict/dic_reader.h',
            'tools/convert_dict/hunspell_reader.cc',
            'tools/convert_dict/hunspell_reader.h',
          ],
        },
        {
          'target_name': 'flush_cache',
          'type': 'executable',
          'msvs_guid': '4539AFB3-B8DC-47F3-A491-6DAC8FD26657',
          'dependencies': [
            '../base/base.gyp:base',
            '../base/base.gyp:test_support_base',
          ],
          'sources': [
            'tools/perf/flush_cache/flush_cache.cc',
          ],
        },
        {
          'target_name': 'perf_tests',
          'type': 'executable',
          'msvs_guid': '9055E088-25C6-47FD-87D5-D9DD9FD75C9F',
          'dependencies': [
            'browser',
            'common',
            'debugger',
            'renderer',
            'syncapi',
            'chrome_resources',
            'chrome_strings',
            '../app/app.gyp:app_base',
            '../base/base.gyp:base',
            '../base/base.gyp:test_support_base',
            '../base/base.gyp:test_support_perf',
            '../skia/skia.gyp:skia',
            '../testing/gtest.gyp:gtest',
            '../webkit/webkit.gyp:glue',
          ],
          'sources': [
            'browser/safe_browsing/filter_false_positive_perftest.cc',
            'browser/visitedlink_perftest.cc',
            'common/json_value_serializer_perftest.cc',
            'test/perf/perftests.cc',
            'test/perf/url_parse_perftest.cc',
          ],
          'conditions': [
            ['OS=="linux"', {
              'dependencies': [
                '../build/linux/system.gyp:gtk',
                '../tools/xdisplaycheck/xdisplaycheck.gyp:xdisplaycheck',
              ],
              'sources!': [
                # TODO(port):
                'browser/safe_browsing/filter_false_positive_perftest.cc',
                'browser/visitedlink_perftest.cc',
              ],
            }],
            ['OS=="win" or (OS=="linux" and toolkit_views==1)', {
              'dependencies': [
                '../views/views.gyp:views',
              ],
            }],
            ['OS=="win"', {
              'configurations': {
                'Debug': {
                  'msvs_settings': {
                    'VCLinkerTool': {
                      'LinkIncremental': '<(msvs_large_module_debug_link_mode)',
                    },
                  },
                },
              },
              'dependencies': [
                '<(allocator_target)',
              ],
            }],
          ],
        },
        # TODO(port): enable on mac.
        {
          'includes': ['test/interactive_ui/interactive_ui_tests.gypi']
        },
      ],
    },],  # OS!="mac"
    ['OS=="linux"',
      { 'targets': [
        {
          'target_name': 'linux_symbols',
          'type': 'none',
          'conditions': [
            ['linux_dump_symbols==1', {
              'actions': [
                {
                  'action_name': 'dump_symbols',
                  'inputs': [
                    '<(DEPTH)/build/linux/dump_app_syms',
                    '<(DEPTH)/build/linux/dump_signature.py',
                    '<(PRODUCT_DIR)/dump_syms',
                    '<(PRODUCT_DIR)/chrome',
                  ],
                  'outputs': [
                    '<(PRODUCT_DIR)/chrome.breakpad.<(target_arch)',
                  ],
                  'action': ['<(DEPTH)/build/linux/dump_app_syms',
                             '<(PRODUCT_DIR)/dump_syms',
                             '<(linux_strip_binary)',
                             '<(PRODUCT_DIR)/chrome',
                             '<@(_outputs)'],
                  'message': 'Dumping breakpad symbols to <(_outputs)',
                  'process_outputs_as_sources': 1,
                },
              ],
              'dependencies': [
                'chrome',
                '../breakpad/breakpad.gyp:dump_syms',
              ],
            }],
          ],
        }
      ],
    },],  # OS=="linux"
    ['OS!="win"',
      { 'targets': [
      ],  # targets
    }],  # OS!="win"
    ['OS=="win"',
      { 'targets': [
        {
          # TODO(sgk):  remove this when we change the buildbots to
          # use the generated build\all.sln file to build the world.
          'target_name': 'pull_in_all',
          'type': 'none',
          'dependencies': [
            'installer/mini_installer.gyp:*',
            'installer/installer.gyp:*',
            '../app/app.gyp:*',
            '../base/allocator/allocator.gyp:*',
            '../base/base.gyp:*',
            '../ipc/ipc.gyp:*',
            '../media/media.gyp:*',
            '../net/net.gyp:*',
            '../printing/printing.gyp:*',
            '../rlz/rlz.gyp:*',
            '../sdch/sdch.gyp:*',
            '../skia/skia.gyp:*',
            '../testing/gmock.gyp:*',
            '../testing/gtest.gyp:*',
            '../third_party/bsdiff/bsdiff.gyp:*',
            '../third_party/bspatch/bspatch.gyp:*',
            '../third_party/bzip2/bzip2.gyp:*',
            '../third_party/cld/cld.gyp:cld',
            '../third_party/codesighs/codesighs.gyp:*',
            '../third_party/icu/icu.gyp:*',
            '../third_party/libjpeg/libjpeg.gyp:*',
            '../third_party/libpng/libpng.gyp:*',
            '../third_party/libxml/libxml.gyp:*',
            '../third_party/libxslt/libxslt.gyp:*',
            '../third_party/lzma_sdk/lzma_sdk.gyp:*',
            '../third_party/modp_b64/modp_b64.gyp:*',
            '../third_party/npapi/npapi.gyp:*',
            '../third_party/sqlite/sqlite.gyp:*',
            '../third_party/zlib/zlib.gyp:*',
            '../webkit/tools/test_shell/test_shell.gyp:*',
            '../webkit/webkit.gyp:*',

            '../build/temp_gyp/googleurl.gyp:*',

            '../breakpad/breakpad.gyp:*',
            '../courgette/courgette.gyp:*',
            '../gears/gears.gyp:*',
            '../rlz/rlz.gyp:*',
            '../sandbox/sandbox.gyp:*',
            '../tools/memory_watcher/memory_watcher.gyp:*',
            '../v8/tools/gyp/v8.gyp:v8_shell',
          ],
          'conditions': [
            ['chrome_frame_define==1', {
              'dependencies': [
                '../chrome_frame/chrome_frame.gyp:*',
              ],
            }],
          ],
        },
        {
          'target_name': 'chrome_dll_version',
          'type': 'none',
          #'msvs_guid': '414D4D24-5D65-498B-A33F-3A29AD3CDEDC',
          'dependencies': [
            '../build/util/build_util.gyp:lastchange',
          ],
          'direct_dependent_settings': {
            'include_dirs': [
              '<(SHARED_INTERMEDIATE_DIR)/chrome_dll_version',
            ],
          },
          'actions': [
            {
              'action_name': 'version',
              'variables': {
                'lastchange_path':
                  '<(SHARED_INTERMEDIATE_DIR)/build/LASTCHANGE',
                'template_input_path': 'app/chrome_dll_version.rc.version',
              },
              'conditions': [
                [ 'branding == "Chrome"', {
                  'variables': {
                     'branding_path': 'app/theme/google_chrome/BRANDING',
                  },
                }, { # else branding!="Chrome"
                  'variables': {
                     'branding_path': 'app/theme/chromium/BRANDING',
                  },
                }],
              ],
              'inputs': [
                '<(template_input_path)',
                '<(version_path)',
                '<(branding_path)',
                '<(lastchange_path)',
              ],
              'outputs': [
                '<(SHARED_INTERMEDIATE_DIR)/chrome_dll_version/chrome_dll_version.rc',
              ],
              'action': [
                'python',
                '<(version_py_path)',
                '-f', '<(version_path)',
                '-f', '<(branding_path)',
                '-f', '<(lastchange_path)',
                '<(template_input_path)',
                '<@(_outputs)',
              ],
              'message': 'Generating version information in <(_outputs)'
            },
          ],
        },
        {
          'target_name': 'automation',
          'type': '<(library)',
          'msvs_guid': '1556EF78-C7E6-43C8-951F-F6B43AC0DD12',
          'dependencies': [
            'theme_resources',
            '../skia/skia.gyp:skia',
          ],
          'include_dirs': [
            '..',
          ],
          'sources': [
             'test/automation/autocomplete_edit_proxy.cc',
             'test/automation/autocomplete_edit_proxy.h',
             'test/automation/automation_constants.h',
             'test/automation/automation_handle_tracker.cc',
             'test/automation/automation_handle_tracker.h',
             'test/automation/automation_messages.h',
             'test/automation/automation_messages_internal.h',
             'test/automation/automation_proxy.cc',
             'test/automation/automation_proxy.h',
             'test/automation/browser_proxy.cc',
             'test/automation/browser_proxy.h',
             'test/automation/tab_proxy.cc',
             'test/automation/tab_proxy.h',
             'test/automation/window_proxy.cc',
             'test/automation/window_proxy.h',
          ],
        },
        {
          # Windows-only for now; this has issues with scons
          # regarding use of run_all_unittests.cc.
          # TODO(zork): add target to linux build.
          'target_name': 'sync_integration_tests',
          'type': 'executable',
            'dependencies': [
              'browser',
              'chrome',
              'chrome_resources',
              'common',
              'debugger',
              'renderer',
              'chrome_resources',
              'chrome_strings',
              'syncapi',
              'test_support_unit',
              '../printing/printing.gyp:printing',
              '../skia/skia.gyp:skia',
              '../testing/gtest.gyp:gtest',
              '../third_party/icu/icu.gyp:icui18n',
              '../third_party/icu/icu.gyp:icuuc',
              '../third_party/libxml/libxml.gyp:libxml',
              '../third_party/npapi/npapi.gyp:npapi',
              '../third_party/WebKit/WebKit/chromium/WebKit.gyp:webkit',
            ],
            'include_dirs': [
              '..',
              '<(INTERMEDIATE_DIR)',
            ],
            # TODO(phajdan.jr): Only temporary, to make transition easier.
            'defines': [ 'ALLOW_IN_PROC_BROWSER_TEST' ],
            'sources': [
              'app/chrome_dll.rc',
              'app/chrome_dll_resource.h',
              'app/chrome_dll_version.rc.version',
              'test/in_process_browser_test.cc',
              'test/in_process_browser_test.h',
              'test/live_sync/bookmark_model_verifier.cc',
              'test/live_sync/bookmark_model_verifier.h',
              'test/live_sync/live_bookmarks_sync_test.cc',
              'test/live_sync/live_bookmarks_sync_test.h',
              'test/live_sync/profile_sync_service_test_harness.cc',
              'test/live_sync/profile_sync_service_test_harness.h',
              'test/live_sync/single_client_live_bookmarks_sync_unittest.cc',
              'test/live_sync/two_client_live_bookmarks_sync_test.cc',
              'test/test_launcher/run_all_unittests.cc',
              'test/test_notification_tracker.cc',
              'test/test_notification_tracker.h',
              'test/testing_browser_process.h',
              'test/ui_test_utils_linux.cc',
              'test/ui_test_utils_mac.cc',
              'test/ui_test_utils_win.cc',
              'test/data/resource.h',
              'test/data/resource.rc',
              '<(SHARED_INTERMEDIATE_DIR)/app/app_resources/app_resources.rc',
              '<(SHARED_INTERMEDIATE_DIR)/chrome/browser_resources.rc',
              '<(SHARED_INTERMEDIATE_DIR)/chrome_dll_version/chrome_dll_version.rc',
              '<(SHARED_INTERMEDIATE_DIR)/chrome/common_resources.rc',
              '<(SHARED_INTERMEDIATE_DIR)/chrome/theme_resources.rc',
            ],
            'conditions': [
              # Plugin code.
              ['OS=="linux" or OS=="win"', {
                'dependencies': [
                  'plugin',
                 ],
                'export_dependent_settings': [
                  'plugin',
                ],
              }],
              # Linux-specific rules.
              ['OS=="linux"', {
                 'dependencies': [
                   '../build/linux/system.gyp:gtk',
                 ],
              }],
              # Windows-specific rules.
              ['OS=="win"', {
                'include_dirs': [
                  'third_party/wtl/include',
                 ],
                'dependencies': [
                  'chrome_dll_version',
                  'installer/installer.gyp:installer_util_strings',
                  '../views/views.gyp:views',
                  '<(allocator_target)',
                ],
                'configurations': {
                  'Debug': {
                    'msvs_settings': {
                      'VCLinkerTool': {
                        'LinkIncremental': '<(msvs_large_module_debug_link_mode)',
                      },
                    },
                  },
                },
              }],
            ],
        },
        {
          'target_name': 'crash_service',
          'type': 'executable',
          'msvs_guid': '89C1C190-A5D1-4EC4-BD6A-67FF2195C7CC',
          'dependencies': [
            'common_constants',
            '../base/base.gyp:base',
            '../breakpad/breakpad.gyp:breakpad_handler',
            '../breakpad/breakpad.gyp:breakpad_sender',
          ],
          'include_dirs': [
            '..',
          ],
          'sources': [
            'tools/crash_service/crash_service.cc',
            'tools/crash_service/crash_service.h',
            'tools/crash_service/main.cc',
          ],
          'msvs_settings': {
            'VCLinkerTool': {
              'SubSystem': '2',         # Set /SUBSYSTEM:WINDOWS
            },
          },
        },
        {
          'target_name': 'generate_profile',
          'type': 'executable',
          'msvs_guid': '2E969AE9-7B12-4EDB-8E8B-48C7AE7BE357',
          'dependencies': [
            'browser',
            'debugger',
            'renderer',
            'syncapi',
            '../base/base.gyp:base',
            '../skia/skia.gyp:skia',
          ],
          'include_dirs': [
            '..',
          ],
          'sources': [
            'tools/profiles/generate_profile.cc',
            'tools/profiles/thumbnail-inl.h',
          ],
          'conditions': [
            ['OS=="win"', {
              'dependencies': [
                '<(allocator_target)',
              ],
              'configurations': {
                'Debug': {
                  'msvs_settings': {
                    'VCLinkerTool': {
                      'LinkIncremental': '<(msvs_large_module_debug_link_mode)',
                    },
                  },
                },
              },
            }],
          ],
        },
        {
          'target_name': 'plugin_tests',
          'type': 'executable',
          'msvs_guid': 'A1CAA831-C507-4B2E-87F3-AEC63C9907F9',
          'dependencies': [
            'chrome_resources',
            'chrome_strings',
            'security_tests',  # run time dependency
            'test_support_common',
            'test_support_ui',
            '../skia/skia.gyp:skia',
            '../testing/gtest.gyp:gtest',
            '../third_party/libxml/libxml.gyp:libxml',
            '../third_party/libxslt/libxslt.gyp:libxslt',
            '../third_party/npapi/npapi.gyp:npapi',
          ],
          'include_dirs': [
            '..',
            'third_party/wtl/include',
          ],
          'sources': [
            'test/plugin/plugin_test.cpp',
          ],
          'conditions': [
            ['OS=="win"', {
              'dependencies': [
                '<(allocator_target)',
              ],
            },],
          ],
        },
        {
          'target_name': 'security_tests',
          'type': 'shared_library',
          'msvs_guid': 'E750512D-FC7C-4C98-BF04-0A0DAF882055',
          'include_dirs': [
            '..',
          ],
          'sources': [
            'test/injection_test_dll.h',
            'test/security_tests/ipc_security_tests.cc',
            'test/security_tests/ipc_security_tests.h',
            'test/security_tests/security_tests.cc',
            '../sandbox/tests/validation_tests/commands.cc',
            '../sandbox/tests/validation_tests/commands.h',
          ],
        },
        {
          'target_name': 'selenium_tests',
          'type': 'executable',
          'msvs_guid': 'E3749617-BA3D-4230-B54C-B758E56D9FA5',
          'dependencies': [
            'chrome_resources',
            'chrome_strings',
            'test_support_common',
            'test_support_ui',
            '../skia/skia.gyp:skia',
            '../testing/gtest.gyp:gtest',
          ],
          'include_dirs': [
            '..',
            'third_party/wtl/include',
          ],
          'sources': [
            'test/selenium/selenium_test.cc',
          ],
          'conditions': [
            ['OS=="win"', {
              'dependencies': [
                '<(allocator_target)',
              ],
            },],
          ],
        },
        {
          'target_name': 'test_chrome_plugin',
          'type': 'shared_library',
          'msvs_guid': '7F0A70F6-BE3F-4C19-B435-956AB8F30BA4',
          'dependencies': [
            '../base/base.gyp:base',
            '../build/temp_gyp/googleurl.gyp:googleurl',
          ],
          'include_dirs': [
            '..',
          ],
          'link_settings': {
            'libraries': [
              '-lwinmm.lib',
            ],
          },
          'sources': [
            'test/chrome_plugin/test_chrome_plugin.cc',
            'test/chrome_plugin/test_chrome_plugin.def',
            'test/chrome_plugin/test_chrome_plugin.h',
          ],
        },
      ]},  # 'targets'
    ],  # OS=="win"
    # TODO(jrg): add in Windows code coverage targets.
    ['coverage!=0',
      { 'targets': [
        {
          'target_name': 'coverage',
          # do NOT place this in the 'all' list; most won't want it.
          # In gyp, booleans are 0/1 not True/False.
          'suppress_wildcard': 1,
          'type': 'none',
          # If you add new tests here you may need to update the croc configs.
          # E.g. build/{linux|mac}/chrome_linux.croc
          'dependencies': [
            'automated_ui_tests',
            '../app/app.gyp:app_unittests',
            '../base/base.gyp:base_unittests',
            '../ipc/ipc.gyp:ipc_tests',
            '../media/media.gyp:media_unittests',
            '../net/net.gyp:net_unittests',
            '../printing/printing.gyp:printing_unittests',
            'ui_tests',
            'unit_tests',
          ],
          'actions': [
            {
              # 'message' for Linux/scons in particular.  Scons
              # requires the 'coverage' target be run from within
              # src/chrome.
              'message': 'Running coverage_posix.py to generate coverage numbers',
              # MSVS must have an input file and an output file.
              'inputs': [ '../tools/code_coverage/coverage_posix.py' ],
              'outputs': [ '<(PRODUCT_DIR)/coverage.info' ],
              'action_name': 'coverage',
              'action': [ 'python',
                          '../tools/code_coverage/coverage_posix.py',
                          '--directory',
                          '<(PRODUCT_DIR)',
                          '--src_root',
                          '..',
                          '--',
                          '<@(_dependencies)'],
              # Use outputs of this action as inputs for the main target build.
              # Seems as a misnomer but makes this happy on Linux (scons).
              'process_outputs_as_sources': 1,
            },
          ],  # 'actions'
        },
      ]
    }],
  ],  # 'conditions'
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
