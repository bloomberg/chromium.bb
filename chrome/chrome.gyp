# Copyright (c) 2011 The Chromium Authors. All rights reserved.
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
    'version_mac_dylib':
        '<!(python <(version_py_path) -f <(version_path) -t "@BUILD@.@PATCH_HI@.@PATCH_LO@" -e "PATCH_HI=int(PATCH)/256" -e "PATCH_LO=int(PATCH)%256")',

    # Define the common dependencies that contain all the actual
    # Chromium functionality.  This list gets pulled in below by
    # the link of the actual chrome (or chromium) executable on
    # Linux or Mac, and into chrome.dll on Windows.
    'chromium_dependencies': [
      'common',
      'browser',
      'debugger',
      'profile_import',
      'renderer',
      'syncapi',
      'utility',
      'service',
      '../content/content.gyp:content_gpu',
      '../content/content.gyp:content_ppapi_plugin',
      '../content/content.gyp:content_worker',
      '../printing/printing.gyp:printing',
      '../third_party/WebKit/Source/WebKit/chromium/WebKit.gyp:inspector_resources',
    ],
    'nacl_win64_dependencies': [
      'common_nacl_win64',
      'common_constants_win64',
      'installer_util_nacl_win64',
    ],
    'allocator_target': '../base/allocator/allocator.gyp:allocator',
    'grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)/chrome',
    'protoc_out_dir': '<(SHARED_INTERMEDIATE_DIR)/protoc_out',
    'repack_locales_cmd': ['python', 'tools/build/repack_locales.py'],
    # TODO: remove this helper when we have loops in GYP
    'apply_locales_cmd': ['python', '<(DEPTH)/build/apply_locales.py'],
    'conditions': [
      ['OS=="win"', {
        'nacl_defines': [
          'NACL_WINDOWS=1',
          'NACL_LINUX=0',
          'NACL_OSX=0',
        ],
        'platform_locale_settings_grd':
            'app/resources/locale_settings_win.grd',
      },],
      ['OS=="linux"', {
        'nacl_defines': [
          'NACL_WINDOWS=0',
          'NACL_LINUX=1',
          'NACL_OSX=0',
        ],
        'conditions': [
          ['chromeos==1', {
            'platform_locale_settings_grd':
                'app/resources/locale_settings_cros.grd',
          }],
          ['chromeos!=1', {
            'platform_locale_settings_grd':
                'app/resources/locale_settings_linux.grd',
          }],
        ],
      },],
      ['os_posix == 1 and OS != "mac" and OS != "linux"', {
        'platform_locale_settings_grd':
            'app/resources/locale_settings_linux.grd',
      },],
      ['OS=="mac"', {
        'tweak_info_plist_path': 'tools/build/mac/tweak_info_plist',
        'nacl_defines': [
          'NACL_WINDOWS=0',
          'NACL_LINUX=0',
          'NACL_OSX=1',
        ],
        'platform_locale_settings_grd':
            'app/resources/locale_settings_mac.grd',
        'conditions': [
          ['branding=="Chrome"', {
            'mac_bundle_id': 'com.google.Chrome',
            'mac_creator': 'rimZ',
            # The policy .grd file also needs the bundle id.
            'grit_defines': ['-D', 'mac_bundle_id=com.google.Chrome'],
          }, {  # else: branding!="Chrome"
            'mac_bundle_id': 'org.chromium.Chromium',
            'mac_creator': 'Cr24',
            # The policy .grd file also needs the bundle id.
            'grit_defines': ['-D', 'mac_bundle_id=org.chromium.Chromium'],
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
    # Note on Win64 targets: targets that end with win64 be used
    # on 64-bit Windows only. Targets that end with nacl_win64 should be used
    # by Native Client only.
    'app/policy/policy_templates.gypi',
    'chrome_browser.gypi',
    'chrome_common.gypi',
    'chrome_dll.gypi',
    'chrome_exe.gypi',
    'chrome_installer.gypi',
    'chrome_installer_util.gypi',
    'chrome_renderer.gypi',
    'chrome_tests.gypi',
    'common_constants.gypi',
    'nacl.gypi',
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
      'actions': [
        # Data resources.
        {
          'action_name': 'autofill_resources',
          'variables': {
            'grit_grd_file': 'browser/autofill/autofill_resources.grd',
          },
          'includes': [ '../build/grit_action.gypi' ],
        },
        {
          'action_name': 'browser_resources',
          'variables': {
            'grit_grd_file': 'browser/browser_resources.grd',
          },
          'includes': [ '../build/grit_action.gypi' ],
        },
        {
          'action_name': 'common_resources',
          'variables': {
            'grit_grd_file': 'common/common_resources.grd',
          },
          'includes': [ '../build/grit_action.gypi' ],
        },
        {
          'action_name': 'renderer_resources',
          'variables': {
            'grit_grd_file': 'renderer/renderer_resources.grd',
          },
          'includes': [ '../build/grit_action.gypi' ],
        },
      ],
      'includes': [ '../build/grit_target.gypi' ],
    },
    {
      # TODO(mark): It would be better if each static library that needed
      # to run grit would list its own .grd files, but unfortunately some
      # of the static libraries currently have circular dependencies among
      # generated headers.
      'target_name': 'chrome_strings',
      'msvs_guid': 'D9DDAF60-663F-49CC-90DC-3D08CC3D1B28',
      'type': 'none',
      'conditions': [
        ['OS=="win"', {
          # HACK(nsylvain): We want to enforce a fake dependency on
          # intaller_util_string.  install_util depends on both
          # chrome_strings and installer_util_strings, but for some reasons
          # Incredibuild does not enforce it (most likely a bug). By changing
          # the type and making sure we depend on installer_util_strings, it
          # will always get built before installer_util.
          'type': 'dummy_executable',
          'dependencies': ['installer_util_strings'],
        }],
      ],
      'actions': [
        # Localizable resources.
        {
          'action_name': 'locale_settings',
          'variables': {
            'grit_grd_file': 'app/resources/locale_settings.grd',
          },
          'includes': [ '../build/grit_action.gypi' ],
        },
        {
          'action_name': 'chromium_strings.grd',
          'variables': {
            'grit_grd_file': 'app/chromium_strings.grd',
          },
          'includes': [ '../build/grit_action.gypi' ],
        },
        {
          'action_name': 'generated_resources',
          'variables': {
            'grit_grd_file': 'app/generated_resources.grd',
          },
          'includes': [ '../build/grit_action.gypi' ],
        },
        {
          'action_name': 'google_chrome_strings',
          'variables': {
            'grit_grd_file': 'app/google_chrome_strings.grd',
          },
          'includes': [ '../build/grit_action.gypi' ],
        },
      ],
      'includes': [ '../build/grit_target.gypi' ],
    },
    {
      'target_name': 'theme_resources',
      'type': 'none',
      'msvs_guid' : 'A158FB0A-25E4-6523-6B5A-4BB294B73D31',
      'actions': [
        {
          'action_name': 'theme_resources',
          'variables': {
            'grit_grd_file': 'app/theme/theme_resources.grd',
          },
          'includes': [ '../build/grit_action.gypi' ],
        },
      ],
      'includes': [ '../build/grit_target.gypi' ],
    },
    {
      'target_name': 'theme_resources_large',
      'type': 'none',
      'actions': [
        {
          'action_name': 'theme_resources_large',
          'variables': {
            'grit_grd_file': 'app/theme/theme_resources_large.grd',
          },
          'includes': [ '../build/grit_action.gypi' ],
        },
      ],
      'includes': [ '../build/grit_target.gypi' ],
    },
    {
      'target_name': 'theme_resources_standard',
      'type': 'none',
      'actions': [
        {
          'action_name': 'theme_resources_standard',
          'variables': {
            'grit_grd_file': 'app/theme/theme_resources_standard.grd',
          },
          'includes': [ '../build/grit_action.gypi' ],
        },
      ],
      'includes': [ '../build/grit_target.gypi' ],
    },
    {
      'target_name': 'platform_locale_settings',
      'type': 'none',
      'actions': [
        {
          'action_name': 'platform_locale_settings',
          'variables': {
            'grit_grd_file': '<(platform_locale_settings_grd)',
          },
          'includes': [ '../build/grit_action.gypi' ],
        },
      ],
      'includes': [ '../build/grit_target.gypi' ],
    },
    {
      'target_name': 'chrome_extra_resources',
      'type': 'none',
      'dependencies': [
        '../third_party/WebKit/Source/WebKit/chromium/WebKit.gyp:generate_devtools_grd',
      ],
      # These resources end up in resources.pak because they are resources
      # used by internal pages.  Putting them in a spearate pak file makes
      # it easier for us to reference them internally.
      'actions': [
        {
          'action_name': 'component_extension_resources',
          'variables': {
            'grit_grd_file': 'browser/resources/component_extension_resources.grd',
          },
          'includes': [ '../build/grit_action.gypi' ],
        },
        {
          'action_name': 'net_internals_resources',
          'variables': {
            'grit_grd_file': 'browser/resources/net_internals_resources.grd',
          },
          'includes': [ '../build/grit_action.gypi' ],
        },
        {
          'action_name': 'options_resources',
          'variables': {
            'grit_grd_file': 'browser/resources/options_resources.grd',
          },
          'includes': [ '../build/grit_action.gypi' ],
        },
        {
          'action_name': 'shared_resources',
          'variables': {
            'grit_grd_file': 'browser/resources/shared_resources.grd',
          },
          'includes': [ '../build/grit_action.gypi' ],
        },
        {
          'action_name': 'sync_internals_resources',
          'variables': {
            'grit_grd_file': 'browser/resources/sync_internals_resources.grd',
          },
          'includes': [ '../build/grit_action.gypi' ],
        },
        {
          'action_name': 'devtools_resources',
          # This can't use ../build/grit_action.gypi because the grd file
          # is generated a build time, so the trick of using grit_info to get
          # the real inputs/outputs at GYP time isn't possible.
          'variables': {
            'grit_cmd': ['python', '../tools/grit/grit.py'],
            'grit_grd_file': '<(SHARED_INTERMEDIATE_DIR)/devtools/devtools_resources.grd',
          },
          'inputs': [
            '<(grit_grd_file)',
          ],
          'outputs': [
            '<(grit_out_dir)/grit/devtools_resources.h',
            '<(grit_out_dir)/devtools_resources.pak',
            '<(grit_out_dir)/grit/devtools_resources_map.cc',
            '<(grit_out_dir)/grit/devtools_resources_map.h',
          ],
          'action': ['<@(grit_cmd)',
                     '-i', '<(grit_grd_file)', 'build',
                     '-o', '<(grit_out_dir)',
                     '-D', 'SHARED_INTERMEDIATE_DIR=<(SHARED_INTERMEDIATE_DIR)',
                     '<@(grit_defines)' ],
          'message': 'Generating resources from <(grit_grd_file)',
        },
        {
          'action_name': 'devtools_frontend_resources',
          # This can't use ../build/grit_action.gypi because the grd file
          # is generated a build time, so the trick of using grit_info to get
          # the real inputs/outputs at GYP time isn't possible.
          'variables': {
            'grit_cmd': ['python', '../tools/grit/grit.py'],
            'frontend_folder': 'browser/debugger/frontend',
            'grit_grd_file':
               '<(frontend_folder)/devtools_frontend_resources.grd',
          },
          'inputs': [
            '<(grit_grd_file)',
            '<(frontend_folder)/devtools_frontend.html',
          ],
          'outputs': [
            '<(grit_out_dir)/grit/devtools_frontend_resources.h',
            '<(grit_out_dir)/devtools_frontend_resources.pak',
            '<(grit_out_dir)/grit/devtools_frontend_resources_map.cc',
            '<(grit_out_dir)/grit/devtools_frontend_resources_map.h',
          ],
          'action': ['<@(grit_cmd)',
                     '-i', '<(grit_grd_file)', 'build',
                     '-o', '<(grit_out_dir)',
                     '-D', 'SHARED_INTERMEDIATE_DIR=<(SHARED_INTERMEDIATE_DIR)',
                     '<@(grit_defines)' ],
          'message': 'Generating resources from <(grit_grd_file)',
        },
      ],
      'includes': [ '../build/grit_target.gypi' ],
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
        ['OS=="linux" and chromeos==1 and branding=="Chrome"', {
          'copies': [
            {
              'destination': '<(PRODUCT_DIR)/extensions',
              'files': [
                '>!@(ls browser/extensions/default_extensions/chromeos/cache/*)'
              ]
            }
          ],
        }],
      ],
    },
    {
      'target_name': 'debugger',
      'type': 'static_library',
      'msvs_guid': '57823D8C-A317-4713-9125-2C91FDFD12D6',
      'dependencies': [
        'chrome_extra_resources',
        'chrome_resources',
        'chrome_strings',
        '../base/base.gyp:base',
        '../net/net.gyp:http_server',
        'theme_resources',
        'theme_resources_standard',
        '../skia/skia.gyp:skia',
        '../third_party/icu/icu.gyp:icui18n',
        '../third_party/icu/icu.gyp:icuuc',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'browser/debugger/browser_list_tabcontents_provider.cc',
        'browser/debugger/browser_list_tabcontents_provider.h',
        'browser/debugger/debugger_remote_service.cc',
        'browser/debugger/debugger_remote_service.h',
        'browser/debugger/devtools_client_host.cc',
        'browser/debugger/devtools_client_host.h',
        'browser/debugger/devtools_file_util.cc',
        'browser/debugger/devtools_file_util.h',
        'browser/debugger/devtools_http_protocol_handler.cc',
        'browser/debugger/devtools_http_protocol_handler.h',
        'browser/debugger/devtools_manager.cc',
        'browser/debugger/devtools_manager.h',
        'browser/debugger/devtools_netlog_observer.cc',
        'browser/debugger/devtools_netlog_observer.h',
        'browser/debugger/devtools_protocol_handler.cc',
        'browser/debugger/devtools_protocol_handler.h',
        'browser/debugger/devtools_remote.h',
        'browser/debugger/devtools_remote_listen_socket.cc',
        'browser/debugger/devtools_remote_listen_socket.h',
        'browser/debugger/devtools_remote_message.cc',
        'browser/debugger/devtools_remote_message.h',
        'browser/debugger/devtools_remote_service.cc',
        'browser/debugger/devtools_remote_service.h',
        'browser/debugger/devtools_handler.cc',
        'browser/debugger/devtools_handler.h',
        'browser/debugger/devtools_toggle_action.h',
        'browser/debugger/devtools_window.cc',
        'browser/debugger/devtools_window.h',
        'browser/debugger/extension_ports_remote_service.cc',
        'browser/debugger/extension_ports_remote_service.h',
        'browser/debugger/inspectable_tab_proxy.cc',
        'browser/debugger/inspectable_tab_proxy.h',
      ],
      'conditions': [
        ['toolkit_uses_gtk == 1', {
          'dependencies': [
            '../build/linux/system.gyp:gtk',
          ],
        }],
      ],
    },
    {
      'target_name': 'utility',
      'type': 'static_library',
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
        ['toolkit_uses_gtk == 1', {
          'dependencies': [
            '../build/linux/system.gyp:gtk',
          ],
        }],
      ],
    },
    {
      'target_name': 'profile_import',
      'type': 'static_library',
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
      # Provides a syncapi dynamic library target from checked-in binaries,
      # or from compiling a stub implementation.
      'target_name': 'syncapi',
      'type': 'static_library',
      'sources': [
        'browser/sync/engine/http_post_provider_factory.h',
        'browser/sync/engine/http_post_provider_interface.h',
        'browser/sync/engine/syncapi.cc',
        'browser/sync/engine/syncapi.h',
        'browser/sync/engine/configure_reason.h'
      ],
      'include_dirs': [
        '..',
      ],
      'defines' : [
        '_CRT_SECURE_NO_WARNINGS',
        '_USE_32BIT_TIME_T',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../build/temp_gyp/googleurl.gyp:googleurl',
        '../jingle/jingle.gyp:notifier',
        '../third_party/icu/icu.gyp:icuuc',
        '../third_party/sqlite/sqlite.gyp:sqlite',
        'app/policy/cloud_policy_codegen.gyp:policy',
        'browser/sync/protocol/sync_proto.gyp:sync_proto_cpp',
        'common_constants',
        'common_net',
        'sync',
        'sync_notifier',
      ],
      'export_dependent_settings': [
        'browser/sync/protocol/sync_proto.gyp:sync_proto_cpp',
        'sync',
      ],
      # This target exports a hard dependency because syncapi.h includes
      # generated proto header files from sync_proto_cpp.
      'hard_dependency': 1,
    },
    {
      'target_name': 'sync',
      'type': 'static_library',
      'sources': [
        'browser/sync/engine/all_status.cc',
        'browser/sync/engine/all_status.h',
        'browser/sync/engine/apply_updates_command.cc',
        'browser/sync/engine/apply_updates_command.h',
        'browser/sync/engine/build_and_process_conflict_sets_command.cc',
        'browser/sync/engine/build_and_process_conflict_sets_command.h',
        'browser/sync/engine/build_commit_command.cc',
        'browser/sync/engine/build_commit_command.h',
        'browser/sync/engine/change_reorder_buffer.cc',
        'browser/sync/engine/change_reorder_buffer.h',
        'browser/sync/engine/cleanup_disabled_types_command.cc',
        'browser/sync/engine/cleanup_disabled_types_command.h',
        'browser/sync/engine/clear_data_command.cc',
        'browser/sync/engine/clear_data_command.h',
        'browser/sync/engine/conflict_resolver.cc',
        'browser/sync/engine/conflict_resolver.h',
        'browser/sync/engine/download_updates_command.cc',
        'browser/sync/engine/download_updates_command.h',
        'browser/sync/engine/get_commit_ids_command.cc',
        'browser/sync/engine/get_commit_ids_command.h',
        'browser/sync/engine/model_changing_syncer_command.cc',
        'browser/sync/engine/model_changing_syncer_command.h',
        'browser/sync/engine/model_safe_worker.cc',
        'browser/sync/engine/model_safe_worker.h',
        'browser/sync/engine/net/server_connection_manager.cc',
        'browser/sync/engine/net/server_connection_manager.h',
        'browser/sync/engine/net/syncapi_server_connection_manager.cc',
        'browser/sync/engine/net/syncapi_server_connection_manager.h',
        'browser/sync/engine/net/url_translator.cc',
        'browser/sync/engine/net/url_translator.h',
        'browser/sync/engine/nudge_source.h',
        'browser/sync/engine/polling_constants.cc',
        'browser/sync/engine/polling_constants.h',
        'browser/sync/engine/post_commit_message_command.cc',
        'browser/sync/engine/post_commit_message_command.h',
        'browser/sync/engine/process_commit_response_command.cc',
        'browser/sync/engine/process_commit_response_command.h',
        'browser/sync/engine/process_updates_command.cc',
        'browser/sync/engine/process_updates_command.h',
        'browser/sync/engine/resolve_conflicts_command.cc',
        'browser/sync/engine/resolve_conflicts_command.h',
        'browser/sync/engine/store_timestamps_command.cc',
        'browser/sync/engine/store_timestamps_command.h',
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
        'browser/sync/engine/syncer_types.cc',
        'browser/sync/engine/syncer_types.h',
        'browser/sync/engine/syncer_util.cc',
        'browser/sync/engine/syncer_util.h',
        'browser/sync/engine/syncproto.h',
        'browser/sync/engine/update_applicator.cc',
        'browser/sync/engine/update_applicator.h',
        'browser/sync/engine/verify_updates_command.cc',
        'browser/sync/engine/verify_updates_command.h',
        'browser/sync/js_arg_list.cc',
        'browser/sync/js_arg_list.h',
        'browser/sync/js_backend.h',
        'browser/sync/js_directory_change_listener.cc',
        'browser/sync/js_directory_change_listener.h',
        'browser/sync/js_event_details.cc',
        'browser/sync/js_event_details.h',
        'browser/sync/js_event_handler.h',
        'browser/sync/js_event_handler_list.cc',
        'browser/sync/js_event_handler_list.h',
        'browser/sync/js_event_router.h',
        'browser/sync/js_frontend.h',
        'browser/sync/js_sync_manager_observer.cc',
        'browser/sync/js_sync_manager_observer.h',
        'browser/sync/protocol/proto_enum_conversions.cc',
        'browser/sync/protocol/proto_enum_conversions.h',
        'browser/sync/protocol/proto_value_conversions.cc',
        'browser/sync/protocol/proto_value_conversions.h',
        'browser/sync/protocol/service_constants.h',
        'browser/sync/sessions/ordered_commit_set.cc',
        'browser/sync/sessions/ordered_commit_set.h',
        'browser/sync/sessions/session_state.cc',
        'browser/sync/sessions/session_state.h',
        'browser/sync/sessions/status_controller.cc',
        'browser/sync/sessions/status_controller.h',
        'browser/sync/sessions/sync_session.cc',
        'browser/sync/sessions/sync_session.h',
        'browser/sync/sessions/sync_session_context.cc',
        'browser/sync/sessions/sync_session_context.h',
        'browser/sync/shared_value.h',
        'browser/sync/syncable/autofill_migration.h',
        'browser/sync/syncable/blob.h',
        'browser/sync/syncable/dir_open_result.h',
        'browser/sync/syncable/directory_backing_store.cc',
        'browser/sync/syncable/directory_backing_store.h',
        'browser/sync/syncable/directory_change_listener.h',
        'browser/sync/syncable/directory_event.h',
        'browser/sync/syncable/directory_manager.cc',
        'browser/sync/syncable/directory_manager.h',
        'browser/sync/syncable/model_type.cc',
        'browser/sync/syncable/model_type.h',
        'browser/sync/syncable/model_type_payload_map.cc',
        'browser/sync/syncable/model_type_payload_map.h',
        'browser/sync/syncable/nigori_util.cc',
        'browser/sync/syncable/nigori_util.h',
        'browser/sync/syncable/syncable-inl.h',
        'browser/sync/syncable/syncable.cc',
        'browser/sync/syncable/syncable.h',
        'browser/sync/syncable/syncable_changes_version.h',
        'browser/sync/syncable/syncable_columns.h',
        'browser/sync/syncable/syncable_id.cc',
        'browser/sync/syncable/syncable_id.h',
        'browser/sync/syncable/syncable_enum_conversions.cc',
        'browser/sync/syncable/syncable_enum_conversions.h',
        'browser/sync/util/cryptographer.cc',
        'browser/sync/util/cryptographer.h',
        'browser/sync/util/dbgq.h',
        'browser/sync/util/extensions_activity_monitor.cc',
        'browser/sync/util/extensions_activity_monitor.h',
        'browser/sync/util/nigori.cc',
        'browser/sync/util/nigori.h',
        'browser/sync/util/user_settings.cc',
        'browser/sync/util/user_settings.h',
        'browser/sync/util/user_settings_posix.cc',
        'browser/sync/util/user_settings_win.cc',
      ],
      'include_dirs': [
        '..',
      ],
      'defines' : [
        'SYNC_ENGINE_VERSION_STRING="Unknown"',
        '_CRT_SECURE_NO_WARNINGS',
        '_USE_32BIT_TIME_T',
      ],
      'dependencies': [
        'common',
        '../base/base.gyp:base',
        '../crypto/crypto.gyp:crypto',
        '../skia/skia.gyp:skia',
        'browser/sync/protocol/sync_proto.gyp:sync_proto_cpp',
      ],
      'export_dependent_settings': [
        '../base/base.gyp:base',
        '../crypto/crypto.gyp:crypto',
        'browser/sync/protocol/sync_proto.gyp:sync_proto_cpp',
      ],
      # This target exports a hard dependency because its header files include
      # protobuf header files from sync_proto_cpp.
      'hard_dependency': 1,
      'conditions': [
        ['OS=="win"', {
          'sources' : [
            'browser/sync/util/data_encryption.cc',
            'browser/sync/util/data_encryption.h',
          ],
        }],
        ['toolkit_uses_gtk == 1', {
          'dependencies': [
            '../build/linux/system.gyp:gtk',
          ],
          'link_settings': {
            'libraries': [
              '-lXss',
            ],
          },
        }],
        ['OS=="linux" and chromeos==1', {
          'include_dirs': [
            '<(grit_out_dir)',
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
    # A library for sending and receiving server-issued notifications.
    {
      'target_name': 'sync_notifier',
      'type': 'static_library',
      'sources': [
        'browser/sync/notifier/cache_invalidation_packet_handler.cc',
        'browser/sync/notifier/cache_invalidation_packet_handler.h',
        'browser/sync/notifier/chrome_invalidation_client.cc',
        'browser/sync/notifier/chrome_invalidation_client.h',
        'browser/sync/notifier/chrome_system_resources.cc',
        'browser/sync/notifier/chrome_system_resources.h',
        'browser/sync/notifier/invalidation_notifier.h',
        'browser/sync/notifier/invalidation_notifier.cc',
        'browser/sync/notifier/invalidation_util.cc',
        'browser/sync/notifier/invalidation_util.h',
        'browser/sync/notifier/non_blocking_invalidation_notifier.h',
        'browser/sync/notifier/non_blocking_invalidation_notifier.cc',
        'browser/sync/notifier/p2p_notifier.h',
        'browser/sync/notifier/p2p_notifier.cc',
        'browser/sync/notifier/registration_manager.cc',
        'browser/sync/notifier/registration_manager.h',
        'browser/sync/notifier/state_writer.h',
        'browser/sync/notifier/sync_notifier.h',
        'browser/sync/notifier/sync_notifier_factory.h',
        'browser/sync/notifier/sync_notifier_factory.cc',
        'browser/sync/notifier/sync_notifier_callback.h',
      ],
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        'sync',
        '../jingle/jingle.gyp:notifier',
        '../third_party/cacheinvalidation/cacheinvalidation.gyp:cacheinvalidation',
      ],
      # This target exports a hard dependency because it depends on
      # cacheinvalidation (which itself has hard_dependency set).
      'hard_dependency': 1,
      'export_dependent_settings': [
        '../jingle/jingle.gyp:notifier',
        '../third_party/cacheinvalidation/cacheinvalidation.gyp:cacheinvalidation',
      ],
    },
    {
      'target_name': 'service',
      'type': 'static_library',
      'msvs_guid': '2DA87614-55C5-4E56-A17E-0CD099786197',
      'dependencies': [
        'chrome_strings',
        'common',
        'common_net',
        '../base/base.gyp:base',
        '../jingle/jingle.gyp:notifier',
        '../printing/printing.gyp:printing',
        '../skia/skia.gyp:skia',
        '../third_party/libjingle/libjingle.gyp:libjingle',
      ],
      'sources': [
        'service/service_child_process_host.cc',
        'service/service_child_process_host.h',
        'service/service_ipc_server.cc',
        'service/service_ipc_server.h',
        'service/service_main.cc',
        'service/service_process.cc',
        'service/service_process.h',
        'service/service_process_prefs.cc',
        'service/service_process_prefs.h',
        'service/service_utility_process_host.cc',
        'service/service_utility_process_host.h',
        'service/cloud_print/cloud_print_consts.cc',
        'service/cloud_print/cloud_print_consts.h',
        'service/cloud_print/cloud_print_helpers.cc',
        'service/cloud_print/cloud_print_helpers.h',
        'service/cloud_print/cloud_print_proxy.cc',
        'service/cloud_print/cloud_print_proxy.h',
        'service/cloud_print/cloud_print_proxy_backend.cc',
        'service/cloud_print/cloud_print_proxy_backend.h',
        'service/cloud_print/cloud_print_token_store.cc',
        'service/cloud_print/cloud_print_token_store.h',
        'service/cloud_print/cloud_print_url_fetcher.cc',
        'service/cloud_print/cloud_print_url_fetcher.h',
        'service/cloud_print/job_status_updater.cc',
        'service/cloud_print/job_status_updater.h',
        'service/cloud_print/print_system_dummy.cc',
        'service/cloud_print/print_system.cc',
        'service/cloud_print/print_system.h',
        'service/cloud_print/printer_job_handler.cc',
        'service/cloud_print/printer_job_handler.h',
        'service/gaia/service_gaia_authenticator.cc',
        'service/gaia/service_gaia_authenticator.h',
        'service/net/service_url_request_context.cc',
        'service/net/service_url_request_context.h',
      ],
      'include_dirs': [
        '..',
      ],
      'conditions': [
        ['OS=="win"', {
          'defines': [
            # CP_PRINT_SYSTEM_AVAILABLE disables default dummy implementation
            # of cloud print system, and allows to use custom implementaiton.
            'CP_PRINT_SYSTEM_AVAILABLE',
          ],
          'sources': [
            'service/cloud_print/print_system_win.cc',
          ],
        }],
        ['toolkit_uses_gtk == 1', {
          'dependencies': [
            '../build/linux/system.gyp:gtk',
          ],
        }],
        ['use_cups==1', {
          'dependencies': [
            '../printing/printing.gyp:cups',
          ],
          'defines': [
            # CP_PRINT_SYSTEM_AVAILABLE disables default dummy implementation
            # of cloud print system, and allows to use custom implementaiton.
            'CP_PRINT_SYSTEM_AVAILABLE',
          ],
          'sources': [
            'service/cloud_print/print_system_cups.cc',
          ],
        }],
      ],
    },
    {
      'target_name': 'ipclist',
      'type': 'executable',
      'dependencies': [
        'test_support_common',
        '../skia/skia.gyp:skia',
      ],
      'include_dirs': [
         '..',
      ],
      'sources': [
        'tools/ipclist/all_messages.h',
        'tools/ipclist/ipclist.cc',
      ],
    },
  ],
  'conditions': [
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
            # chrome_exe_main_mac.mm's main() is the entry point for
            # the "chrome" (browser app) target.  All it does is jump
            # to chrome_dll's ChromeMain.  This is appropriate for
            # helper processes too, because the logic to discriminate
            # between process types at run time is actually directed
            # by the --type command line argument processed by
            # ChromeMain.  Sharing chrome_exe_main_mac.mm with the
            # browser app will suffice for now.
            'app/chrome_exe_main_mac.mm',
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
            'CHROMIUM_STRIP_SAVE_FILE': 'app/app.saves',
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
              'action_name': 'Generate InfoPlist.strings files',
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
              # targets.  In this case, -b0, -k0, and -s0 are used because
              # Breakpad, Keystone, and Subersion keys are never placed into
              # the helper.
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
          # This produces the app mode loader, but not as a bundle. Chromium
          # itself is responsible for producing bundles.
          'target_name': 'app_mode_app',
          'type': 'executable',
          'product_name': '<(mac_product_name) App Mode Loader',
          'sources': [
            'app/app_mode_loader_mac.mm',
            'common/app_mode_common_mac.h',
            'common/app_mode_common_mac.mm',
          ],
          'include_dirs': [
            '..',
          ],
          'link_settings': {
            'libraries': [
              '$(SDKROOT)/System/Library/Frameworks/CoreFoundation.framework',
              '$(SDKROOT)/System/Library/Frameworks/Foundation.framework',
            ],
          },
        },  # target app_mode_app
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
            'DYLIB_COMPATIBILITY_VERSION': '<(version_mac_dylib)',
            'DYLIB_CURRENT_VERSION': '<(version_mac_dylib)',
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
            '../app/app.gyp:app_base',
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
          'dependencies': [
            '../base/base.gyp:base',
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
          # Mac needs 'process_outputs_as_mac_bundle_resources' to be set,
          # and the option is only effective when the target type is native
          # binary. Hence we cannot build the Mac bundle resources here and
          # the action is duplicated in chrome_dll.gypi.
          'target_name': 'packed_extra_resources',
          'type': 'none',
          'variables': {
            'repack_path': '../tools/data_pack/repack.py',
          },
          'dependencies': [
            'chrome_extra_resources',
          ],
          'actions': [
            {
              'action_name': 'repack_resources',
              'variables': {
                'pak_inputs': [
                  '<(grit_out_dir)/component_extension_resources.pak',
                  '<(grit_out_dir)/devtools_frontend_resources.pak',
                  '<(grit_out_dir)/devtools_resources.pak',
                  '<(grit_out_dir)/options_resources.pak',
                  '<(grit_out_dir)/net_internals_resources.pak',
                  '<(grit_out_dir)/shared_resources.pak',
                  '<(grit_out_dir)/sync_internals_resources.pak',
                ],
              },
              'inputs': [
                '<(repack_path)',
                '<@(pak_inputs)',
              ],
              'outputs': [
                '<(PRODUCT_DIR)/resources.pak',
              ],
              'action': ['python', '<(repack_path)', '<@(_outputs)',
                         '<@(pak_inputs)'],
            },
          ]
        }
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
            ['linux_strip_reliability_tests==1', {
              'actions': [
                {
                  'action_name': 'strip_reliability_tests',
                  'inputs': [
                    '<(PRODUCT_DIR)/automated_ui_tests',
                    '<(PRODUCT_DIR)/reliability_tests',
                    '<(PRODUCT_DIR)/lib.target/_pyautolib.so',
                  ],
                  'outputs': [
                    '<(PRODUCT_DIR)/strip_reliability_tests.stamp',
                  ],
                  'action': ['strip',
                             '-g',
                             '<@(_inputs)'],
                  'message': 'Stripping reliability tests',
                },
              ],
              'dependencies': [
                'automated_ui_tests',
                'reliability_tests',
              ],
            }],
          ],
        },
        {
          'target_name': 'ipcfuzz',
          'type': 'loadable_module',
          'include_dirs': [
            '..',
          ],
          'dependencies': [
            'test_support_common',
            '../skia/skia.gyp:skia',
          ],
          'sources': [
            'tools/ipclist/all_messages.h',
            'tools/ipclist/ipcfuzz.cc',
          ],
        },
      ],
    },],  # OS=="linux"
    ['OS=="win"',
      { 'targets': [
        {
          # TODO(sgk):  remove this when we change the buildbots to
          # use the generated build\all.sln file to build the world.
          'target_name': 'pull_in_all',
          'type': 'none',
          'dependencies': [
            'installer/mini_installer.gyp:*',
            'installer/installer_tools.gyp:*',
            'installer/upgrade_test.gyp:*',
            '../app/app.gyp:*',
            '../base/base.gyp:*',
            '../chrome_frame/chrome_frame.gyp:*',
            '../content/content.gyp:*',
            '../ipc/ipc.gyp:*',
            '../media/media.gyp:*',
            '../net/net.gyp:*',
            '../ppapi/ppapi.gyp:*',
            '../ppapi/ppapi_internal.gyp:*',
            '../printing/printing.gyp:*',
            '../sdch/sdch.gyp:*',
            '../skia/skia.gyp:*',
            '../testing/gmock.gyp:*',
            '../testing/gtest.gyp:*',
            '../third_party/bsdiff/bsdiff.gyp:*',
            '../third_party/bspatch/bspatch.gyp:*',
            '../third_party/bzip2/bzip2.gyp:*',
            '../third_party/codesighs/codesighs.gyp:*',
            '../third_party/iccjpeg/iccjpeg.gyp:*',
            '../third_party/icu/icu.gyp:*',
            '../third_party/libpng/libpng.gyp:*',
            '../third_party/libwebp/libwebp.gyp:*',
            '../third_party/libxslt/libxslt.gyp:*',
            '../third_party/lzma_sdk/lzma_sdk.gyp:*',
            '../third_party/modp_b64/modp_b64.gyp:*',
            '../third_party/npapi/npapi.gyp:*',
            '../third_party/qcms/qcms.gyp:*',
            '../third_party/sqlite/sqlite.gyp:*',
            '../third_party/zlib/zlib.gyp:*',
            '../ui/ui.gyp:*',
            '../webkit/support/webkit_support.gyp:*',
            '../webkit/webkit.gyp:*',

            '../build/temp_gyp/googleurl.gyp:*',

            '../breakpad/breakpad.gyp:*',
            '../courgette/courgette.gyp:*',
            '../rlz/rlz.gyp:*',
            '../sandbox/sandbox.gyp:*',
            '../tools/memory_watcher/memory_watcher.gyp:*',
            '../v8/tools/gyp/v8.gyp:v8_shell',
            '<(libjpeg_gyp_path):*',
          ],
          'conditions': [
            ['win_use_allocator_shim==1', {
              'dependencies': [
                '../base/allocator/allocator.gyp:*',
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
          'target_name': 'chrome_version_header',
          'type': 'none',
          'hard_dependency': 1,
          'dependencies': [
            '../build/util/build_util.gyp:lastchange',
          ],
          'actions': [
            {
              'action_name': 'version_header',
              'variables': {
                'lastchange_path':
                  '<(SHARED_INTERMEDIATE_DIR)/build/LASTCHANGE',
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
                '<(version_path)',
                '<(branding_path)',
                '<(lastchange_path)',
                'version.h.in',
              ],
              'outputs': [
                '<(SHARED_INTERMEDIATE_DIR)/version.h',
              ],
              'action': [
                'python',
                '<(version_py_path)',
                '-f', '<(version_path)',
                '-f', '<(branding_path)',
                '-f', '<(lastchange_path)',
                'version.h.in',
                '<@(_outputs)',
              ],
              'message': 'Generating version header file: <@(_outputs)',
            },
          ],
        },
        {
          'target_name': 'automation',
          'type': 'static_library',
          'msvs_guid': '1556EF78-C7E6-43C8-951F-F6B43AC0DD12',
          'dependencies': [
            'theme_resources',
            'theme_resources_standard',
            '../base/base.gyp:test_support_base',
            '../skia/skia.gyp:skia',
            '../testing/gtest.gyp:gtest',
          ],
          'include_dirs': [
            '..',
          ],
          'sources': [
             'test/automation/autocomplete_edit_proxy.cc',
             'test/automation/autocomplete_edit_proxy.h',
             'test/automation/automation_handle_tracker.cc',
             'test/automation/automation_handle_tracker.h',
             'test/automation/automation_json_requests.cc',
             'test/automation/automation_json_requests.h',
             'test/automation/automation_proxy.cc',
             'test/automation/automation_proxy.h',
             'test/automation/browser_proxy.cc',
             'test/automation/browser_proxy.h',
             'test/automation/dom_element_proxy.cc',
             'test/automation/dom_element_proxy.h',
             'test/automation/extension_proxy.cc',
             'test/automation/extension_proxy.h',
             'test/automation/javascript_execution_controller.cc',
             'test/automation/javascript_execution_controller.h',
             'test/automation/tab_proxy.cc',
             'test/automation/tab_proxy.h',
             'test/automation/window_proxy.cc',
             'test/automation/window_proxy.h',
          ],
        },
        {
          'target_name': 'crash_service',
          'type': 'executable',
          'msvs_guid': '89C1C190-A5D1-4EC4-BD6A-67FF2195C7CC',
          'dependencies': [
            'app/policy/cloud_policy_codegen.gyp:policy',
            'common_constants',
            'installer_util',
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
      ]},  # 'targets'
    ],  # OS=="win"
    ['os_posix == 1 and OS != "mac"', {
      'targets': [{
        'target_name': 'packed_resources',
        'type': 'none',
        'variables': {
          'repack_path': '../tools/data_pack/repack.py',
        },
        'actions': [
          # TODO(mark): These actions are duplicated for the Mac in the
          # chrome_dll target.  Can they be unified?
          #
          # Mac needs 'process_outputs_as_mac_bundle_resources' to be set,
          # and the option is only effective when the target type is native
          # binary. Hence we cannot build the Mac bundle resources here.
          {
            'action_name': 'repack_chrome',
            'variables': {
              'pak_inputs': [
                '<(grit_out_dir)/autofill_resources.pak',
                '<(grit_out_dir)/browser_resources.pak',
                '<(grit_out_dir)/common_resources.pak',
                '<(grit_out_dir)/default_plugin_resources/default_plugin_resources.pak',
                '<(grit_out_dir)/renderer_resources.pak',
                '<(grit_out_dir)/theme_resources.pak',
                '<(SHARED_INTERMEDIATE_DIR)/app/app_resources/app_resources.pak',
                '<(SHARED_INTERMEDIATE_DIR)/ui/gfx/gfx_resources.pak',
                '<(SHARED_INTERMEDIATE_DIR)/net/net_resources.pak',
                '<(SHARED_INTERMEDIATE_DIR)/webkit/webkit_chromium_resources.pak',
                '<(SHARED_INTERMEDIATE_DIR)/webkit/webkit_resources.pak',
              ],
              'conditions': [
                ['touchui==0', {
                  'pak_inputs': [
                    '<(grit_out_dir)/theme_resources_standard.pak',
                  ],
                }, {  # else: touchui!=0
                  'pak_inputs': [
                    '<(grit_out_dir)/theme_resources_large.pak',
                  ],
                }],
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
        # We'll install the resource files to the product directory.
        'copies': [
          {
            'destination': '<(PRODUCT_DIR)/locales',
            'files': [
              '>!@(<(repack_locales_cmd) -o -g \'<(grit_out_dir)\' -s \'<(SHARED_INTERMEDIATE_DIR)\' -x \'<(INTERMEDIATE_DIR)\' <(locales))',
            ],
          },
          {
            'destination': '<(PRODUCT_DIR)',
            'files': [
              '<(INTERMEDIATE_DIR)/repack/chrome.pak'
            ],
          },
        ],
      }],  # targets
    }],  # os_posix == 1 and OS != "mac"
  ],  # 'conditions'
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
