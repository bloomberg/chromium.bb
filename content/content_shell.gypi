# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'content_shell_product_name': 'Content Shell',
  },
  'targets': [
    {
      'target_name': 'content_shell_lib',
      'type': 'static_library',
      'defines!': ['CONTENT_IMPLEMENTATION'],
      'variables': {
        'chromium_code': 1,
      },
      'dependencies': [
        'content_app',
        'content_browser',
        'content_common',
        'content_gpu',
        'content_plugin',
        'content_ppapi_plugin',
        'content_renderer',
        'content_shell_resources',
        'content_utility',
        'content_worker',
        'content_resources.gyp:content_resources',
        '../base/base.gyp:base',
        '../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        '../build/temp_gyp/googleurl.gyp:googleurl',
        '../ipc/ipc.gyp:ipc',
        '../media/media.gyp:media',
        '../net/net.gyp:net',
        '../skia/skia.gyp:skia',
        '../third_party/WebKit/Source/WebKit/chromium/WebKit.gyp:webkit',
        '../ui/ui.gyp:ui',
        '../v8/tools/gyp/v8.gyp:v8',
        '../webkit/support/webkit_support.gyp:appcache',
        '../webkit/support/webkit_support.gyp:database',
        '../webkit/support/webkit_support.gyp:fileapi',
        '../webkit/support/webkit_support.gyp:glue',
        '../webkit/support/webkit_support.gyp:quota',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'shell/layout_test_controller_bindings.cc',
        'shell/layout_test_controller_bindings.h',
        'shell/paths_mac.h',
        'shell/paths_mac.mm',
        'shell/shell.cc',
        'shell/shell.h',
        'shell/shell_android.cc',
        'shell/shell_aura.cc',
        'shell/shell_gtk.cc',
        'shell/shell_mac.mm',
        'shell/shell_win.cc',
        'shell/shell_application_mac.h',
        'shell/shell_application_mac.mm',
        'shell/shell_browser_context.cc',
        'shell/shell_browser_context.h',
        'shell/shell_browser_main.cc',
        'shell/shell_browser_main.h',
        'shell/shell_browser_main_parts.cc',
        'shell/shell_browser_main_parts.h',
        'shell/shell_browser_main_parts_mac.mm',
        'shell/shell_content_browser_client.cc',
        'shell/shell_content_browser_client.h',
        'shell/shell_content_client.cc',
        'shell/shell_content_client.h',
        'shell/shell_content_plugin_client.cc',
        'shell/shell_content_plugin_client.h',
        'shell/shell_content_renderer_client.cc',
        'shell/shell_content_renderer_client.h',
        'shell/shell_content_utility_client.cc',
        'shell/shell_content_utility_client.h',
        'shell/shell_devtools_delegate.cc',
        'shell/shell_devtools_delegate.h',
        'shell/shell_download_manager_delegate.cc',
        'shell/shell_download_manager_delegate.h',
        'shell/shell_javascript_dialog_creator.cc',
        'shell/shell_javascript_dialog_creator.h',
        'shell/shell_javascript_dialog_mac.mm',
        'shell/shell_javascript_dialog_win.cc',
        'shell/shell_javascript_dialog.h',
        'shell/shell_login_dialog_mac.mm',
        'shell/shell_login_dialog.cc',
        'shell/shell_login_dialog.h',
        'shell/shell_main_delegate.cc',
        'shell/shell_main_delegate.h',
        'shell/shell_messages.cc',
        'shell/shell_messages.h',
        'shell/shell_network_delegate.cc',
        'shell/shell_network_delegate.h',
        'shell/shell_render_process_observer.cc',
        'shell/shell_render_process_observer.h',
        'shell/shell_render_view_host_observer.cc',
        'shell/shell_render_view_host_observer.h',
        'shell/shell_render_view_observer.cc',
        'shell/shell_render_view_observer.h',
        'shell/shell_resource_context.cc',
        'shell/shell_resource_context.h',
        'shell/shell_resource_dispatcher_host_delegate.cc',
        'shell/shell_resource_dispatcher_host_delegate.h',
        'shell/shell_switches.cc',
        'shell/shell_switches.h',
        'shell/shell_url_request_context_getter.cc',
        'shell/shell_url_request_context_getter.h',
      ],
      'msvs_settings': {
        'VCLinkerTool': {
          'SubSystem': '2',  # Set /SUBSYSTEM:WINDOWS
        },
      },
      'conditions': [
        ['OS=="win" and win_use_allocator_shim==1', {
          'dependencies': [
            '../base/allocator/allocator.gyp:allocator',
          ],
        }],
        ['OS=="win"', {
          'resource_include_dirs': [
            '<(SHARED_INTERMEDIATE_DIR)/webkit',
          ],
          'dependencies': [
            '<(DEPTH)/webkit/support/webkit_support.gyp:webkit_resources',
            '<(DEPTH)/webkit/support/webkit_support.gyp:webkit_strings',
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
        }],  # OS=="win"
        ['OS!="android"', {
          'dependencies': [
            # This dependency is for running DRT against the content shell, and
            # this combination is not yet supported on Android.
            '../webkit/support/webkit_support.gyp:webkit_support',
          ],
        }],  # OS!="android"
        ['use_aura==1', {
          'sources/': [
            ['exclude', 'shell/shell_gtk.cc'],
            ['exclude', 'shell/shell_win.cc'],
          ],
        }],
      ],
    },
    {
      'target_name': 'content_shell_resources',
      'type': 'none',
      'dependencies': [
        'generate_content_shell_resources',
      ],
      'variables': {
        'grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)/content',
      },
      'includes': [ '../build/grit_target.gypi' ],
      'copies': [
        {
          'destination': '<(PRODUCT_DIR)',
          'files': [
            '<(SHARED_INTERMEDIATE_DIR)/content/shell_resources.pak'
          ],
        },
      ],
    },
    {
      'target_name': 'generate_content_shell_resources',
      'type': 'none',
      'variables': {
        'grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)/content',
      },
      'actions': [
        {
          'action_name': 'content_shell_resources',
          'variables': {
            'grit_grd_file': 'shell/shell_resources.grd',
          },
          'includes': [ '../build/grit_action.gypi' ],
        },
      ],
    },
    {
      # We build a minimal set of resources so WebKit in content_shell has
      # access to necessary resources.
      'target_name': 'content_shell_pak',
      'type': 'none',
      'dependencies': [
        'browser/debugger/devtools_resources.gyp:devtools_resources',
        'content_shell_resources',
        '<(DEPTH)/net/net.gyp:net_resources',
        '<(DEPTH)/ui/ui.gyp:ui_resources',
        '<(DEPTH)/ui/ui.gyp:ui_resources_standard',
      ],
      'variables': {
        'repack_path': '<(DEPTH)/tools/grit/grit/format/repack.py',
      },
      'actions': [
        {
          'action_name': 'repack_content_shell_pack',
          'variables': {
            'pak_inputs': [
              '<(SHARED_INTERMEDIATE_DIR)/content/content_resources.pak',
              '<(SHARED_INTERMEDIATE_DIR)/content/shell_resources.pak',
              '<(SHARED_INTERMEDIATE_DIR)/net/net_resources.pak',
              '<(SHARED_INTERMEDIATE_DIR)/ui/ui_resources/ui_resources.pak',
              '<(SHARED_INTERMEDIATE_DIR)/ui/ui_resources_standard/ui_resources_standard.pak',
              '<(SHARED_INTERMEDIATE_DIR)/webkit/devtools_resources.pak',
              '<(SHARED_INTERMEDIATE_DIR)/webkit/webkit_chromium_resources.pak',
              '<(SHARED_INTERMEDIATE_DIR)/webkit/webkit_resources.pak',
            ],
            'conditions': [
              ['OS != "mac"', {
                'pak_inputs': [
                  '<(SHARED_INTERMEDIATE_DIR)/ui/native_theme/native_theme_resources.pak',
                ]
              }],
            ],
          },
          'inputs': [
            '<(repack_path)',
            '<@(pak_inputs)',
          ],
          'action': ['python', '<(repack_path)', '<@(_outputs)',
                     '<@(pak_inputs)'],
          'conditions': [
            ['OS!="android"', {
              'outputs': [
                '<(PRODUCT_DIR)/content_shell.pak',
              ],
            }, {
              'outputs': [
                '<(PRODUCT_DIR)/content_shell/assets/content_shell.pak',
              ],
            }],
          ],
        },
      ],
    },
    {
      'target_name': 'content_shell',
      'type': 'executable',
      'mac_bundle': 1,
      'defines!': ['CONTENT_IMPLEMENTATION'],
      'variables': {
        'chromium_code': 1,
      },
      'dependencies': [
        'content_shell_lib',
        'content_shell_pak',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'app/startup_helper_win.cc',
        'shell/shell_main.cc',
      ],
      'mac_bundle_resources': [
        'shell/mac/app.icns',
        'shell/mac/app-Info.plist',
      ],
      # TODO(mark): Come up with a fancier way to do this.  It should only
      # be necessary to list app-Info.plist once, not the three times it is
      # listed here.
      'mac_bundle_resources!': [
        'shell/mac/app-Info.plist',
      ],
      'xcode_settings': {
        'INFOPLIST_FILE': 'shell/mac/app-Info.plist',
      },
      'msvs_settings': {
        'VCLinkerTool': {
          'SubSystem': '2',  # Set /SUBSYSTEM:WINDOWS
        },
      },
      'conditions': [
        ['OS=="win" and win_use_allocator_shim==1', {
          'dependencies': [
            '../base/allocator/allocator.gyp:allocator',
          ],
        }],
        ['OS=="win"', {
          'sources': [
            'shell/shell.rc',
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
        }],  # OS=="win"
        ['OS == "win" or (toolkit_uses_gtk == 1 and selinux == 0)', {
          'dependencies': [
            '../sandbox/sandbox.gyp:sandbox',
          ],
        }],  # OS=="win" or (toolkit_uses_gtk == 1 and selinux == 0)
        ['toolkit_uses_gtk == 1', {
          'dependencies': [
            '<(DEPTH)/build/linux/system.gyp:gtk',
          ],
        }],  # toolkit_uses_gtk
        ['OS=="mac"', {
          'product_name': '<(content_shell_product_name)',
          'dependencies!': [
            'content_shell_lib',
          ],
          'dependencies': [
            'content_shell_framework',
            'content_shell_helper_app',
          ],
          'copies': [
            {
              'destination': '<(PRODUCT_DIR)/<(content_shell_product_name).app/Contents/Frameworks',
              'files': [
                '<(PRODUCT_DIR)/<(content_shell_product_name) Helper.app',
              ],
            },
          ],
          'postbuilds': [
            {
              'postbuild_name': 'Copy <(content_shell_product_name) Framework.framework',
              'action': [
                '../build/mac/copy_framework_unversioned.sh',
                '${BUILT_PRODUCTS_DIR}/<(content_shell_product_name) Framework.framework',
                '${BUILT_PRODUCTS_DIR}/${CONTENTS_FOLDER_PATH}/Frameworks',
              ],
            },
            {
              'postbuild_name': 'Fix Framework Link',
              'action': [
                'install_name_tool',
                '-change',
                '/Library/Frameworks/<(content_shell_product_name) Framework.framework/Versions/A/<(content_shell_product_name) Framework',
                '@executable_path/../Frameworks/<(content_shell_product_name) Framework.framework/<(content_shell_product_name) Framework',
                '${BUILT_PRODUCTS_DIR}/${EXECUTABLE_PATH}'
              ],
            },
            {
              # Modify the Info.plist as needed.
              'postbuild_name': 'Tweak Info.plist',
              'action': ['../build/mac/tweak_info_plist.py',
                         '--svn=1'],
            },
            {
              # This postbuid step is responsible for creating the following
              # helpers:
              #
              # Content Shell Helper EH.app and Content Shell Helper NP.app are
              # created from Content Shell Helper.app.
              #
              # The EH helper is marked for an executable heap. The NP helper
              # is marked for no PIE (ASLR).
              'postbuild_name': 'Make More Helpers',
              'action': [
                '../build/mac/make_more_helpers.sh',
                'Frameworks',
                '<(content_shell_product_name)',
              ],
            },
            {
              # Make sure there isn't any Objective-C in the shell's
              # executable.
              'postbuild_name': 'Verify No Objective-C',
              'action': [
                '../build/mac/verify_no_objc.sh',
              ],
            },
          ],
        }],  # OS=="mac"
      ],
    },
  ],
  'conditions': [
    ['OS=="mac"', {
      'targets': [
        {
          'target_name': 'content_shell_framework',
          'type': 'shared_library',
          'product_name': '<(content_shell_product_name) Framework',
          'mac_bundle': 1,
          'mac_bundle_resources': [
            'shell/mac/English.lproj/HttpAuth.xib',
            'shell/mac/English.lproj/MainMenu.xib',
            '<(PRODUCT_DIR)/content_shell.pak'
          ],
          'dependencies': [
            'content_shell_lib',
          ],
          'include_dirs': [
            '..',
          ],
          'sources': [
            'shell/shell_content_main.cc',
            'shell/shell_content_main.h',
          ],
        },  # target content_shell_framework
        {
          'target_name': 'content_shell_helper_app',
          'type': 'executable',
          'variables': { 'enable_wexit_time_destructors': 1, },
          'product_name': '<(content_shell_product_name) Helper',
          'mac_bundle': 1,
          'dependencies': [
            'content_shell_framework',
          ],
          'sources': [
            'shell/shell_main.cc',
            'shell/mac/helper-Info.plist',
          ],
          # TODO(mark): Come up with a fancier way to do this.  It should only
          # be necessary to list helper-Info.plist once, not the three times it
          # is listed here.
          'mac_bundle_resources!': [
            'shell/mac/helper-Info.plist',
          ],
          # TODO(mark): For now, don't put any resources into this app.  Its
          # resources directory will be a symbolic link to the browser app's
          # resources directory.
          'mac_bundle_resources/': [
            ['exclude', '.*'],
          ],
          'xcode_settings': {
            'INFOPLIST_FILE': 'shell/mac/helper-Info.plist',
          },
          'postbuilds': [
            {
              # The framework defines its load-time path
              # (DYLIB_INSTALL_NAME_BASE) relative to the main executable
              # (chrome).  A different relative path needs to be used in
              # content_shell_helper_app.
              'postbuild_name': 'Fix Framework Link',
              'action': [
                'install_name_tool',
                '-change',
                '/Library/Frameworks/<(content_shell_product_name) Framework.framework/Versions/A/<(content_shell_product_name) Framework',
                '@executable_path/../../../../Frameworks/<(content_shell_product_name) Framework.framework/<(content_shell_product_name) Framework',
                '${BUILT_PRODUCTS_DIR}/${EXECUTABLE_PATH}'
              ],
            },
            {
              # Modify the Info.plist as needed.  The script explains why this
              # is needed.  This is also done in the chrome and chrome_dll
              # targets.  In this case, --breakpad=0, --keystone=0, and --svn=0
              # are used because Breakpad, Keystone, and Subversion keys are
              # never placed into the helper.
              'postbuild_name': 'Tweak Info.plist',
              'action': ['../build/mac/tweak_info_plist.py',
                         '--breakpad=0',
                         '--keystone=0',
                         '--svn=0'],
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
        },  # target content_shell_helper_app
      ],
    }],  # OS=="mac"
    ['OS=="android"', {
      'targets': [
        {
          # TODO(jrg): Update this action and other jni generators to only
          # require specifying the java directory and generate the rest.
          'target_name': 'content_shell_jni_headers',
          'type': 'none',
          'variables': {
            'java_sources': [
              'shell/android/java/org/chromium/content_shell/ShellManager.java',
              'shell/android/java/org/chromium/content_shell/ShellView.java',
            ],
            'jni_headers': [
              '<(SHARED_INTERMEDIATE_DIR)/content/shell/jni/shell_manager_jni.h',
              '<(SHARED_INTERMEDIATE_DIR)/content/shell/jni/shell_view_jni.h',
            ],
          },
          'includes': [ '../build/jni_generator.gypi' ],
        },
        {
          'target_name': 'content_shell_content_view',
          'type': 'shared_library',
          'dependencies': [
            'content_shell_jni_headers',
            'content_shell_lib',
            'content_shell_pak',
            # Skia is necessary to ensure the dependencies needed by
            # WebContents are included.
            '../skia/skia.gyp:skia',
            '<(DEPTH)/media/media.gyp:player_android',
          ],
          'include_dirs': [
            '<(SHARED_INTERMEDIATE_DIR)/content/shell',
          ],
          'sources': [
            'shell/android/shell_library_loader.cc',
            'shell/android/shell_library_loader.h',
            'shell/android/shell_manager.cc',
            'shell/android/shell_manager.h',
            'shell/android/shell_view.cc',
            'shell/android/shell_view.h',
          ],
        },
        {
          'target_name': 'content_shell_apk',
          'type': 'none',
          'actions': [
            {
              'action_name': 'copy_base_jar',
              'inputs': ['<(PRODUCT_DIR)/lib.java/chromium_base.jar'],
              'outputs': ['<(PRODUCT_DIR)/content_shell/java/libs/chromium_base.jar'],
              'action': ['cp', '<@(_inputs)', '<@(_outputs)'],
            },
            {
              'action_name': 'copy_net_jar',
              'inputs': ['<(PRODUCT_DIR)/lib.java/chromium_net.jar'],
              'outputs': ['<(PRODUCT_DIR)/content_shell/java/libs/chromium_net.jar'],
              'action': ['cp', '<@(_inputs)', '<@(_outputs)'],
            },
            {
              'action_name': 'copy_media_jar',
              'inputs': ['<(PRODUCT_DIR)/lib.java/chromium_media.jar'],
              'outputs': ['<(PRODUCT_DIR)/content_shell/java/libs/chromium_media.jar'],
              'action': ['cp', '<@(_inputs)', '<@(_outputs)'],
            },
            {
              'action_name': 'copy_content_jar',
              'inputs': ['<(PRODUCT_DIR)/lib.java/chromium_content.jar'],
              'outputs': ['<(PRODUCT_DIR)/content_shell/java/libs/chromium_content.jar'],
              'action': ['cp', '<@(_inputs)', '<@(_outputs)'],
            },
            {
              'action_name': 'copy_and_strip_so',
              'inputs': ['<(SHARED_LIB_DIR)/libcontent_shell_content_view.so'],
              'outputs': ['<(PRODUCT_DIR)/content_shell/libs/<(android_app_abi)/libcontent_shell_content_view.so'],
              'action': [
                '<!(/bin/echo -n $STRIP)',
                '--strip-unneeded',  # All symbols not needed for relocation.
                '<@(_inputs)',
                '-o',
                '<@(_outputs)' 
              ],
            },
            {
              'action_name': 'content_shell_apk',
              'inputs': [
                '<(DEPTH)/content/shell/android/content_shell_apk.xml',
                '<(DEPTH)/content/shell/android/AndroidManifest.xml',
                '<!@(find shell/android/java -name "*.java")',
                '<!@(find shell/android/res -name "*")',
                '<(PRODUCT_DIR)/content_shell/java/libs/chromium_base.jar',
                '<(PRODUCT_DIR)/content_shell/java/libs/chromium_net.jar',
                '<(PRODUCT_DIR)/content_shell/java/libs/chromium_media.jar',
                '<(PRODUCT_DIR)/content_shell/java/libs/chromium_content.jar',
                '<(PRODUCT_DIR)/content_shell/libs/<(android_app_abi)/libcontent_shell_content_view.so',
              ],
              'outputs': [
                # Awkwardly, we build a Debug APK even when gyp is in
                # Release mode.  I don't think it matters (e.g. we're
                # probably happy to not codesign) but naming should be
                # fixed.
                '<(PRODUCT_DIR)/ContentShell-debug.apk',
              ],
              'action': [
                'ant',
                '-DPRODUCT_DIR=<(PRODUCT_DIR)',
                '-DAPP_ABI=<(android_app_abi)',
                '-buildfile',
                '<(DEPTH)/content/shell/android/content_shell_apk.xml',
              ]
            }
          ],
        },
      ],
    }],  # OS=="android"
  ]
}
