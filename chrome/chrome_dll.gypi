# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'target_defaults': {
    'variables': {
      'chrome_dll_target': 0,
    },
    'target_conditions': [
      ['chrome_dll_target==1', {
        'conditions': [
          ['OS=="win"', {
            'include_dirs': [
              '<(DEPTH)/third_party/wtl/include',
            ],
            'defines': [
              'CHROME_DLL',
              'BROWSER_DLL',
              'RENDERER_DLL',
              'PLUGIN_DLL',
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
                  'setupapi.dll',
                ],
                # Set /SUBSYSTEM:WINDOWS for chrome.dll (for consistency).
                'SubSystem': '2',
              },
              'VCManifestTool': {
                'AdditionalManifestFiles': '$(ProjectDir)\\app\\chrome.dll.manifest',
              },
            },
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
        ],  # conditions
      }],
    ],
  },
  'conditions': [
    ['OS=="mac" or OS=="win"', {
      'targets': [
        {
          'target_name': 'chrome_dll',
          'type': 'shared_library',
          'variables': {
            'chrome_dll_target': 1,
          },
          'dependencies': [
            '<@(chromium_dependencies)',
            'app/policy/cloud_policy_codegen.gyp:policy',
          ],
          'conditions': [
            ['OS=="win"', {
              'product_name': 'chrome',
              'msvs_guid': 'C0A7EE2C-2A6D-45BE-BA78-6D006FDF52D9',
              'dependencies': [
                # On Windows, link the dependencies (libraries) that make
                # up actual Chromium functionality into this .dll.
                'chrome_dll_version',
                'chrome_resources',
                'installer_util_strings',
                'worker',
                '../printing/printing.gyp:printing',
                '../net/net.gyp:net_resources',
                '../third_party/cld/cld.gyp:cld',
                '../views/views.gyp:views',
                '../webkit/support/webkit_support.gyp:webkit_resources',
                '../gears/gears.gyp:gears',
              ],
              'sources': [
                'app/chrome_command_ids.h',
                'app/chrome_dll.rc',
                'app/chrome_dll_resource.h',
                'app/chrome_main.cc',
                'app/chrome_main_win.cc',
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
                '<(SHARED_INTERMEDIATE_DIR)/chrome/autofill_resources.rc',
                '<(SHARED_INTERMEDIATE_DIR)/chrome/browser_resources.rc',
                '<(SHARED_INTERMEDIATE_DIR)/chrome/common_resources.rc',
                '<(SHARED_INTERMEDIATE_DIR)/chrome/renderer_resources.rc',
                '<(SHARED_INTERMEDIATE_DIR)/chrome/theme_resources.rc',
                '<(SHARED_INTERMEDIATE_DIR)/net/net_resources.rc',
                '<(SHARED_INTERMEDIATE_DIR)/webkit/webkit_chromium_resources.rc',
                '<(SHARED_INTERMEDIATE_DIR)/webkit/webkit_resources.rc',

                # TODO(sgk):  left-over from pre-gyp build, figure out
                # if we still need them and/or how to update to gyp.
                #'app/check_dependents.bat',
                #'app/chrome.dll.deps',
              ],
              'msvs_settings': {
                'VCLinkerTool': {
                  'ImportLibrary': '$(OutDir)\\lib\\chrome_dll.lib',
                  'ProgramDatabaseFile': '$(OutDir)\\chrome_dll.pdb',
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
                # is to just use a=BUILD, b=(PATCH/256), c=(PATCH%256). There
                # is no ambiguity in this scheme because the build and patch
                # numbers are guaranteed unique even across distinct major
                # and minor version numbers.  These settings correspond to
                # -compatibility_version and -current_version.
                'DYLIB_COMPATIBILITY_VERSION': '<(version_mac_dylib)',
                'DYLIB_CURRENT_VERSION': '<(version_mac_dylib)',

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
                'app/chrome_command_ids.h',
                'app/chrome_dll_resource.h',
                'app/chrome_main.cc',
                'app/chrome_main_app_mode_mac.mm',
                'app/chrome_main_mac.mm',
                'app/chrome_main_posix.cc',
              ],
              'include_dirs': [
                '<(grit_out_dir)',
              ],
              # TODO(mark): Come up with a fancier way to do this.  It should
              # only be necessary to list framework-Info.plist once, not the
              # three times it is listed here.
              'mac_bundle_resources': [
                # This image is used to badge the lock icon in the
                # authentication dialogs, such as those used for installation
                # from disk image and Keystone promotion (if so enabled).  It
                # needs to exist as a file on disk and not just something in a
                # resource bundle because that's the interface that
                # Authorization Services uses.  Also, Authorization Services
                # can't deal with .icns files.
                'app/theme/<(theme_dir_name)/product_logo_32.png',

                'app/framework-Info.plist',
                'app/nibs/About.xib',
                'app/nibs/AboutIPC.xib',
                'app/nibs/BookmarkAllTabs.xib',
                'app/nibs/BookmarkBar.xib',
                'app/nibs/BookmarkBarFolderWindow.xib',
                'app/nibs/BookmarkBubble.xib',
                'app/nibs/BookmarkEditor.xib',
                'app/nibs/BookmarkNameFolder.xib',
                'app/nibs/BrowserWindow.xib',
                'app/nibs/CollectedCookies.xib',
                'app/nibs/CookieDetailsView.xib',
                'app/nibs/ContentBlockedCookies.xib',
                'app/nibs/ContentBlockedImages.xib',
                'app/nibs/ContentBlockedJavaScript.xib',
                'app/nibs/ContentBlockedPlugins.xib',
                'app/nibs/ContentBlockedPopups.xib',
                'app/nibs/ContentBubbleGeolocation.xib',
                'app/nibs/DownloadItem.xib',
                'app/nibs/DownloadShelf.xib',
                'app/nibs/ExtensionInstalledBubble.xib',
                'app/nibs/ExtensionInstallPrompt.xib',
                'app/nibs/ExtensionInstallPromptNoWarnings.xib',
                'app/nibs/FindBar.xib',
                'app/nibs/FirstRunBubble.xib',
                'app/nibs/FirstRunDialog.xib',
                'app/nibs/HungRendererDialog.xib',
                'app/nibs/HttpAuthLoginSheet.xib',
                'app/nibs/ImportProgressDialog.xib',
                'app/nibs/ImportSettingsDialog.xib',
                'app/nibs/InfoBar.xib',
                'app/nibs/InfoBarContainer.xib',
                'app/nibs/InstantOptIn.xib',
                'app/nibs/MainMenu.xib',
                'app/nibs/Notification.xib',
                'app/nibs/PreviewableContents.xib',
                'app/nibs/ReportBug.xib',
                'app/nibs/SaveAccessoryView.xib',
                'app/nibs/SadTab.xib',
                'app/nibs/SearchEngineDialog.xib',
                'app/nibs/SimpleContentExceptionsWindow.xib',
                'app/nibs/SpeechInputBubble.xib',
                'app/nibs/TabView.xib',
                'app/nibs/TaskManager.xib',
                'app/nibs/Toolbar.xib',
                'app/theme/back_Template.pdf',
                'app/theme/balloon_wrench.pdf',
                'app/theme/browser_actions_overflow_Template.pdf',
                'app/theme/chevron.pdf',
                'app/theme/find_next_Template.pdf',
                'app/theme/find_prev_Template.pdf',
                'app/theme/forward_Template.pdf',
                'app/theme/home_Template.pdf',
                'app/theme/menu_hierarchy_arrow.pdf',
                'app/theme/menu_overflow_down.pdf',
                'app/theme/menu_overflow_up.pdf',
                'app/theme/nav.pdf',
                'app/theme/newtab.pdf',
                'app/theme/newtab_h.pdf',
                'app/theme/newtab_p.pdf',
                'app/theme/omnibox_history.pdf',
                'app/theme/omnibox_http.pdf',
                'app/theme/omnibox_https_invalid.pdf',
                'app/theme/omnibox_https_valid.pdf',
                'app/theme/omnibox_https_warning.pdf',
                'app/theme/omnibox_search.pdf',
                'app/theme/omnibox_star.pdf',
                'app/theme/otr_icon.pdf',
                'app/theme/popup_window_animation.pdf',
                'app/theme/reload_Template.pdf',
                'app/theme/star.pdf',
                'app/theme/star_lit.pdf',
                'app/theme/stop_Template.pdf',
                'app/theme/tools_Template.pdf',
                'browser/ui/cocoa/install.sh',
              ],
              'mac_bundle_resources!': [
                'app/framework-Info.plist',
              ],
              'dependencies': [
                # Bring in pdfsqueeze and run it on all pdfs
                '../build/temp_gyp/pdfsqueeze.gyp:pdfsqueeze',
                # On Mac, Flash gets put into the framework, so we need this
                # dependency here. flash_player.gyp will copy the Flash bundle
                # into PRODUCT_DIR.
                '../third_party/adobe/flash/flash_player.gyp:flash_player',
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
                'conditions': [
                  ['branding=="Chrome"', {
                    'theme_dir_name': 'google_chrome',
                  }, {  # else: 'branding!="Chrome"
                    'theme_dir_name': 'chromium',
                  }],
                ],
                'repack_path': '../tools/data_pack/repack.py',
              },
              'actions': [
                # TODO(mark): These actions are duplicated for Linux and
                # FreeBSD in the chrome target.  Can they be unified?
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
                      '<(SHARED_INTERMEDIATE_DIR)/net/net_resources.pak',
                      '<(SHARED_INTERMEDIATE_DIR)/webkit/webkit_chromium_resources.pak',
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
                {
                  'action_name': 'repack_resources',
                  'variables': {
                    'pak_inputs': [
                      '<(grit_out_dir)/component_extension_resources.pak',
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
                    '<(INTERMEDIATE_DIR)/repack/resources.pak',
                  ],
                  'action': ['python', '<(repack_path)', '<@(_outputs)',
                             '<@(pak_inputs)'],
                  'process_outputs_as_mac_bundle_resources': 1,
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
                  # The framework needs the Breakpad keys if this feature is
                  # enabled.  It does not need the Keystone keys; these always
                  # come from the outer application bundle.  The framework
                  # doesn't currently use the Subversion keys for anything,
                  # but this seems like a really good place to store them.
                  'postbuild_name': 'Tweak Info.plist',
                  'action': ['<(tweak_info_plist_path)',
                             '-b<(mac_breakpad)',
                             '-k0',
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
                    '<(PRODUCT_DIR)/resources/inspector/',
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
                {
                  'destination': '<(PRODUCT_DIR)/$(CONTENTS_FOLDER_PATH)/Internet Plug-Ins',
                  'files': [
                    '<(PRODUCT_DIR)/ppGoogleNaClPluginChrome.plugin',
                  ],
                  'conditions': [
                    [ 'branding == "Chrome"', {
                      'files': [
                        '<(PRODUCT_DIR)/Flash Player Plugin for Chrome.plugin',
                        '<(PRODUCT_DIR)/plugin.vch',
                      ],
                    }],
                    ['internal_pdf', {
                      'files': [
                        '<(PRODUCT_DIR)/PDF.plugin',
                      ],
                    }],
                  ],
                },
                {
                  'destination': '<(PRODUCT_DIR)',
                  'files': [
                      '<(INTERMEDIATE_DIR)/repack/resources.pak'
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
                    'app/policy/cloud_policy_codegen.gyp:policy',
                  ],
                }, {  # else: mac_breakpad!=1
                  # No Breakpad, put in the stubs.
                  'sources': [
                    'app/breakpad_mac_stubs.mm',
                    'app/breakpad_mac.h',
                  ],
                }],  # mac_breakpad
                ['mac_keystone==1', {
                  'mac_bundle_resources': [
                    'browser/ui/cocoa/keystone_promote_preflight.sh',
                    'browser/ui/cocoa/keystone_promote_postflight.sh',
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
    [ 'OS=="win"', {
      'targets': [
        {
          'target_name': 'chrome_dll_nacl_win64',
          'type': 'shared_library',
          'product_name': 'nacl64',
          'msvs_guid': 'F5B2D851-1279-4CE1-9386-AB7C6433551B',
          'variables': {
            'chrome_dll_target': 1,
          },
          'include_dirs': [
            '..',
          ],
          'dependencies': [
            '<@(nacl_win64_dependencies)',
            'chrome_dll_version',
            'nacl_win64',
          ],
          'defines': [
            '<@(nacl_win64_defines)',
          ],
          'sources': [
            'app/chrome_command_ids.h',
            'app/chrome_dll_resource.h',
            'app/chrome_main.cc',
            'app/chrome_main_win.cc',
            # Parsing is needed for the UserDataDir policy which is read much
            # earlier than the initialization of the policy/pref system. 
            'browser/policy/policy_path_parser_win.cc',
            'browser/renderer_host/render_process_host_dummy.cc',
            'common/googleurl_dummy.cc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome_dll_version/chrome_dll_version.rc',

            # TODO:  It would be nice to have these pulled in
            # automatically from direct_dependent_settings in
            # their various targets (net.gyp:net_resources, etc.),
            # but that causes errors in other targets when
            # resulting .res files get referenced multiple times.
            '<(SHARED_INTERMEDIATE_DIR)/app/app_resources/app_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/autofill_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/common_resources.rc',

            # TODO(sgk):  left-over from pre-gyp build, figure out
            # if we still need them and/or how to update to gyp.
            #'app/check_dependents.bat',
            #'app/chrome.dll.deps',

            # Stub entry points for process types that are not supported
            # by NaCl Win64 executable
            'app/dummy_main_functions.cc',

            # TODO(bradnelson): once automatic generation of 64 bit targets on
            # Windows is ready, take this out and add a dependency on
            # content_common.gypi and common.gypi in nacl_win64_dependencies
            # and get rid of the common_constants.gypi which was added as a hack
            # to avoid making common compile on 64 bit on Windows.
            '../chrome/common/chrome_content_client.cc',
            '../content/common/child_process.cc',
            '../content/common/child_thread.cc',
            '../content/common/content_client.cc',
            '../content/common/content_paths.cc',
            '../content/common/content_switches.cc',
            '../content/common/notification_details.cc',
            '../content/common/notification_service.cc',
            '../content/common/notification_source.cc',
          ],
          'msvs_settings': {
            'VCLinkerTool': {
              'ImportLibrary': '$(OutDir)\\lib\\nacl64_dll.lib',
              'ProgramDatabaseFile': '$(OutDir)\\nacl64_dll.pdb',
            },
          },
          'configurations': {
            'Common_Base': {
              'msvs_target_platform': 'x64',
            },
            'Debug_Base': {
              'msvs_settings': {
                'VCLinkerTool': {
                  'LinkIncremental': '<(msvs_debug_link_nonincremental)',
                },
              },
            },
          },
        },  # target chrome_dll
      ],
    }],
  ],
}
