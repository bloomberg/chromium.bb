# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'variables': {
    'chromium_code': 1,

    # Define the common dependencies that contain all the actual
    # Chromium functionality.  This list gets pulled in below by
    # the link of the actual chrome (or chromium) executable on
    # Linux or Mac, and into chrome.dll on Windows.
    # NOTE: Most new includes should go in the OS!="ios" condition below.
    'chromium_dependencies': [
      'common',
      'browser',
      '../content/content.gyp:content_app',
      '../sync/sync.gyp:sync_core',
    ],
    'allocator_target': '../base/allocator/allocator.gyp:allocator',
    'grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)/chrome',
    'protoc_out_dir': '<(SHARED_INTERMEDIATE_DIR)/protoc_out',
    'repack_locales_cmd': ['python', 'tools/build/repack_locales.py'],
    # TODO: remove this helper when we have loops in GYP
    'apply_locales_cmd': ['python', '<(DEPTH)/build/apply_locales.py'],
    'conditions': [
      ['OS!="ios"', {
        'chromium_dependencies': [
          'debugger',
          'plugin',
          'renderer',
          'utility',
          '../content/content.gyp:content_gpu',
          '../content/content.gyp:content_ppapi_plugin',
          '../content/content.gyp:content_worker',
          '../printing/printing.gyp:printing',
          '../third_party/WebKit/Source/WebKit/chromium/WebKit.gyp:inspector_resources',
        ],
      }],
      ['OS=="win"', {
        'nacl_defines': [
          'NACL_WINDOWS=1',
          'NACL_LINUX=0',
          'NACL_OSX=0',
        ],
        'platform_locale_settings_grd':
            'app/resources/locale_settings_win.grd',
      },],
      ['OS!="android" and OS!="ios"', {
        'chromium_dependencies': [
          # Android doesn't use the service process (only needed for print).
          'service',
        ],
      }],
      ['OS=="linux"', {
        'nacl_defines': [
          'NACL_WINDOWS=0',
          'NACL_LINUX=1',
          'NACL_OSX=0',
        ],
        'conditions': [
          ['chromeos==1', {
            'conditions': [
              ['branding=="Chrome"', {
                'platform_locale_settings_grd':
                    'app/resources/locale_settings_google_chromeos.grd',
              }, {  # branding!=Chrome
                'platform_locale_settings_grd':
                    'app/resources/locale_settings_chromiumos.grd',
              }],
            ]
          }, {  # chromeos==0
            'platform_locale_settings_grd':
                'app/resources/locale_settings_linux.grd',
          }],
        ],
      },],
      ['os_posix == 1 and OS != "mac" and OS != "ios" and OS != "linux"', {
        'platform_locale_settings_grd':
            'app/resources/locale_settings_linux.grd',
      },],
      ['OS=="mac"', {
        'tweak_info_plist_path': '../build/mac/tweak_info_plist.py',
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
      # TODO(mcgrathr): This duplicates native_client/build/common.gypi;
      # we should figure out a way to unify the settings.
      ['target_arch=="ia32"', {
        'nacl_defines': [
          'NACL_TARGET_SUBARCH=32',
          'NACL_TARGET_ARCH=x86',
          'NACL_BUILD_SUBARCH=32',
          'NACL_BUILD_ARCH=x86',
        ],
      }],
      ['target_arch=="x64"', {
        'nacl_defines': [
          'NACL_TARGET_SUBARCH=64',
          'NACL_TARGET_ARCH=x86',
          'NACL_BUILD_SUBARCH=64',
          'NACL_BUILD_ARCH=x86',
        ],
      }],
      ['target_arch=="arm"', {
        'nacl_defines': [
          'NACL_BUILD_ARCH=arm',
          'NACL_BUILD_SUBARCH=32',
          'NACL_TARGET_ARCH=arm',
          'NACL_TARGET_SUBARCH=32',
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
    # NOTE: Most new includes should go in the OS!="ios" condition below.
    '../build/win_precompile.gypi',
    'chrome_browser.gypi',
    'chrome_browser_ui.gypi',
    'chrome_common.gypi',
    'chrome_tests_unit.gypi',
    'version.gypi',
  ],
  'conditions': [
    ['OS!="ios"', {
      'includes': [
        'app/policy/policy_templates.gypi',
        'chrome_browser_extensions.gypi',
        'chrome_dll.gypi',
        'chrome_exe.gypi',
        'chrome_installer.gypi',
        'chrome_installer_util.gypi',
        'chrome_renderer.gypi',
        'chrome_tests.gypi',
        'nacl.gypi',
        '../apps/apps.gypi',
      ],
      'targets': [
        {
          'target_name': 'default_extensions',
          'type': 'none',
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
          'variables': { 'enable_wexit_time_destructors': 1, },
          'dependencies': [
            'chrome_resources.gyp:chrome_extra_resources',
            'chrome_resources.gyp:chrome_resources',
            'chrome_resources.gyp:chrome_strings',
            'chrome_resources.gyp:theme_resources',
            'common/extensions/api/api.gyp:api',
            '../base/base.gyp:base',
            '../content/content.gyp:content_browser',
            '../net/net.gyp:http_server',
            '../net/net.gyp:net',
            '../skia/skia.gyp:skia',
            '../third_party/icu/icu.gyp:icui18n',
            '../third_party/icu/icu.gyp:icuuc',
            '../third_party/leveldatabase/leveldatabase.gyp:leveldatabase',
          ],
          'include_dirs': [
            '..',
          ],
          'sources': [
            'browser/devtools/browser_list_tabcontents_provider.cc',
            'browser/devtools/browser_list_tabcontents_provider.h',
            'browser/devtools/devtools_file_helper.cc',
            'browser/devtools/devtools_file_helper.h',
            'browser/devtools/devtools_toggle_action.h',
            'browser/devtools/devtools_window.cc',
            'browser/devtools/devtools_window.h',
            'browser/devtools/remote_debugging_server.cc',
            'browser/devtools/remote_debugging_server.h',
          ],
          'conditions': [
            ['toolkit_uses_gtk == 1', {
              'dependencies': [
                '../build/linux/system.gyp:gtk',
              ],
            }],
            ['OS=="android"', {
              'sources!': [
                'browser/devtools/browser_list_tabcontents_provider.cc',
                'browser/devtools/devtools_window.cc',
                'browser/devtools/remote_debugging_server.cc',
              ],
            }],
            ['debug_devtools==1', {
              'defines': [
                'DEBUG_DEVTOOLS=1',
               ],
            }],
          ],
        },
        {
          'target_name': 'plugin',
          'type': 'static_library',
          'variables': { 'enable_wexit_time_destructors': 1, },
          'dependencies': [
            'chrome_resources.gyp:chrome_strings',
            '../base/base.gyp:base',
            '../content/content.gyp:content_plugin',
          ],
          'sources': [
            'plugin/chrome_content_plugin_client.cc',
            'plugin/chrome_content_plugin_client.h',
          ],
          'include_dirs': [
            '..',
            '<(grit_out_dir)',
          ],
        },
        {
          'target_name': 'utility',
          'type': 'static_library',
          'variables': { 'enable_wexit_time_destructors': 1, },
          'dependencies': [
            '../base/base.gyp:base',
            '../content/content.gyp:content_utility',
            '../skia/skia.gyp:skia',
          ],
          'sources': [
            'utility/chrome_content_utility_client.cc',
            'utility/chrome_content_utility_client.h',
            'utility/profile_import_handler.cc',
            'utility/profile_import_handler.h',
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
            ['OS=="android"', {
              'sources!': [
                'utility/profile_import_handler.cc',
              ],
            }],
          ],
        },
        {
          'target_name': 'service',
          'type': 'static_library',
          'variables': { 'enable_wexit_time_destructors': 1, },
          'dependencies': [
            'chrome_resources.gyp:chrome_strings',
            'common',
            'common_net',
            '../base/base.gyp:base',
            '../google_apis/google_apis.gyp:google_apis',
            '../jingle/jingle.gyp:notifier',
            '../net/net.gyp:net',
            '../printing/printing.gyp:printing',
            '../skia/skia.gyp:skia',
            '../third_party/libjingle/libjingle.gyp:libjingle',
          ],
          'sources': [
            'service/chrome_service_application_mac.h',
            'service/chrome_service_application_mac.mm',
            'service/service_ipc_server.cc',
            'service/service_ipc_server.h',
            'service/service_main.cc',
            'service/service_process.cc',
            'service/service_process.h',
            'service/service_process_prefs.cc',
            'service/service_process_prefs.h',
            'service/service_utility_process_host.cc',
            'service/service_utility_process_host.h',
            'service/cloud_print/cloud_print_auth.cc',
            'service/cloud_print/cloud_print_auth.h',
            'service/cloud_print/cloud_print_connector.cc',
            'service/cloud_print/cloud_print_connector.h',
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
            'service/cloud_print/cloud_print_wipeout.cc',
            'service/cloud_print/cloud_print_wipeout.h',
            'service/cloud_print/connector_settings.cc',
            'service/cloud_print/connector_settings.h',
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
          'variables': { 'enable_wexit_time_destructors': 1, },
          'dependencies': [
            'test_support_common',
            '../skia/skia.gyp:skia',
            '../sync/sync.gyp:sync_core',
          ],
          'include_dirs': [
             '..',
          ],
          'sources': [
            'tools/ipclist/ipclist.cc',
          ],
        },
      ],
    }],  # OS!="ios"
    ['OS=="mac"',
      { 'targets': [
        {
          'target_name': 'helper_app',
          'type': 'executable',
          'variables': { 'enable_wexit_time_destructors': 1, },
          'product_name': '<(mac_product_name) Helper',
          'mac_bundle': 1,
          'dependencies': [
            'chrome_dll',
            'infoplist_strings_tool',
          ],
          'sources': [
            # chrome_exe_main_mac.cc's main() is the entry point for
            # the "chrome" (browser app) target.  All it does is jump
            # to chrome_dll's ChromeMain.  This is appropriate for
            # helper processes too, because the logic to discriminate
            # between process types at run time is actually directed
            # by the --type command line argument processed by
            # ChromeMain.  Sharing chrome_exe_main_mac.cc with the
            # browser app will suffice for now.
            'app/chrome_exe_main_mac.cc',
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
          'postbuilds': [
            {
              # The helper doesn't have real localizations, it just has
              # empty .lproj directories, which is enough to convince Cocoa
              # that anything running out of the helper .app supports those
              # languages.
              'postbuild_name': 'Make Empty Localizations',
              'variables': {
                'locale_dirs': [
                  '>!@(<(apply_locales_cmd) -d ZZLOCALE.lproj <(locales))',
                ],
              },
              'action': [
                'tools/build/mac/make_locale_dirs.sh',
                '<@(locale_dirs)',
              ],
            },
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
              # targets.  In this case, --breakpad=0, --keystone=0, and --scm=0
              # are used because Breakpad, Keystone, and SCM keys are
              # never placed into the helper.
              'postbuild_name': 'Tweak Info.plist',
              'action': ['<(tweak_info_plist_path)',
                         '--breakpad=0',
                         '--keystone=0',
                         '--scm=0'],
            },
            {
              # Make sure there isn't any Objective-C in the helper app's
              # executable.
              'postbuild_name': 'Verify No Objective-C',
              'action': [
                '../build/mac/verify_no_objc.sh',
              ],
            },
          ],
          'conditions': [
            ['mac_breakpad==1', {
              'variables': {
                # A real .dSYM is needed for dump_syms to operate on.
                'mac_real_dsym': 1,
              },
              'xcode_settings': {
                # With mac_real_dsym set, strip_from_xcode won't be used.
                # Specify CHROMIUM_STRIP_SAVE_FILE directly to Xcode.
                'STRIPFLAGS': '-s $(CHROMIUM_STRIP_SAVE_FILE)',
              },
            }],
            ['asan==1', {
              'xcode_settings': {
                # Override the outer definition of CHROMIUM_STRIP_SAVE_FILE.
                'CHROMIUM_STRIP_SAVE_FILE': 'app/app_asan.saves',
              },
            }],
            ['component=="shared_library"', {
              'xcode_settings': {
                'LD_RUNPATH_SEARCH_PATHS': [
                  # Get back from Chromium.app/Contents/Versions/V/
                  #                                    Helper.app/Contents/MacOS
                  '@loader_path/../../../../../../..',
                ],
              },
            }],
          ],
        },  # target helper_app
        {
          # A library containing the actual code for the app mode app, shared
          # by unit tests.
          'target_name': 'app_mode_app_support',
          'type': 'static_library',
          'variables': { 'enable_wexit_time_destructors': 1, },
          'product_name': 'app_mode_app_support',
          'dependencies': [
            '../base/base.gyp:base',
          ],
          'sources': [
            'common/mac/app_mode_chrome_locator.h',
            'common/mac/app_mode_chrome_locator.mm',
            'common/mac/app_mode_common.h',
            'common/mac/app_mode_common.mm',
          ],
          'include_dirs': [
            '..',
          ],
        },  # target app_mode_app_support
        {
          # This produces the template for app mode loader bundles. It's a
          # template in the sense that parts of it need to be "filled in" by
          # Chrome before it can be executed.
          'target_name': 'app_mode_app',
          'type': 'executable',
          'mac_bundle' : 1,
          'variables': { 'enable_wexit_time_destructors': 1, },
          'product_name': 'app_mode_loader',
          'dependencies': [
            'app_mode_app_support',
            'infoplist_strings_tool',
          ],
          'sources': [
            'app/app_mode_loader_mac.mm',
            'app/app_mode-Info.plist',
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
          'mac_bundle_resources!': [
            'app/app_mode-Info.plist',
          ],
          'mac_bundle_resources/': [
            ['exclude', '.*'],
          ],
          'xcode_settings': {
            'INFOPLIST_FILE': 'app/app_mode-Info.plist',
            'APP_MODE_APP_BUNDLE_ID': '<(mac_bundle_id).app.@APP_MODE_SHORTCUT_ID@',
          },
          'postbuilds' : [
            {
              # Modify the Info.plist as needed.  The script explains why this
              # is needed.  This is also done in the chrome and chrome_dll
              # targets.  In this case, --breakpad=0, --keystone=0, and --scm=0
              # are used because Breakpad, Keystone, and SCM keys are
              # never placed into the app mode loader.
              'postbuild_name': 'Tweak Info.plist',
              'action': ['<(tweak_info_plist_path)',
                         '--breakpad=0',
                         '--keystone=0',
                         '--scm=0'],
            },
          ],
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
            'pkg_dmg_script_path': 'installer/mac/pkg-dmg',

            'conditions': [
              # This duplicates the output path from build_app_dmg.
              ['branding=="Chrome"', {
                'dmg_name': 'GoogleChrome.dmg',
              }, { # else: branding!="Chrome"
                'dmg_name': 'Chromium.dmg',
              }],
            ],
          },
          'actions': [
            {
              'inputs': [
                '<(build_app_dmg_script_path)',
                '<(pkg_dmg_script_path)',
                '<(PRODUCT_DIR)/<(mac_product_name).app',
              ],
              'outputs': [
                '<(PRODUCT_DIR)/<(dmg_name)',
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
          'variables': { 'enable_wexit_time_destructors': 1, },
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
          'variables': { 'enable_wexit_time_destructors': 1, },
          # This target must not depend on static libraries, else the code in
          # those libraries would appear twice in plugin processes: Once from
          # Chromium Framework, and once from this dylib.
          'dependencies': [
            'chrome_dll',
          ],
          'conditions': [
            ['component=="shared_library"', {
              'dependencies': [
                '../webkit/support/webkit_support.gyp:glue',
                '../content/content.gyp:content_plugin',
              ],
              'xcode_settings': {
                'LD_RUNPATH_SEARCH_PATHS': [
                  # Get back from Chromium.app/Contents/Versions/V
                  '@loader_path/../../../..',
                ],
              },
            }],
          ],
          'sources': [
            '../content/plugin/plugin_carbon_interpose_mac.cc',
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
            'DYLIB_INSTALL_NAME_BASE': '@executable_path/../../..',
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
          'variables': { 'enable_wexit_time_destructors': 1, },
          'dependencies': [
            'chrome_resources.gyp:chrome_strings',
            '../base/base.gyp:base',
            '../ui/ui.gyp:ui',
          ],
          'include_dirs': [
            '<(grit_out_dir)',
          ],
          'sources': [
            'tools/mac_helpers/infoplist_strings_util.mm',
          ],
        },
      ],  # targets
    }],  # OS=="mac"
    ['OS!="mac" and OS!="ios"', {
      'targets': [
        {
          'target_name': 'convert_dict',
          'type': 'executable',
          'variables': { 'enable_wexit_time_destructors': 1, },
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
          'variables': { 'enable_wexit_time_destructors': 1, },
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
          'dependencies': [
            '../base/base.gyp:base',
            '../base/base.gyp:test_support_base',
          ],
          'sources': [
            'tools/perf/flush_cache/flush_cache.cc',
          ],
        },
      ],
    }],  # OS!="mac" and OS!="ios"
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
                    '<(PRODUCT_DIR)/_pyautolib.so',
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
            'tools/ipclist/ipcfuzz.cc',
          ],
        },
      ],
    },],  # OS=="linux"
    ['OS=="win"',
      { 'targets': [
        {
          # For historical reasons, chrome/chrome.sln has been the entry point
          # for new Chrome developers. To assist development, include several
          # core unittests that are otherwise only accessible side-by-side with
          # chrome via all/all.sln.
          'target_name': 'test_targets',
          'type': 'none',
          'dependencies': [
            '../base/base.gyp:base_unittests',
            '../chrome_frame/chrome_frame.gyp:chrome_frame_tests',
            '../chrome_frame/chrome_frame.gyp:chrome_frame_net_tests',
            '../content/content.gyp:content_browsertests',
            '../content/content.gyp:content_shell',
            '../content/content.gyp:content_unittests',
            '../net/net.gyp:net_unittests',
            '../ui/ui.gyp:ui_unittests',
          ],
          'conditions': [
            ['use_aura==1', {
              'dependencies!': [
                '../chrome_frame/chrome_frame.gyp:chrome_frame_tests',
                '../chrome_frame/chrome_frame.gyp:chrome_frame_net_tests',
              ],
            }],
          ],
        },
        {
          'target_name': 'chrome_version_resources',
          'type': 'none',
          'conditions': [
            ['branding == "Chrome"', {
              'variables': {
                 'branding_path': 'app/theme/google_chrome/BRANDING',
              },
            }, { # else branding!="Chrome"
              'variables': {
                 'branding_path': 'app/theme/chromium/BRANDING',
              },
            }],
          ],
          'variables': {
            'output_dir': 'chrome_version',
            'template_input_path': 'app/chrome_version.rc.version',
          },
          'direct_dependent_settings': {
            'include_dirs': [
              '<(SHARED_INTERMEDIATE_DIR)/<(output_dir)',
            ],
          },
          'sources': [
            'app/app_host_exe.ver',
            'app/chrome_exe.ver',
            'app/chrome_dll.ver',
            'app/nacl64_exe.ver',
            'app/other.ver',
          ],
          'includes': [
            'version_resource_rules.gypi',
          ],
        },
        {
          'target_name': 'chrome_version_header',
          'type': 'none',
          'hard_dependency': 1,
          'actions': [
            {
              'action_name': 'version_header',
              'variables': {
                'lastchange_path':
                  '<(DEPTH)/build/util/LASTCHANGE',
              },
              'conditions': [
                ['branding == "Chrome"', {
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
          'dependencies': [
            'chrome_resources.gyp:theme_resources',
            '../skia/skia.gyp:skia',
          ],
          'include_dirs': [
            '..',
          ],
          'sources': [
             'test/automation/automation_handle_tracker.cc',
             'test/automation/automation_handle_tracker.h',
             'test/automation/automation_json_requests.cc',
             'test/automation/automation_json_requests.h',
             'test/automation/automation_proxy.cc',
             'test/automation/automation_proxy.h',
             'test/automation/browser_proxy.cc',
             'test/automation/browser_proxy.h',
             'test/automation/tab_proxy.cc',
             'test/automation/tab_proxy.h',
             'test/automation/value_conversion_traits.cc',
             'test/automation/value_conversion_traits.h',
             'test/automation/value_conversion_util.h',
             'test/automation/window_proxy.cc',
             'test/automation/window_proxy.h',
          ],
        },
        {
          'target_name': 'crash_service',
          'type': 'executable',
          'dependencies': [
            'app/policy/cloud_policy_codegen.gyp:policy',
            'installer_util',
            '../base/base.gyp:base',
            '../breakpad/breakpad.gyp:breakpad_handler',
            '../breakpad/breakpad.gyp:breakpad_sender',
            '../chrome/common_constants.gyp:common_constants',
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
          'target_name': 'sb_sigutil',
          'type': 'executable',
          'dependencies': [
            '../base/base.gyp:base',
            'safe_browsing_proto',
          ],
          'sources': [
            'browser/safe_browsing/signature_util.h',
            'browser/safe_browsing/signature_util_win.cc',
            'tools/safe_browsing/sb_sigutil.cc',
          ],
        },
      ]},  # 'targets'
    ],  # OS=="win"
    ['OS=="win" and target_arch=="ia32"',
      { 'targets': [
        {
          'target_name': 'crash_service_win64',
          'type': 'executable',
          'product_name': 'crash_service64',
          'dependencies': [
            'installer_util_nacl_win64',
            '../base/base.gyp:base_static_win64',
            '../breakpad/breakpad.gyp:breakpad_handler_win64',
            '../breakpad/breakpad.gyp:breakpad_sender_win64',
            '../chrome/common_constants.gyp:common_constants_win64',
          ],
          'include_dirs': [
            '..',
          ],
          'sources': [
            'tools/crash_service/crash_service.cc',
            'tools/crash_service/crash_service.h',
            'tools/crash_service/main.cc',
            '../content/public/common/content_switches.cc',
          ],
          'defines': [
            'COMPILE_CONTENT_STATICALLY',
          ],
          'msvs_settings': {
            'VCLinkerTool': {
              'SubSystem': '2',         # Set /SUBSYSTEM:WINDOWS
            },
          },
          'configurations': {
            'Common_Base': {
              'msvs_target_platform': 'x64',
            },
          },
        },
      ]},  # 'targets'
    ],  # OS=="win" and target_arch=="ia32"
    ['chromeos==1', {
      'includes': [ 'chrome_browser_chromeos.gypi' ],
    }],  # chromeos==1
    ['OS=="android"',
      {
      'targets': [
        {
          'target_name': 'chrome_java',
          'type': 'none',
          'dependencies': [
            '../base/base.gyp:base',
            '../components/components.gyp:navigation_interception_java',
            '../components/components.gyp:web_contents_delegate_android_java',
            '../content/content.gyp:content_java',
            '../sync/sync.gyp:sync_java',
            '../third_party/guava/guava.gyp:guava_javalib',
            '../ui/ui.gyp:ui_java',
          ],
          'variables': {
            'package_name': 'chrome',
            'java_in_dir': '../chrome/android/java',
            'has_java_resources': 1,
            'R_package': 'org.chromium.chrome',
            'R_package_relpath': 'org/chromium/chrome',
            'java_strings_grd': 'android_chrome_strings.grd',
          },
          'includes': [
            '../build/java.gypi',
          ],
        },
      ], # 'targets'
      'includes': [
        'chrome_android.gypi',
      ]}, # 'includes'
    ],  # OS=="android"
  ],  # 'conditions'
}
