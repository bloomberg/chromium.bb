# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'target_defaults': {
    'variables': {
      'chrome_exe_target': 0,
    },
    'target_conditions': [
      ['chrome_exe_target==1', {
        'sources': [
          # .cc, .h, and .mm files under app that are used on all
          # platforms, including both 32-bit and 64-bit Windows.
          # Test files are not included.
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
          ['OS=="win"', {
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
        ],
      }],
    ],
  },
  'targets': [
    {
      'target_name': 'chrome',
      'type': 'executable',
      'mac_bundle': 1,
      'msvs_guid': '7B219FAA-E360-43C8-B341-804A94EEFFAC',
      'variables': {
        'chrome_exe_target': 1,
      },
      'conditions': [
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
            'packed_resources',
          ],
          'sources': [
            'app/chrome_dll_main.cc',
            'app/chrome_dll_resource.h',
          ],
          'copies': [
            {
              'destination': '<(PRODUCT_DIR)',
              'files': ['tools/build/linux/chrome-wrapper',
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
              # targets.  Use -b0 to not include any Breakpad information; that
              # all goes into the framework's Info.plist.  Keystone information
              # is included if Keystone is enabled because the ticket will
              # reference this Info.plist to determine the tag of the installed
              # product.  Use -s1 to include Subversion information.
              'postbuild_name': 'Tweak Info.plist',
              'action': ['<(tweak_info_plist_path)',
                         '-b0',
                         '-k<(mac_keystone)',
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
        ['OS != "mac"', {
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
          'msvs_settings': {
            'VCLinkerTool': {
              'ImportLibrary': '$(OutDir)\\lib\\chrome_exe.lib',
              'ProgramDatabaseFile': '$(OutDir)\\chrome_exe.pdb',
            },
          },
        }],
      ],
    },
  ],
  'conditions': [
    ['OS=="win"', {
      'targets': [
        {
          'target_name': 'chrome_nacl_win64',
          'type': 'executable',
          'product_name': 'nacl',
          'msvs_guid': 'BB1AE956-038B-4092-96A2-951D2B418548',
          'variables': {
            'chrome_exe_target': 1,
          },
          'dependencies': [
            # On Windows make sure we've built Win64 version of chrome_dll,
            # which contains all of the library code with Chromium
            # functionality.
            'chrome_dll_nacl_win64',
            'installer/installer.gyp:installer_util_nacl_win64',
            'common_constants_win64',
            '../breakpad/breakpad.gyp:breakpad_handler_win64',
            '../breakpad/breakpad.gyp:breakpad_sender_win64',
            '../base/base.gyp:base_nacl_win64',
            '../chrome_frame/chrome_frame.gyp:npchrome_frame',
            # TODO(gregoryd): build sandbox for 64 bit
            # '../sandbox/sandbox.gyp:sandbox',
          ],
          'defines': [
            '<@(nacl_win64_defines)',
          ],
          'include_dirs': [
            '<(SHARED_INTERMEDIATE_DIR)/chrome',
          ],
          'msvs_settings': {
            'VCLinkerTool': {
              'ImportLibrary': '$(OutDir)\\lib\\nacl_exe.lib',
              'ProgramDatabaseFile': '$(OutDir)\\nacl_exe.pdb',
            },
          },
          'configurations': {
            'Common_Base': {
              'msvs_target_platform': 'x64',
            },
          },
        },
      ],
    }],
  ],
}
