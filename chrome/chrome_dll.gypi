# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'conditions': [
    ['OS=="mac" or OS=="win"', {
      'targets': [
        {
          'target_name': 'chrome_dll',
          'type': 'none',
          'dependencies': [
            'chrome_main_dll',
          ],
          'conditions': [
            ['OS=="mac" and component=="shared_library"', {
              'type': 'shared_library',
              'includes': [ 'chrome_dll_bundle.gypi' ],
              'xcode_settings': {
                'OTHER_LDFLAGS': [
                  '-Wl,-reexport_library,<(PRODUCT_DIR)/libchrome_main_dll.dylib',
                ],
              },
            }],  # OS=="mac"
            ['incremental_chrome_dll==1', {
              # Linking to a different directory and then hardlinking back
              # to OutDir is a workaround to avoid having the .ilk for
              # chrome.exe and chrome.dll conflicting. See crbug.com/92528
              # for more information. Done on the dll instead of the exe so
              # that people launching from VS don't need to modify
              # $(TargetPath) for the exe.
              'actions': [
                {
                  'action_name': 'hardlink_to_output',
                  'inputs': [
                    '$(OutDir)\\initial\\chrome.dll',
                  ],
                  'outputs': [
                    '$(OutDir)\\chrome.dll',
                  ],
                  'action': ['tools\\build\\win\\hardlink_failsafe.bat',
                             '$(OutDir)\\initial\\chrome.dll',
                             '$(OutDir)\\chrome.dll'],
                  'msvs_cygwin_shell': 0,
                },
              ],
              'conditions': [
                # Only hardlink pdb if we're generating debug info.
                ['fastbuild==0 or win_z7!=0', {
                  'actions': [
                    {
                      'action_name': 'hardlink_pdb_to_output',
                      'inputs': [
                        # Not the pdb, since gyp doesn't know about it
                        '$(OutDir)\\initial\\chrome.dll',
                      ],
                      'outputs': [
                        '$(OutDir)\\chrome.dll.pdb',
                      ],
                      'action': ['tools\\build\\win\\hardlink_failsafe.bat',
                                 '$(OutDir)\\initial\\chrome.dll.pdb',
                                 '$(OutDir)\\chrome.dll.pdb'],
                      'msvs_cygwin_shell': 0,
                    }
                  ]
                }]
              ],
            }],
          ]
        },
        {
          'target_name': 'chrome_main_dll',
          'type': 'shared_library',
          'variables': {
            'enable_wexit_time_destructors': 1,
          },
          'dependencies': [
            '<@(chromium_dependencies)',
            'app/policy/cloud_policy_codegen.gyp:policy',
          ],
          'conditions': [
            ['use_aura==1', {
              'dependencies': [
                '../ui/compositor/compositor.gyp:compositor',
              ],
            }],
            ['use_ash==1', {
              'sources': [
                '<(SHARED_INTERMEDIATE_DIR)/ash/ash_resources/ash_wallpaper_resources.rc',
              ],
            }],
            ['OS=="win" and target_arch=="ia32"', {
              # Add a dependency to custom import library for user32 delay
              # imports only in x86 builds.
              'dependencies': [
                'chrome_user32_delay_imports',
              ],
            },],
            ['OS=="win"', {
              'product_name': 'chrome',
              'dependencies': [
                # On Windows, link the dependencies (libraries) that make
                # up actual Chromium functionality into this .dll.
                'chrome_dll_pdb_workaround',
                'chrome_resources.gyp:chrome_resources',
                'chrome_version_resources',
                '../chrome/chrome_resources.gyp:chrome_unscaled_resources',
                '../content/content.gyp:content_worker',
                '../crypto/crypto.gyp:crypto',
                '../printing/printing.gyp:printing',
                '../net/net.gyp:net_resources',
                '../third_party/cld/cld.gyp:cld',
                '../ui/views/views.gyp:views',
                '../webkit/support/webkit_support.gyp:webkit_resources',
              ],
              'sources': [
                'app/chrome_command_ids.h',
                'app/chrome_dll.rc',
                'app/chrome_dll_resource.h',
                'app/chrome_main.cc',
                'app/chrome_main_delegate.cc',
                'app/chrome_main_delegate.h',
                'app/delay_load_hook_win.cc',
                'app/delay_load_hook_win.h',

                '<(SHARED_INTERMEDIATE_DIR)/chrome_version/chrome_dll_version.rc',
                '../base/win/dllmain.cc',

                '../ui/resources/cursors/aliasb.cur',
                '../ui/resources/cursors/cell.cur',
                '../ui/resources/cursors/col_resize.cur',
                '../ui/resources/cursors/copy.cur',
                '../ui/resources/cursors/none.cur',
                '../ui/resources/cursors/row_resize.cur',
                '../ui/resources/cursors/vertical_text.cur',
                '../ui/resources/cursors/zoom_in.cur',
                '../ui/resources/cursors/zoom_out.cur',

                # TODO:  It would be nice to have these pulled in
                # automatically from direct_dependent_settings in
                # their various targets (net.gyp:net_resources, etc.),
                # but that causes errors in other targets when
                # resulting .res files get referenced multiple times.
                '<(SHARED_INTERMEDIATE_DIR)/chrome/browser_resources.rc',
                '<(SHARED_INTERMEDIATE_DIR)/chrome/chrome_unscaled_resources.rc',
                '<(SHARED_INTERMEDIATE_DIR)/chrome/common_resources.rc',
                '<(SHARED_INTERMEDIATE_DIR)/chrome/extensions_api_resources.rc',
                '<(SHARED_INTERMEDIATE_DIR)/content/content_resources.rc',
                '<(SHARED_INTERMEDIATE_DIR)/net/net_resources.rc',
                '<(SHARED_INTERMEDIATE_DIR)/ui/ui_resources/ui_unscaled_resources.rc',
                '<(SHARED_INTERMEDIATE_DIR)/webkit/webkit_chromium_resources.rc',
              ],
              'include_dirs': [
                '<(DEPTH)/third_party/wtl/include',
              ],
              'defines': [
                'CHROME_DLL',
                'BROWSER_DLL',
                'RENDERER_DLL',
                'PLUGIN_DLL',
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
              'msvs_settings': {
                'VCLinkerTool': {
                  'BaseAddress': '0x01c30000',
                  'ImportLibrary': '$(OutDir)\\lib\\chrome_dll.lib',
                  # Set /SUBSYSTEM:WINDOWS for chrome.dll (for consistency).
                  'SubSystem': '2',
                  'conditions': [
                    ['incremental_chrome_dll==1', {
                      'OutputFile': '$(OutDir)\\initial\\chrome.dll',
                      'UseLibraryDependencyInputs': "true",
                    }],
                    ['target_arch=="ia32"', {
                      # Link against the XP-constrained user32 import library
                      # instead of the platform-SDK provided one to avoid
                      # inadvertently taking dependencies on post-XP user32
                      # exports.
                      'AdditionalDependencies!': [
                        'user32.lib',
                      ],
                      'IgnoreDefaultLibraryNames': [
                        'user32.lib',
                      ],
                      # Remove user32 delay load for chrome.dll.
                      'DelayLoadDLLs!': [
                        'user32.dll',
                      ],
                      'AdditionalDependencies': [
                        'user32.winxp.lib',
                      ],
                      'DelayLoadDLLs': [
                        'user32-delay.dll',
                      ],
                      'AdditionalLibraryDirectories': [
                        '<(DEPTH)/build/win/importlibs/x86',
                      ],
                      'ForceSymbolReferences': [
                        # Force the inclusion of the delay load hook in this
                        # binary.
                        '_ChromeDelayLoadHook@8',
                      ],
                    }],
                  ],
                  'DelayLoadDLLs': [
                    'comdlg32.dll',
                    'crypt32.dll',
                    'cryptui.dll',
                    'dhcpcsvc.dll',
                    'imagehlp.dll',
                    'imm32.dll',
                    'iphlpapi.dll',
                    'setupapi.dll',
                    'urlmon.dll',
                    'winhttp.dll',
                    'wininet.dll',
                    'winspool.drv',
                    'ws2_32.dll',
                    'wsock32.dll',
                  ],
                },
                'VCManifestTool': {
                  'AdditionalManifestFiles': '$(ProjectDir)\\app\\chrome.dll.manifest',
                },
              },
            }],  # OS=="win"
            ['OS=="mac" and component!="shared_library"', {
              'includes': [ 'chrome_dll_bundle.gypi' ],
            }],
            ['OS=="mac" and component=="shared_library"', {
              'xcode_settings': { 'OTHER_LDFLAGS': [ '-Wl,-ObjC' ], },
            }],
            ['OS=="mac"', {
              'xcode_settings': {
                # Define the order of symbols within the framework.  This
                # sets -order_file.
                'ORDER_FILE': 'app/framework.order',
              },
              'sources': [
                'app/chrome_command_ids.h',
                'app/chrome_dll_resource.h',
                'app/chrome_main.cc',
                'app/chrome_main_delegate.cc',
                'app/chrome_main_delegate.h',
                'app/chrome_main_app_mode_mac.mm',
                'app/chrome_main_mac.mm',
                'app/chrome_main_mac.h',
              ],
              'include_dirs': [
                '<(grit_out_dir)',
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
              ],
              'conditions': [
                ['mac_breakpad_compiled_in==1', {
                  'dependencies': [
                    '../breakpad/breakpad.gyp:breakpad',
                    'app/policy/cloud_policy_codegen.gyp:policy',
                  ],
                  'sources': [
                    'app/breakpad_mac.mm',
                    'app/breakpad_mac.h',
                  ],
                }, {  # else: mac_breakpad_compiled_in!=1
                  # No Breakpad, put in the stubs.
                  'sources': [
                    'app/breakpad_mac_stubs.mm',
                    'app/breakpad_mac.h',
                  ],
                }],  # mac_breakpad_compiled_in
                ['internal_pdf', {
                  'dependencies': [
                    '../pdf/pdf.gyp:pdf',
                  ],
                }],
              ],  # conditions
            }],  # OS=="mac"
          ],  # conditions
        },  # target chrome_dll
      ],  # targets
    }],  # OS=="mac" or OS=="win"
    ['OS=="win"', {
      'targets': [
        {
          # This target is only depended upon on Windows.
          'target_name': 'chrome_dll_pdb_workaround',
          'type': 'static_library',
          'sources': [ 'empty_pdb_workaround.cc' ],
          'conditions': [
            ['fastbuild==0 or win_z7!=0', {
             'msvs_settings': {
              'VCCLCompilerTool': {
                # This *in the compile phase* must match the pdb name that's
                # output by the final link. See empty_pdb_workaround.cc for
                # more details.
                'DebugInformationFormat': '3',
                'ProgramDataBaseFileName': '<(PRODUCT_DIR)/chrome.dll.pdb',
              },
             },
            }],
          ],
        },
      ],
    }],
  ],
}
