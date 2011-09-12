# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'conditions': [
    ['OS=="mac" or OS=="win"', {
      'targets': [
        {
          'variables': {
            'conditions' : [
              ['OS=="win" and optimize_with_syzygy==1', {
                # On Windows we use build chrome_dll as an intermediate target
                # then have a subsequent step which either optimizes it to its
                # final location, or copies it to its final location, depending
                # on whether or not optimize_with_syzygy==1.  Please, refer to
                # chrome_dll_syzygy.gypi for the subsequent defintion of the
                # Windows chrome_dll target.
                'dll_target_name': 'chrome_dll_initial',
              }, {
                'dll_target_name': 'chrome_dll',
              }],
            ],
          },
          'target_name': '<(dll_target_name)',
          'type': 'shared_library',
          'dependencies': [
            '<@(chromium_dependencies)',
            'app/policy/cloud_policy_codegen.gyp:policy',
          ],
          'conditions': [
            ['OS=="win"', {
              'product_name': 'chrome',
              'dependencies': [
                # On Windows, link the dependencies (libraries) that make
                # up actual Chromium functionality into this .dll.
                'chrome_version_resources',
                'chrome_resources',
                'installer_util_strings',
                '../content/content.gyp:content_worker',
                '../crypto/crypto.gyp:crypto',
                '../printing/printing.gyp:printing',
                '../net/net.gyp:net_resources',
                '../third_party/cld/cld.gyp:cld',
                '../views/views.gyp:views',
                '../webkit/support/webkit_support.gyp:webkit_resources',
              ],
              'sources': [
                'app/chrome_command_ids.h',
                'app/chrome_dll.rc',
                'app/chrome_dll_resource.h',
                'app/chrome_main.cc',

                '<(SHARED_INTERMEDIATE_DIR)/chrome_version/chrome_dll_version.rc',

                '../webkit/glue/resources/aliasb.cur',
                '../webkit/glue/resources/cell.cur',
                '../webkit/glue/resources/col_resize.cur',
                '../webkit/glue/resources/copy.cur',
                '../webkit/glue/resources/none.cur',
                '../webkit/glue/resources/row_resize.cur',
                '../webkit/glue/resources/vertical_text.cur',
                '../webkit/glue/resources/zoom_in.cur',
                '../webkit/glue/resources/zoom_out.cur',

                # TODO:  It would be nice to have these pulled in
                # automatically from direct_dependent_settings in
                # their various targets (net.gyp:net_resources, etc.),
                # but that causes errors in other targets when
                # resulting .res files get referenced multiple times.
                '<(SHARED_INTERMEDIATE_DIR)/chrome/browser_resources.rc',
                '<(SHARED_INTERMEDIATE_DIR)/chrome/common_resources.rc',
                '<(SHARED_INTERMEDIATE_DIR)/chrome/renderer_resources.rc',
                '<(SHARED_INTERMEDIATE_DIR)/chrome/theme_resources.rc',
                '<(SHARED_INTERMEDIATE_DIR)/chrome/theme_resources_standard.rc',
                '<(SHARED_INTERMEDIATE_DIR)/net/net_resources.rc',
                '<(SHARED_INTERMEDIATE_DIR)/ui/gfx/gfx_resources.rc',
                '<(SHARED_INTERMEDIATE_DIR)/ui/ui_resources/ui_resources.rc',
                '<(SHARED_INTERMEDIATE_DIR)/ui/ui_resources_standard/ui_resources_standard.rc',
                '<(SHARED_INTERMEDIATE_DIR)/webkit/webkit_chromium_resources.rc',
                '<(SHARED_INTERMEDIATE_DIR)/webkit/webkit_resources.rc',

                # TODO(sgk):  left-over from pre-gyp build, figure out
                # if we still need them and/or how to update to gyp.
                #'app/check_dependents.bat',
                #'app/chrome.dll.deps',
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
                  'ProgramDatabaseFile': '$(OutDir)\\chrome_dll.pdb',
                  # Set /SUBSYSTEM:WINDOWS for chrome.dll (for consistency).
                  'SubSystem': '2',
                  'conditions': [
                    ['optimize_with_syzygy==1', {
                      # When syzygy is enabled we use build chrome_dll as an
                      # intermediate target then have a subsequent step which
                      # optimizes it to its final location
                      'ProgramDatabaseFile': '$(OutDir)\\initial\\chrome_dll.pdb',
                      'OutputFile': '$(OutDir)\\initial\\chrome.dll',
                    }], ['incremental_chrome_dll==1', {
                      'OutputFile': '$(OutDir)\\initial\\chrome.dll',
                      'UseLibraryDependencyInputs': "true",
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
              'conditions': [
                ['incremental_chrome_dll==1 and optimize_with_syzygy==0', {
                  # Linking to a different directory and then hardlinking back
                  # to OutDir is a workaround to avoid having the .ilk for
                  # chrome.exe and chrome.dll conflicting. See crbug.com/92528
                  # for more information. Done on the dll instead of the exe so
                  # that people launching from VS don't need to modify
                  # $(TargetPath) for the exe.
                  'msvs_postbuild': 'tools\\build\\win\\hardlink_failsafe.bat $(OutDir)\\initial\\chrome.dll $(OutDir)\\chrome.dll'
                }]
              ]
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
                'app/chrome_main_mac.h',
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
                'app/nibs/AvatarMenuItem.xib',
                'app/nibs/BookmarkAllTabs.xib',
                'app/nibs/BookmarkBar.xib',
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
                'app/nibs/ContentBlockedGeolocation.xib',
                'app/nibs/DownloadItem.xib',
                'app/nibs/DownloadShelf.xib',
                'app/nibs/EditSearchEngine.xib',
                'app/nibs/ExtensionInstalledBubble.xib',
                'app/nibs/ExtensionInstallPrompt.xib',
                'app/nibs/ExtensionInstallPromptInline.xib',
                'app/nibs/ExtensionInstallPromptNoWarnings.xib',
                'app/nibs/FindBar.xib',
                'app/nibs/FirstRunBubble.xib',
                'app/nibs/FirstRunDialog.xib',
                'app/nibs/GlobalErrorBubble.xib',
                'app/nibs/HungRendererDialog.xib',
                'app/nibs/HttpAuthLoginSheet.xib',
                'app/nibs/ImportProgressDialog.xib',
                'app/nibs/InfoBar.xib',
                'app/nibs/InfoBarContainer.xib',
                'app/nibs/InstantOptIn.xib',
                'app/nibs/MainMenu.xib',
                'app/nibs/Notification.xib',
                'app/nibs/Panel.xib',
                'app/nibs/PreviewableContents.xib',
                'app/nibs/ReportBug.xib',
                'app/nibs/SaveAccessoryView.xib',
                'app/nibs/SadTab.xib',
                'app/nibs/SearchEngineDialog.xib',
                'app/nibs/SpeechInputBubble.xib',
                'app/nibs/TabView.xib',
                'app/nibs/TaskManager.xib',
                'app/nibs/Toolbar.xib',
                'app/theme/balloon_wrench.pdf',
                'app/theme/chevron.pdf',
                'app/theme/find_next_Template.pdf',
                'app/theme/find_prev_Template.pdf',
                'app/theme/nav.pdf',
                'app/theme/newtab.pdf',
                'app/theme/newtab_h.pdf',
                'app/theme/newtab_p.pdf',
                'app/theme/omnibox_extension_app.pdf',
                'app/theme/omnibox_history.pdf',
                'app/theme/omnibox_http.pdf',
                'app/theme/omnibox_https_invalid.pdf',
                'app/theme/omnibox_https_valid.pdf',
                'app/theme/omnibox_https_warning.pdf',
                'app/theme/omnibox_search.pdf',
                'app/theme/omnibox_star.pdf',
                'app/theme/otr_icon.pdf',
                'app/theme/popup_window_animation.pdf',
                'app/theme/star.pdf',
                'app/theme/star_lit.pdf',
                'browser/mac/install.sh',
              ],
              'mac_bundle_resources!': [
                'app/framework-Info.plist',
              ],
              'dependencies': [
                # Bring in pdfsqueeze and run it on all pdfs
                '../build/temp_gyp/pdfsqueeze.gyp:pdfsqueeze',
                '../crypto/crypto.gyp:crypto',
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
                'repack_path': '../tools/grit/grit/format/repack.py',
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
                      '<(grit_out_dir)/default_plugin_resources/default_plugin_resources.pak',
                      '<(grit_out_dir)/renderer_resources.pak',
                      '<(grit_out_dir)/theme_resources.pak',
                      '<(grit_out_dir)/theme_resources_standard.pak',
                      '<(SHARED_INTERMEDIATE_DIR)/net/net_resources.pak',
                      '<(SHARED_INTERMEDIATE_DIR)/ui/ui_resources/ui_resources.pak',
                      '<(SHARED_INTERMEDIATE_DIR)/ui/ui_resources_standard/ui_resources_standard.pak',
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
                  'action_name': 'repack_theme_resources_large',
                  'variables': {
                    'pak_inputs': [
                      '<(grit_out_dir)/theme_resources_large.pak',
                    ],
                  },
                  'inputs': [
                    '<(repack_path)',
                    '<@(pak_inputs)',
                  ],
                  'outputs': [
                    '<(INTERMEDIATE_DIR)/repack/theme_resources_large.pak',
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
                  # This is an exact copy of the above phase, except for two
                  # changes:
                  # 1. process_outputs_as_mac_bundle_resources is omitted.
                  # 2. We pass 'pseudo_locales' instead of 'locales' wherever
                  #    'locales' is used.
                  # The result is a build phase that builds all pseudo locales
                  # but doesn't copy them to the final dll/framework.
                  'action_name': 'repack_pseudo_locales',
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
                    '>!@(<(repack_locales_cmd) -i <(branding_flag) -g \'<(grit_out_dir)\' -s \'<(SHARED_INTERMEDIATE_DIR)\' -x \'<(INTERMEDIATE_DIR)\' <(pseudo_locales))',
                  ],
                  'outputs': [
                    '<(INTERMEDIATE_DIR)/<(pseudo_locales).pak'
                  ],
                  'action': [
                    '<@(repack_locales_cmd)',
                    '<@(branding_flag)',
                    '-g', '<(grit_out_dir)',
                    '-s', '<(SHARED_INTERMEDIATE_DIR)',
                    '-x', '<(INTERMEDIATE_DIR)',
                    '<@(pseudo_locales)',
                  ],
                },
                {
                  'action_name': 'repack_resources',
                  'variables': {
                    'pak_inputs': [
                      '<(grit_out_dir)/component_extension_resources.pak',
                      '<(grit_out_dir)/devtools_frontend_resources.pak',
                      '<(grit_out_dir)/devtools_resources.pak',
                      '<(grit_out_dir)/net_internals_resources.pak',
                      '<(grit_out_dir)/options_resources.pak',
                      '<(grit_out_dir)/shared_resources.pak',
                      '<(grit_out_dir)/sync_internals_resources.pak',
                      '<(grit_out_dir)/workers_resources.pak',
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
                  'destination':
                      '<(PRODUCT_DIR)/$(CONTENTS_FOLDER_PATH)/Libraries',
                  'files': [
                    # TODO(ajwong): Find a way to share this path with
                    # ffmpeg.gyp so they don't diverge. (BUG=23602)
                    '<(PRODUCT_DIR)/ffmpegsumo.so',
                  ],
                },
                {
                  'destination': '<(PRODUCT_DIR)/$(CONTENTS_FOLDER_PATH)/Internet Plug-Ins',
                  'files': [],
                  'conditions': [
                    ['branding == "Chrome"', {
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
                    ['disable_nacl!=1', {
                      'files': [
                        '<(PRODUCT_DIR)/ppGoogleNaClPluginChrome.plugin',
                        # We leave out nacl_irt_x86_64.nexe because we only
                        # support x86-32 NaCl on Mac OS X.
                        '<(PRODUCT_DIR)/nacl_irt_x86_32.nexe',
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
                {
                  'destination': '<(PRODUCT_DIR)/pseudo_locales',
                  'files': [
                      '<(INTERMEDIATE_DIR)/<(pseudo_locales).pak'
                  ],
                },
                {
                  'destination': '<(PRODUCT_DIR)/$(CONTENTS_FOLDER_PATH)/resources',
                  'files': [],
                  'conditions': [
                    ['debug_devtools!=0', {
                      'files': [
                         '<(PRODUCT_DIR)/resources/inspector',
                      ],
                    }],
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
                  'copies': [
                    {
                      'destination': '<(PRODUCT_DIR)/$(CONTENTS_FOLDER_PATH)/Resources',
                      'files': [
                        '<(PRODUCT_DIR)/crash_inspector',
                        '<(PRODUCT_DIR)/crash_report_sender.app'
                      ],
                    },
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
                    'browser/mac/keystone_promote_preflight.sh',
                    'browser/mac/keystone_promote_postflight.sh',
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
  ],
}
