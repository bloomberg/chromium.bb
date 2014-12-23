# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    # Product name is used for Mac bundle.
    'app_shell_product_name': 'App Shell',
    # The version is high enough to be supported by Omaha (at least 31)
    # but fake enough to be obviously not a Chrome release.
    'app_shell_version': '38.1234.5678.9',
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'app_shell_lib',
      'type': 'static_library',
      'dependencies': [
        'app_shell_version_header',
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/base/base.gyp:base_prefs',
        '<(DEPTH)/components/components.gyp:omaha_client',
        '<(DEPTH)/components/components.gyp:pref_registry',
        '<(DEPTH)/components/components.gyp:user_prefs',
        '<(DEPTH)/components/components.gyp:web_cache_renderer',
        '<(DEPTH)/content/content.gyp:content',
        '<(DEPTH)/content/content.gyp:content_gpu',
        '<(DEPTH)/content/content.gyp:content_ppapi_plugin',
        '<(DEPTH)/content/content_shell_and_tests.gyp:content_shell_lib',
        '<(DEPTH)/device/core/core.gyp:device_core',
        '<(DEPTH)/device/hid/hid.gyp:device_hid',
        '<(DEPTH)/extensions/browser/api/api_registration.gyp:extensions_api_registration',
        '<(DEPTH)/extensions/common/api/api.gyp:extensions_api',
        '<(DEPTH)/extensions/extensions.gyp:extensions_browser',
        '<(DEPTH)/extensions/extensions.gyp:extensions_common',
        '<(DEPTH)/extensions/extensions.gyp:extensions_renderer',
        '<(DEPTH)/extensions/extensions.gyp:extensions_shell_and_test_pak',
        '<(DEPTH)/extensions/extensions.gyp:extensions_utility',
        '<(DEPTH)/extensions/extensions_resources.gyp:extensions_resources',
        '<(DEPTH)/extensions/shell/browser/api/api_registration.gyp:shell_api_registration',
        '<(DEPTH)/extensions/shell/common/api/api.gyp:shell_api',
        '<(DEPTH)/mojo/mojo_base.gyp:mojo_environment_chromium',
        '<(DEPTH)/mojo/mojo_edk.gyp:mojo_system_impl',
        '<(DEPTH)/skia/skia.gyp:skia',
        '<(DEPTH)/third_party/WebKit/public/blink.gyp:blink',
        '<(DEPTH)/v8/tools/gyp/v8.gyp:v8',
      ],
      'include_dirs': [
        '../..',
        '<(SHARED_INTERMEDIATE_DIR)',
        '<(SHARED_INTERMEDIATE_DIR)/extensions/shell',
      ],
      'sources': [
        'app/paths_mac.h',
        'app/paths_mac.mm',
        'app/shell_main_delegate.cc',
        'app/shell_main_delegate.h',
        'browser/api/identity/identity_api.cc',
        'browser/api/identity/identity_api.h',
        'browser/shell_browser_context_keyed_service_factories.cc',
        'browser/shell_browser_context_keyed_service_factories.h',
        'browser/default_shell_browser_main_delegate.cc',
        'browser/default_shell_browser_main_delegate.h',
        'browser/desktop_controller.cc',
        'browser/desktop_controller.h',
        'browser/media_capture_util.cc',
        'browser/media_capture_util.h',
        'browser/shell_app_delegate.cc',
        'browser/shell_app_delegate.h',
        'browser/shell_app_view_guest_delegate.cc',
        'browser/shell_app_view_guest_delegate.h',
        'browser/shell_app_window_client.cc',
        'browser/shell_app_window_client.h',
        'browser/shell_app_window_client_mac.mm',
        'browser/shell_audio_controller_chromeos.cc',
        'browser/shell_audio_controller_chromeos.h',
        'browser/shell_browser_context.cc',
        'browser/shell_browser_context.h',
        'browser/shell_browser_main_delegate.h',
        'browser/shell_browser_main_parts.cc',
        'browser/shell_browser_main_parts.h',
        'browser/shell_browser_main_parts_mac.h',
        'browser/shell_browser_main_parts_mac.mm',
        'browser/shell_content_browser_client.cc',
        'browser/shell_content_browser_client.h',
        'browser/shell_desktop_controller_mac.h',
        'browser/shell_desktop_controller_mac.mm',
        'browser/shell_device_client.cc',
        'browser/shell_device_client.h',
        'browser/shell_display_info_provider.cc',
        'browser/shell_display_info_provider.h',
        'browser/shell_extension_host_delegate.cc',
        'browser/shell_extension_host_delegate.h',
        'browser/shell_extension_system.cc',
        'browser/shell_extension_system.h',
        'browser/shell_extension_system_factory.cc',
        'browser/shell_extension_system_factory.h',
        'browser/shell_extension_web_contents_observer.cc',
        'browser/shell_extension_web_contents_observer.h',
        'browser/shell_extensions_api_client.cc',
        'browser/shell_extensions_api_client.h',
        'browser/shell_extensions_browser_client.cc',
        'browser/shell_extensions_browser_client.h',
        'browser/shell_native_app_window.cc',
        'browser/shell_native_app_window.h',
        'browser/shell_native_app_window_mac.h',
        'browser/shell_native_app_window_mac.mm',
        'browser/shell_network_controller_chromeos.cc',
        'browser/shell_network_controller_chromeos.h',
        'browser/shell_network_delegate.cc',
        'browser/shell_network_delegate.h',
        'browser/shell_oauth2_token_service.cc',
        'browser/shell_oauth2_token_service.h',
        'browser/shell_omaha_query_params_delegate.cc',
        'browser/shell_omaha_query_params_delegate.h',
        'browser/shell_prefs.cc',
        'browser/shell_prefs.h',
        'browser/shell_runtime_api_delegate.cc',
        'browser/shell_runtime_api_delegate.h',
        'browser/shell_special_storage_policy.cc',
        'browser/shell_special_storage_policy.h',
        'browser/shell_speech_recognition_manager_delegate.cc',
        'browser/shell_speech_recognition_manager_delegate.h',
        'browser/shell_url_request_context_getter.cc',
        'browser/shell_url_request_context_getter.h',
        'browser/shell_web_contents_modal_dialog_manager.cc',
        'common/shell_content_client.cc',
        'common/shell_content_client.h',
        'common/shell_extensions_client.cc',
        'common/shell_extensions_client.h',
        'common/switches.cc',
        'common/switches.h',
        'renderer/shell_content_renderer_client.cc',
        'renderer/shell_content_renderer_client.h',
        'renderer/shell_extensions_renderer_client.cc',
        'renderer/shell_extensions_renderer_client.h',
        'utility/shell_content_utility_client.cc',
        'utility/shell_content_utility_client.h',
      ],
      'conditions': [
        ['use_aura==1', {
          'dependencies': [
            '<(DEPTH)/ui/wm/wm.gyp:wm',
          ],
          'sources': [
            'browser/shell_app_window_client_aura.cc',
            'browser/shell_desktop_controller_aura.cc',
            'browser/shell_desktop_controller_aura.h',
            'browser/shell_native_app_window_aura.cc',
            'browser/shell_native_app_window_aura.h',
            'browser/shell_screen.cc',
            'browser/shell_screen.h',
          ],
        }],
        ['chromeos==1', {
          'dependencies': [
            '<(DEPTH)/chromeos/chromeos.gyp:chromeos',
            '<(DEPTH)/ui/chromeos/ui_chromeos.gyp:ui_chromeos',
            '<(DEPTH)/ui/display/display.gyp:display',
          ],
          'sources': [
            'browser/api/shell_gcd/shell_gcd_api.cc',
            'browser/api/shell_gcd/shell_gcd_api.h',
            'browser/api/vpn_provider/vpn_service_factory.cc',
          ],
        }],
        ['disable_nacl==0 and OS=="linux"', {
          'dependencies': [
            '<(DEPTH)/components/nacl.gyp:nacl_helper',
          ],
        }],
        ['disable_nacl==0', {
          'dependencies': [
            '<(DEPTH)/components/nacl.gyp:nacl',
            '<(DEPTH)/components/nacl.gyp:nacl_browser',
            '<(DEPTH)/components/nacl.gyp:nacl_common',
            '<(DEPTH)/components/nacl.gyp:nacl_renderer',
            '<(DEPTH)/components/nacl.gyp:nacl_switches',
          ],
          'sources': [
            'browser/shell_nacl_browser_delegate.cc',
            'browser/shell_nacl_browser_delegate.h',
          ],
        }],
      ],
    },
    {
      'target_name': 'app_shell',
      'type': 'executable',
      'mac_bundle': 1,
      'dependencies': [
        'app_shell_lib',
        '<(DEPTH)/extensions/extensions.gyp:extensions_shell_and_test_pak',
      ],
      'include_dirs': [
        '../..',
      ],
      'sources': [
        'app/shell_main.cc',
      ],
      'conditions': [
        ['OS=="win"', {
          'msvs_settings': {
            'VCLinkerTool': {
              'SubSystem': '2',  # Set /SUBSYSTEM:WINDOWS
            },
          },
          'dependencies': [
            '<(DEPTH)/sandbox/sandbox.gyp:sandbox',
          ],
        }],
        ['OS=="win" and win_use_allocator_shim==1', {
          'dependencies': [
            '<(DEPTH)/base/allocator/allocator.gyp:allocator',
          ],
        }],
        ['OS=="mac"', {
          'product_name': '<(app_shell_product_name)',
          'dependencies!': [
            'app_shell_lib',
          ],
          'dependencies': [
            'app_shell_framework',
            'app_shell_helper',
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
          'copies': [{
              'destination': '<(PRODUCT_DIR)/<(app_shell_product_name).app/Contents/Frameworks',
              'files': [
                '<(PRODUCT_DIR)/<(app_shell_product_name) Helper.app',
              ],
          }],
          'postbuilds': [
            {
              'postbuild_name': 'Copy <(app_shell_product_name) Framework.framework',
              'action': [
                '../../build/mac/copy_framework_unversioned.sh',
                '${BUILT_PRODUCTS_DIR}/<(app_shell_product_name) Framework.framework',
                '${BUILT_PRODUCTS_DIR}/${CONTENTS_FOLDER_PATH}/Frameworks',
              ],
            },
            {
              # Modify the Info.plist as needed.
              'postbuild_name': 'Tweak Info.plist',
              'action': ['../../build/mac/tweak_info_plist.py',
                         '--scm=1',
                         '--version=<(app_shell_version)'],
            },
            {
              # This postbuild step is responsible for creating the following
              # helpers:
              #
              # App Shell Helper EH.app and App Shell Helper NP.app are
              # created from App Shell Helper.app.
              #
              # The EH helper is marked for an executable heap. The NP helper
              # is marked for no PIE (ASLR).
              'postbuild_name': 'Make More Helpers',
              'action': [
                '../../build/mac/make_more_helpers.sh',
                'Frameworks',
                '<(app_shell_product_name)',
              ],
            },
            {
              # Make sure there isn't any Objective-C in the shell's
              # executable.
              'postbuild_name': 'Verify No Objective-C',
              'action': [
                '../../build/mac/verify_no_objc.sh',
              ],
            },
          ],
        }],
      ],
    },

    {
      'target_name': 'app_shell_browsertests',
      'type': '<(gtest_target_type)',
      'dependencies': [
        'app_shell_lib',
        # TODO(yoz): find the right deps
        '<(DEPTH)/base/base.gyp:test_support_base',
        '<(DEPTH)/components/components.gyp:storage_monitor_test_support',
        '<(DEPTH)/content/content.gyp:content_app_both',
        '<(DEPTH)/content/content_shell_and_tests.gyp:content_browser_test_support',
        '<(DEPTH)/content/content_shell_and_tests.gyp:test_support_content',
        '<(DEPTH)/extensions/extensions.gyp:extensions_test_support',
        '<(DEPTH)/testing/gmock.gyp:gmock',
        '<(DEPTH)/testing/gtest.gyp:gtest',
      ],
      'defines': [
        'HAS_OUT_OF_PROC_TEST_RUNNER',
      ],
      'sources': [
        # TODO(yoz): Refactor once we have a second test target.
        # TODO(yoz): Something is off here; should this .gyp file be
        # in the parent directory? Test target extensions_browsertests?
        '../browser/api/audio/audio_apitest.cc',
        '../browser/api/dns/dns_apitest.cc',
        '../browser/api/hid/hid_apitest.cc',
        '../browser/api/socket/socket_apitest.cc',
        '../browser/api/sockets_tcp/sockets_tcp_apitest.cc',
        '../browser/api/sockets_udp/sockets_udp_apitest.cc',
        '../browser/api/system_cpu/system_cpu_apitest.cc',
        '../browser/api/system_display/system_display_apitest.cc',
        '../browser/api/system_memory/system_memory_apitest.cc',
        '../browser/api/system_network/system_network_apitest.cc',
        '../browser/api/system_storage/storage_api_test_util.cc',
        '../browser/api/system_storage/storage_api_test_util.h',
        '../browser/api/system_storage/system_storage_apitest.cc',
        '../browser/api/system_storage/system_storage_eject_apitest.cc',
        '../browser/api/usb/usb_apitest.cc',
        '../browser/guest_view/app_view/app_view_apitest.cc',
        '../browser/guest_view/web_view/web_view_apitest.h',
        '../browser/guest_view/web_view/web_view_apitest.cc',
        '../browser/guest_view/web_view/web_view_media_access_apitest.cc',
        '../browser/updater/update_service_browsertest.cc',
        'browser/shell_browsertest.cc',
        'test/shell_apitest.cc',
        'test/shell_apitest.h',
        'test/shell_test.cc',
        'test/shell_test.h',
        'test/shell_test_launcher_delegate.cc',
        'test/shell_test_launcher_delegate.h',
        'test/shell_tests_main.cc',
      ],
      'conditions': [
        ['OS=="win" and win_use_allocator_shim==1', {
          'dependencies': [
            '<(DEPTH)/base/allocator/allocator.gyp:allocator',
          ],
        }],
        ['OS=="mac"', {
          'dependencies': [
            'app_shell',  # Needed for App Shell.app's Helper.
          ],
        }],
      ],
    },
    {
      'target_name': 'app_shell_unittests',
      'type': 'executable',
      'dependencies': [
        'app_shell_lib',
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/base/base.gyp:test_support_base',
        '<(DEPTH)/content/content.gyp:content_app_both',
        '<(DEPTH)/content/content_shell_and_tests.gyp:test_support_content',
        '<(DEPTH)/extensions/extensions.gyp:extensions_shell_and_test_pak',
        '<(DEPTH)/extensions/extensions.gyp:extensions_test_support',
        '<(DEPTH)/testing/gtest.gyp:gtest',
      ],
      'sources': [
        '../test/extensions_unittests_main.cc',
        'browser/api/identity/identity_api_unittest.cc',
        'browser/shell_oauth2_token_service_unittest.cc',
        'browser/shell_prefs_unittest.cc',
        'common/shell_content_client_unittest.cc'
      ],
      'conditions': [
        ['disable_nacl==0', {
          'sources': [
            'browser/shell_nacl_browser_delegate_unittest.cc',
          ],
        }],
        ['use_aura==1', {
          'sources': [
            'browser/shell_desktop_controller_aura_unittest.cc',
            'browser/shell_native_app_window_aura_unittest.cc',
            'browser/shell_screen_unittest.cc',
          ],
          'dependencies': [
            '<(DEPTH)/ui/aura/aura.gyp:aura_test_support',
          ],
        }],
        ['chromeos==1', {
          'dependencies': [
            '<(DEPTH)/chromeos/chromeos.gyp:chromeos_test_support_without_gmock',
          ],
          'sources': [
            'browser/api/shell_gcd/shell_gcd_api_unittest.cc',
            'browser/shell_audio_controller_chromeos_unittest.cc',
          ],
        }],
        ['OS=="win" and win_use_allocator_shim==1', {
          'dependencies': [
            '<(DEPTH)/base/allocator/allocator.gyp:allocator',
          ],
        }],
      ],
    },
    {
      'target_name': 'app_shell_version_header',
      'type': 'none',
      'direct_dependent_settings': {
        'include_dirs': [
          '<(SHARED_INTERMEDIATE_DIR)',
        ],
      },
      'actions': [
        {
          'action_name': 'version_header',
          'message': 'Generating version header file: <@(_outputs)',
          'variables': {
            'lastchange_path': '<(DEPTH)/build/util/LASTCHANGE',
          },
          'inputs': [
            '<(version_path)',
            '<(lastchange_path)',
            'common/version.h.in',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/extensions/shell/common/version.h',
          ],
          'action': [
            'python',
            '<(version_py_path)',
            '-f', '<(lastchange_path)',
            '-f', '<(version_path)',
            'common/version.h.in',
            '<@(_outputs)',
          ],
          'includes': [
            '../../build/util/version.gypi',
          ],
        },
      ],
    },
  ],  # targets

  'conditions': [
    ['OS=="mac"', {
      'targets': [
        {
          'target_name': 'app_shell_framework',
          'type': 'shared_library',
          'product_name': '<(app_shell_product_name) Framework',
          'mac_bundle': 1,
          'mac_bundle_resources': [
            '<(PRODUCT_DIR)/extensions_shell_and_test.pak',
            'app/framework-Info.plist',
          ],
          'mac_bundle_resources!': [
            'app/framework-Info.plist',
          ],
          'xcode_settings': {
            # The framework is placed within the .app's Framework
            # directory.  DYLIB_INSTALL_NAME_BASE and
            # LD_DYLIB_INSTALL_NAME affect -install_name.
            'DYLIB_INSTALL_NAME_BASE':
                '@executable_path/../Frameworks',
            # See /build/mac/copy_framework_unversioned.sh for
            # information on LD_DYLIB_INSTALL_NAME.
            'LD_DYLIB_INSTALL_NAME':
                '$(DYLIB_INSTALL_NAME_BASE:standardizepath)/$(WRAPPER_NAME)/$(PRODUCT_NAME)',

            'INFOPLIST_FILE': 'app/framework-Info.plist',
          },
          'dependencies': [
            'app_shell_lib',
          ],
          'include_dirs': [
            '../..',
          ],
          'sources': [
            'app/shell_main_mac.h',
            'app/shell_main_mac.cc',
          ],
          'postbuilds': [
            {
              # Modify the Info.plist as needed.  The script explains why
              # this is needed.  This is also done in the chrome target.
              # The framework needs the Breakpad keys if this feature is
              # enabled.  It does not need the Keystone keys; these always
              # come from the outer application bundle.  The framework
              # doesn't currently use the SCM keys for anything,
              # but this seems like a really good place to store them.
              'postbuild_name': 'Tweak Info.plist',
              'action': ['../../build/mac/tweak_info_plist.py',
                         '--breakpad=1',
                         '--keystone=0',
                         '--scm=1',
                         '--version=<(app_shell_version)',
                         '--branding=<(app_shell_product_name)'],
            },
          ],
          'conditions': [
            ['icu_use_data_file_flag==1', {
              'mac_bundle_resources': [
                '<(PRODUCT_DIR)/icudtl.dat',
              ],
            }],
            ['v8_use_external_startup_data==1', {
              'mac_bundle_resources': [
                '<(PRODUCT_DIR)/natives_blob.bin',
                '<(PRODUCT_DIR)/snapshot_blob.bin',
              ],
            }],
          ],
        },  # target app_shell_framework
        {
          'target_name': 'app_shell_helper',
          'type': 'executable',
          'variables': { 'enable_wexit_time_destructors': 1, },
          'product_name': '<(app_shell_product_name) Helper',
          'mac_bundle': 1,
          'dependencies': [
            'app_shell_framework',
          ],
          'sources': [
            'app/shell_main.cc',
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
            'INFOPLIST_FILE': 'app/helper-Info.plist',
          },
          'postbuilds': [
            {
              # The framework defines its load-time path
              # (DYLIB_INSTALL_NAME_BASE) relative to the main executable
              # (chrome).  A different relative path needs to be used in
              # helper_app.
              'postbuild_name': 'Fix Framework Link',
              'action': [
                'install_name_tool',
                '-change',
                '@executable_path/../Frameworks/<(app_shell_product_name) Framework.framework/<(app_shell_product_name) Framework',
                '@executable_path/../../../<(app_shell_product_name) Framework.framework/<(app_shell_product_name) Framework',
                '${BUILT_PRODUCTS_DIR}/${EXECUTABLE_PATH}'
              ],
            },
            {
              # Modify the Info.plist as needed.  The script explains why this
              # is needed.  This is also done in the chrome and chrome_dll
              # targets.  In this case, --breakpad=0, --keystone=0, and --scm=0
              # are used because Breakpad, Keystone, and SCM keys are
              # never placed into the helper.
              'postbuild_name': 'Tweak Info.plist',
              'action': ['../../build/mac/tweak_info_plist.py',
                         '--breakpad=0',
                         '--keystone=0',
                         '--scm=0',
                         '--version=<(app_shell_version)'],
            },
            {
              # Make sure there isn't any Objective-C in the helper app's
              # executable.
              'postbuild_name': 'Verify No Objective-C',
              'action': [
                '../../build/mac/verify_no_objc.sh',
              ],
            },
          ],
        },  # target app_shell_helper
      ],
    }],  # OS=="mac"
  ],
}
