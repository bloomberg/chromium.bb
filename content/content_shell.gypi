# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'content_shell_product_name': 'Content Shell',
    # The "19" is so that sites that sniff for version think that this is
    # something reasonably current; the "77.34.5" is a hint that this isn't a
    # standard Chrome.
    'content_shell_version': '19.77.34.5',
    'conditions': [
      ['OS=="linux"', {
       'use_custom_freetype%': 1,
      }, {
       'use_custom_freetype%': 0,
      }],
    ],
  },
  'targets': [
    {
      'target_name': 'content_shell_lib',
      'type': 'static_library',
      'defines!': ['CONTENT_IMPLEMENTATION'],
      'defines': ['CONTENT_SHELL_VERSION="<(content_shell_version)"'],
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
        'test_support_content',
        'content_resources.gyp:content_resources',
        '../base/base.gyp:base',
        '../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        '../build/temp_gyp/googleurl.gyp:googleurl',
        '../ipc/ipc.gyp:ipc',
        '../media/media.gyp:media',
        '../net/net.gyp:net',
        '../net/net.gyp:net_resources',
        '../skia/skia.gyp:skia',
        '../ui/gl/gl.gyp:gl',
        '../ui/ui.gyp:ui',
        '../v8/tools/gyp/v8.gyp:v8',
        '../webkit/support/webkit_support.gyp:webkit_resources',
        '../webkit/support/webkit_support.gyp:webkit_support',
        '<(webkit_src_dir)/Source/WebKit/chromium/WebKit.gyp:webkit',
        '<(webkit_src_dir)/Source/WebKit/chromium/WebKit.gyp:webkit_test_support',
        '<(webkit_src_dir)/Tools/DumpRenderTree/DumpRenderTree.gyp/DumpRenderTree.gyp:TestRunner',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'shell/android/shell_jni_registrar.cc',
        'shell/android/shell_jni_registrar.h',
        'shell/android/shell_manager.cc',
        'shell/android/shell_manager.h',
        'shell/geolocation/shell_access_token_store.cc',
        'shell/geolocation/shell_access_token_store.h',
        'shell/minimal_ash.cc',
        'shell/minimal_ash.h',
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
        'shell/shell_content_renderer_client.cc',
        'shell/shell_content_renderer_client.h',
        'shell/shell_devtools_delegate.cc',
        'shell/shell_devtools_delegate.h',
        'shell/shell_devtools_frontend.cc',
        'shell/shell_devtools_frontend.h',
        'shell/shell_download_manager_delegate.cc',
        'shell/shell_download_manager_delegate.h',
        'shell/shell_javascript_dialog_manager.cc',
        'shell/shell_javascript_dialog_manager.h',
        'shell/shell_javascript_dialog_gtk.cc',
        'shell/shell_javascript_dialog_mac.mm',
        'shell/shell_javascript_dialog_win.cc',
        'shell/shell_javascript_dialog.h',
        'shell/shell_login_dialog_gtk.cc',
        'shell/shell_login_dialog_mac.mm',
        'shell/shell_login_dialog.cc',
        'shell/shell_login_dialog.h',
        'shell/shell_main_delegate.cc',
        'shell/shell_main_delegate.h',
        'shell/shell_message_filter.cc',
        'shell/shell_message_filter.h',
        'shell/shell_messages.cc',
        'shell/shell_messages.h',
        'shell/shell_network_delegate.cc',
        'shell/shell_network_delegate.h',
        'shell/shell_quota_permission_context.cc',
        'shell/shell_quota_permission_context.h',
        'shell/shell_render_process_observer.cc',
        'shell/shell_render_process_observer.h',
        'shell/shell_resource_dispatcher_host_delegate.cc',
        'shell/shell_resource_dispatcher_host_delegate.h',
        'shell/shell_switches.cc',
        'shell/shell_switches.h',
        'shell/shell_test_configuration.cc',
        'shell/shell_test_configuration.h',
        'shell/shell_url_request_context_getter.cc',
        'shell/shell_url_request_context_getter.h',
        'shell/shell_web_contents_view_delegate_android.cc',
        'shell/shell_web_contents_view_delegate_creator.h',
        'shell/shell_web_contents_view_delegate_gtk.cc',
        'shell/shell_web_contents_view_delegate_mac.mm',
        'shell/shell_web_contents_view_delegate_win.cc',
        'shell/shell_web_contents_view_delegate.h',
        'shell/webkit_test_controller.cc',
        'shell/webkit_test_controller.h',
        'shell/webkit_test_helpers.cc',
        'shell/webkit_test_helpers.h',
        'shell/webkit_test_platform_support.h',
        'shell/webkit_test_platform_support_android.cc',
        'shell/webkit_test_platform_support_linux.cc',
        'shell/webkit_test_platform_support_mac.mm',
        'shell/webkit_test_platform_support_win.cc',
        'shell/webkit_test_runner.cc',
        'shell/webkit_test_runner.h',
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
          # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
          'msvs_disabled_warnings': [ 4267, ],
        }],  # OS=="win"
        ['OS=="linux"', {
          'dependencies': [
            '../build/linux/system.gyp:fontconfig',
          ],
        }],
        ['OS=="android"', {
          'dependencies': [
            'content_shell_jni_headers',
          ],
        }, {  # else: OS!="android"
          'dependencies': [
            # This dependency is for running DRT against the content shell, and
            # this combination is not yet supported on Android.
            '../webkit/support/webkit_support.gyp:webkit_support',
          ],
        }],  # OS=="android"
        ['os_posix==1 and use_aura==1 and linux_use_tcmalloc==1', {
          'dependencies': [
            # This is needed by content/app/content_main_runner.cc
            '../base/allocator/allocator.gyp:allocator',
          ],
        }],
        ['use_aura==1', {
          'dependencies': [
            '../ui/aura/aura.gyp:aura',
            '../ui/base/strings/ui_strings.gyp:ui_strings',
            '../ui/views/controls/webview/webview.gyp:webview',
            '../ui/views/views.gyp:views',
            '../ui/views/views.gyp:views_test_support',
            '../ui/ui.gyp:ui_resources',
          ],
          'sources/': [
            ['exclude', 'shell/shell_gtk.cc'],
            ['exclude', 'shell/shell_win.cc'],
          ],
        }],  # use_aura==1
        ['chromeos==1', {
          'dependencies': [
            '../ash/ash.gyp:ash',
            '../chromeos/chromeos.gyp:chromeos',
           ],
        }], # chromeos==1
        ['use_custom_freetype==1', {
          'dependencies': [
             '../third_party/freetype2/freetype2.gyp:freetype2',
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
        'browser/devtools/devtools_resources.gyp:devtools_resources',
        'content_shell_resources',
        '<(DEPTH)/ui/ui.gyp:ui_resources',
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
              '<(SHARED_INTERMEDIATE_DIR)/ui/app_locale_settings/app_locale_settings_en-US.pak',
              '<(SHARED_INTERMEDIATE_DIR)/ui/ui_resources/ui_resources_100_percent.pak',
              '<(SHARED_INTERMEDIATE_DIR)/ui/ui_resources/webui_resources.pak',
              '<(SHARED_INTERMEDIATE_DIR)/ui/ui_strings/ui_strings_en-US.pak',
              '<(SHARED_INTERMEDIATE_DIR)/webkit/devtools_resources.pak',
              '<(SHARED_INTERMEDIATE_DIR)/webkit/webkit_chromium_resources.pak',
              '<(SHARED_INTERMEDIATE_DIR)/webkit/webkit_resources_100_percent.pak',
              '<(SHARED_INTERMEDIATE_DIR)/webkit/webkit_strings_en-US.pak',
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
        '../third_party/mesa/mesa.gyp:osmesa',
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
                         '--scm=1',
                         '--version=<(content_shell_version)'],
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
    {
      'target_name': 'content_shell_builder',
      'type': 'none',
      'dependencies': [
        'content_shell',
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
          'copies': [
            {
              # Copy FFmpeg binaries for audio/video support.
              'destination': '<(PRODUCT_DIR)/$(CONTENTS_FOLDER_PATH)/Libraries',
              'files': [
                '<(PRODUCT_DIR)/ffmpegsumo.so',
              ],
            },
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
              # targets.  In this case, --breakpad=0, --keystone=0, and --scm=0
              # are used because Breakpad, Keystone, and SCM keys are
              # never placed into the helper.
              'postbuild_name': 'Tweak Info.plist',
              'action': ['../build/mac/tweak_info_plist.py',
                         '--breakpad=0',
                         '--keystone=0',
                         '--scm=0',
                         '--version=<(content_shell_version)'],
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
            ['component=="shared_library"', {
              'xcode_settings': {
                'LD_RUNPATH_SEARCH_PATHS': [
                  # Get back from Content Shell.app/Contents/Frameworks/
                  #                                 Helper.app/Contents/MacOS
                  '@loader_path/../../../../../..',
                ],
              },
            }],
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
          'sources': [
            'shell/android/browsertests_apk/src/org/chromium/content_browsertests_apk/BrowserTestSystemMessageHandler.java',
            'shell/android/browsertests_apk/src/org/chromium/content_browsertests_apk/ContentBrowserTestsActivity.java',
            'shell/android/java/src/org/chromium/content_shell/ShellManager.java',
            'shell/android/java/src/org/chromium/content_shell/Shell.java',
          ],
          'direct_dependent_settings': {
            'include_dirs': [
              '<(SHARED_INTERMEDIATE_DIR)/content/shell',
            ],
          },
          'variables': {
            'jni_gen_package': 'content/shell',
          },
          'includes': [ '../build/jni_generator.gypi' ],
        },
        {
          'target_name': 'libcontent_shell_content_view',
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
          'sources': [
            'shell/android/shell_library_loader.cc',
            'shell/android/shell_library_loader.h',
          ],
          'conditions': [
            ['android_webview_build==1', {
              'ldflags': [
                '-lgabi++',  # For rtti
              ],
            }],
          ],
        },
        {
          'target_name': 'content_shell_java',
          'type': 'none',
          'dependencies': [
            'content_java',
          ],
          'variables': {
            'java_in_dir': '../content/shell/android/java',
            'has_java_resources': 1,
            'R_package': 'org.chromium.content_shell',
            'R_package_relpath': 'org/chromium/content_shell',
          },
          'includes': [ '../build/java.gypi' ],
        },
        {
          # content_shell_apk creates a .jar as a side effect. Any java targets
          # that need that .jar in their classpath should depend on this target,
          # content_shell_apk_java. Dependents of content_shell_apk receive its
          # jar path in the variable 'apk_output_jar_path'. This target should
          # only be used by targets which instrument content_shell_apk.
          'target_name': 'content_shell_apk_java',
          'type': 'none',
          'dependencies': [
            'content_shell_apk',
          ],
          # This all_dependent_settings is used for java targets only. This will
          # add the content_shell jar to the classpath of dependent java
          # targets.
          'all_dependent_settings': {
            'variables': {
              'input_jars_paths': ['>(apk_output_jar_path)'],
            },
          },
          # Add an action with the appropriate output. This allows the generated
          # buildfiles to determine which target the output corresponds to.
          'actions': [
            {
              'action_name': 'fake_generate_jar',
              'inputs': [],
              'outputs': ['>(apk_output_jar_path)'],
              'action': [],
            },
          ],
        },
        {
          'target_name': 'content_shell_apk',
          'type': 'none',
          'dependencies': [
            'content_java',
            'content_shell_java',
            'libcontent_shell_content_view',
            '../base/base.gyp:base_java',
            '../media/media.gyp:media_java',
            '../net/net.gyp:net_java',
            '../ui/ui.gyp:ui_java',
          ],
          'variables': {
            'apk_name': 'ContentShell',
            'manifest_package_name': 'org.chromium.content_shell_apk',
            'java_in_dir': 'shell/android/shell_apk',
            'resource_dir': 'shell/android/shell_apk/res',
            'native_libs_paths': ['<(SHARED_LIB_DIR)/libcontent_shell_content_view.so'],
            'additional_input_paths': ['<(PRODUCT_DIR)/content_shell/assets/content_shell.pak'],
            'asset_location': '<(ant_build_out)/content_shell/assets',
          },
          'includes': [ '../build/java_apk.gypi' ],
        },
      ],
    }],  # OS=="android"
  ]
}
