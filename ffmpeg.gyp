# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# There's a couple key GYP variables that control how FFmpeg is built:
#   ffmpeg_branding
#     Controls whether we build the Chromium or Google Chrome version of
#     FFmpeg.  The Google Chrome version contains additional codecs.
#     Typical values are Chromium, Chrome, ChromiumOS, and ChromeOS.
#   use_system_ffmpeg
#     When set to non-zero will build Chromium against the system FFmpeg
#     headers via pkg-config.  When Chromium is launched it will assume that
#     FFmpeg is present in the system library path.  Default value is 0.
#   build_ffmpegsumo
#     When set to zero will build Chromium against Chrome's FFmpeg headers, but
#     not build ffmpegsumo itself.  Users are expected to build and provide
#     their own version of ffmpegsumo.  Default value is 1.
#

{
  'target_defaults': {
    'variables': {
      # Since we are not often debugging FFmpeg, and performance is
      # unacceptable without optimization, freeze the optimizations to -O2.
      # If someone really wants -O1 , they can change these in their checkout.
      # If you want -O0, see the Gotchas in README.Chromium for why that
      # won't work.
      'release_optimize': '2',
      'debug_optimize': '2',
      'mac_debug_optimization': '2',
      # In addition to the above reasons, /Od optimization won't remove symbols
      # that are under "if (0)" style sections.  Which lead to link time errors
      # when for example it tries to link an ARM symbol on X86.
      'win_debug_Optimization': '2',
      # Run time checks are incompatible with any level of optimizations.
      'win_debug_RuntimeChecks': '0',
    },
  },
  'variables': {
    # Allow overriding the selection of which FFmpeg binaries to copy via an
    # environment variable.  Affects the ffmpeg_binaries target.
    'conditions': [
      ['armv7 == 1 and arm_neon == 1', {
        # Need a separate config for arm+neon vs arm
        'ffmpeg_config%': 'arm-neon',
      }, {
        'ffmpeg_config%': '<(target_arch)',
      }],
      ['target_arch == "mipsel"', {
        'asm_sources': [
        ],
      }],
      ['OS == "mac" or OS == "win" or OS == "openbsd"', {
        'os_config%': '<(OS)',
      }, {  # all other Unix OS's use the linux config
        'os_config%': 'linux',
      }],
      ['chromeos == 1', {
        'ffmpeg_branding%': '<(branding)OS',
      }, {  # otherwise, assume Chrome/Chromium.
        'ffmpeg_branding%': '<(branding)',
      }],
    ],

    'ffmpeg_variant%': '<(target_arch)',

    'use_system_ffmpeg%': 0,
    'build_ffmpegsumo%': 1,

    # Locations for generated artifacts.
    'shared_generated_dir': '<(SHARED_INTERMEDIATE_DIR)/third_party/ffmpeg',

    # Stub generator script and signatures of all functions used by Chrome.
    'generate_stubs_script': '../../tools/generate_stubs/generate_stubs.py',
    'sig_files': ['chromium/ffmpegsumo.sigs'],
    'extra_header': 'chromium/ffmpeg_stub_headers.fragment',
  },
  'conditions': [
    ['OS != "win" and use_system_ffmpeg == 0 and build_ffmpegsumo != 0', {
      'variables': {
        # Path to platform configuration files.
        'platform_config_root': 'chromium/config/<(ffmpeg_branding)/<(os_config)/<(ffmpeg_config)',
      },
      'includes': [
        'ffmpeg_generated.gypi',
      ],
      'targets': [
        {
          'target_name': 'ffmpegsumo',
          'type': 'loadable_module',
          'sources': [
            '<(platform_config_root)/config.h',
            '<(platform_config_root)/libavutil/avconfig.h',
            '<@(c_sources)',
          ],
          'include_dirs': [
            '<(platform_config_root)',
            '.',
          ],
          'defines': [
            'HAVE_AV_CONFIG_H',
            '_POSIX_C_SOURCE=200112',
            '_XOPEN_SOURCE=600',
            'PIC',
          ],
          'cflags': [
            '-fPIC',
            '-fomit-frame-pointer',
          ],
          'conditions': [
            ['target_arch != "arm" and target_arch != "mipsel"', {
              'dependencies': [
                'ffmpeg_yasm',
              ],
            }],
            ['clang == 1', {
              'xcode_settings': {
                'WARNING_CFLAGS': [
                  # ffmpeg uses its own deprecated functions.
                  '-Wno-deprecated-declarations',
                  # ffmpeg doesn't care about pointer constness.
                  '-Wno-incompatible-pointer-types',
                  # ffmpeg doesn't follow usual parentheses conventions.
                  '-Wno-parentheses',
                  # ffmpeg doesn't care about pointer signedness.
                  '-Wno-pointer-sign',
                  # ffmpeg doesn't believe in exhaustive switch statements.
                  '-Wno-switch',
                ],
              },
              'cflags': [
                '-Wno-deprecated-declarations',
                '-Wno-incompatible-pointer-types',
                '-Wno-logical-op-parentheses',
                '-Wno-parentheses',
                '-Wno-pointer-sign',
                '-Wno-switch',
                # Don't emit warnings for gcc -f flags clang doesn't implement.
                '-Qunused-arguments',
              ],
              'conditions': [
                ['ffmpeg_branding == "Chrome" or ffmpeg_branding == "ChromeOS"', {
                  'xcode_settings': {
                    'WARNING_CFLAGS': [
                      # Clang doesn't support __attribute__((flatten)),
                      # http://llvm.org/PR7559
                      # This is used in the h264 decoder.
                      '-Wno-attributes',
                    ],
                  },
                  'cflags': [
                    '-Wno-attributes',
                  ],
                }],
              ],
            }, {
              'cflags': [
                # gcc doesn't have flags for specific warnings, so disable them
                # all.
                '-w',
              ],
            }],
            ['target_arch == "ia32"', {
              # Turn off valgrind build option that breaks ffmpeg builds.
              'cflags!': [
                '-fno-omit-frame-pointer',
              ],
              'debug_extra_cflags!': [
                '-fno-omit-frame-pointer',
              ],
              'release_extra_cflags!': [
                '-fno-omit-frame-pointer',
              ],
            }],  # target_arch == "ia32"
            ['target_arch == "arm"', {
              # TODO(ihf): See the long comment in build_ffmpeg.sh
              # We want to be consistent with CrOS and have configured
              # ffmpeg for thumb. Protect yourself from -marm.
              'cflags!': [
                '-marm',
              ],
              'cflags': [
                '-mthumb',
                '-march=armv7-a',
                '-mtune=cortex-a8',
              ],
              # On arm we use gcc to compile the assembly.
              'sources': [
                '<@(asm_sources)',
              ],
              'conditions': [
                ['arm_neon == 0', {
                  'cflags': [
                    '-mfpu=vfpv3-d16',
                  ],
                }, {
                  'cflags': [
                    '-mfpu=neon',
                  ],
                }],
                ['arm_float_abi == "hard"', {
                  'cflags': [
                    '-DHAVE_VFP_ARGS=1'
                  ],
                }, {
                  'cflags': [
                    '-DHAVE_VFP_ARGS=0'
                  ],
                }],
              ],
            }],
            ['target_arch == "mipsel"', {
              'cflags': [
                '-mips32',
                '-EL -Wl,-EL',
              ],
            }],  # target_arch == "mipsel"
            ['os_posix == 1 and OS != "mac"', {
              'defines': [
                '_ISOC99_SOURCE',
                '_LARGEFILE_SOURCE',
                # BUG(ihf): ffmpeg compiles with this define. But according to
                # ajwong: I wouldn't change _FILE_OFFSET_BITS.  That's a scary change
                # cause it affects the default length of off_t, and fpos_t,
                # which can cause strange problems if the loading code doesn't
                # have it set and you start passing FILE*s or file descriptors
                # between symbol contexts.
                # '_FILE_OFFSET_BITS=64',
              ],
              'cflags': [
                '-std=c99',
                '-pthread',
                '-fno-math-errno',
                '-fno-signed-zeros',
                '-fno-tree-vectorize',
                '-fomit-frame-pointer',
                # Don't warn about libavformat using its own deprecated APIs.
                '-Wno-deprecated-declarations',
              ],
              'cflags!': [
                # Ensure the symbols are exported.
                #
                # TODO(ajwong): Manually tag the API that we use to be
                # exported.
                '-fvisibility=hidden',
              ],
              'link_settings': {
                'ldflags': [
                  '-Wl,-Bsymbolic',
                  '-L<(shared_generated_dir)',
                ],
                'libraries': [
                  '-lz',
                ],
              },
            }],  # os_posix == 1 and OS != "mac"
            ['OS == "openbsd"', {
              # OpenBSD's gcc (4.2.1) does not support this flag
              'cflags!': [
                '-fno-signed-zeros',
              ],
            }],
            ['OS == "mac"', {
              'defines': [
                '_DARWIN_C_SOURCE',
              ],
              'conditions': [
                ['mac_breakpad == 1', {
                  'variables': {
                    # A real .dSYM is needed for dump_syms to operate on.
                    'mac_real_dsym': 1,
                  },
                }],
              ],
              'link_settings': {
                'libraries': [
                  '$(SDKROOT)/usr/lib/libz.dylib',
                ],
              },
              'xcode_settings': {
                'GCC_SYMBOLS_PRIVATE_EXTERN': 'NO',  # No -fvisibility=hidden
                'DYLIB_INSTALL_NAME_BASE': '@loader_path',
                'LIBRARY_SEARCH_PATHS': [
                  '<(shared_generated_dir)'
                ],
                'OTHER_LDFLAGS': [
                  # This is needed because even though FFmpeg now builds with
                  # -fPIC, it's not quite a complete PIC build, only partial :(
                  # Thus we need to instruct the linker to allow relocations
                  # for read-only segments for this target to be able to
                  # generated the shared library on Mac.
                  #
                  # This makes Mark sad, but he's okay with it since it is
                  # isolated to this module. When Mark finds this in the
                  # future, and has forgotten this conversation, this comment
                  # should remind him that the world is still nice and
                  # butterflies still exist...as do rainbows, sunshine,
                  # tulips, etc., etc...but not kittens. Those went away
                  # with this flag.
                  '-Wl,-read_only_relocs,suppress',
                ],
              },
            }],  # OS == "mac"
          ],
        },
      ],
    }],
    ['OS == "win"', {
      'variables': {
        # Path to platform configuration files.
        'platform_config_root': 'chromium/config/<(ffmpeg_branding)/<(os_config)/<(ffmpeg_config)',
      },
      'includes': [
        'ffmpeg_generated.gypi',
      ],
      'targets': [
        {
          'target_name': 'convert_ffmpeg_sources',
          'type': 'none',
          'sources': [
            '<@(c_sources)',
          ],
          'variables': {
            'converter_script': 'chromium/scripts/c99conv.py',
            'converter_executable': 'chromium/binaries/c99conv.exe',
          },
          'rules': [
            {
              'rule_name': 'convert_c99_to_c89',
              'extension': 'c',
              'inputs': [
                '<(converter_script)',
                '<(converter_executable)',
                # Since we don't know the dependency graph for header includes
                # we need to list them all here to ensure a header change causes
                # a recompilation.
                '<(platform_config_root)/config.h',
                '<(platform_config_root)/libavutil/avconfig.h',
                '<@(c_headers)',
              ],
              # Argh!  Required so that the msvs generator will properly convert
              # RULE_INPUT_DIRNAME to a relative path.
              'msvs_external_rule': 1,
              'outputs': [
                '<(shared_generated_dir)/<(RULE_INPUT_DIRNAME)/<(RULE_INPUT_ROOT).c',
              ],
              'action': [
                'python',
                '<(converter_script)',
                '<(RULE_INPUT_PATH)',
                '<(shared_generated_dir)/<(RULE_INPUT_DIRNAME)/<(RULE_INPUT_ROOT).c',
                '-I', '<(platform_config_root)',
              ],
              'message': 'Converting <(RULE_INPUT_PATH) from C99 to C89.',
              'process_outputs_as_sources': 1,
            },
          ],
        },
        {
          'target_name': 'ffmpegsumo',
          'type': 'loadable_module',
          'dependencies': [
            'convert_ffmpeg_sources',
            'ffmpeg_yasm',
          ],
          'sources': [
            '<(platform_config_root)/config.h',
            '<(platform_config_root)/libavutil/avconfig.h',
            '<@(converter_outputs)',
            '<(shared_generated_dir)/ffmpegsumo.def',
          ],
          # TODO(dalecurtis): We should fix these.  http://crbug.com/154421
          'msvs_disabled_warnings': [
            4996, 4018, 4090, 4305, 4133, 4146, 4554, 4028, 4334, 4101, 4102,
            4116, 4307, 4273
          ],
          'msvs_settings': {
            # This magical incantation is necessary because VC++ will compile
            # object files to same directory... even if they have the same name!
            'VCCLCompilerTool': {
              'ObjectFile': '$(IntDir)/%(RelativeDir)/',
            },
            # Ignore warnings about a local symbol being inefficiently imported,
            # upstream is working on a fix.
            'VCLinkerTool': {
              'AdditionalOptions': ['/ignore:4049', '/ignore:4217'],
            }
          },
          'actions': [
            {
              'action_name': 'generate_def',
              'inputs': [
                '<(generate_stubs_script)',
                '<@(sig_files)',
              ],
              'outputs': [
                '<(shared_generated_dir)/ffmpegsumo.def',
              ],
              'action': ['python',
                         '<(generate_stubs_script)',
                         '-i', '<(INTERMEDIATE_DIR)',
                         '-o', '<(shared_generated_dir)',
                         '-t', 'windows_def',
                         '-m', 'ffmpegsumo.dll',
                         '<@(_inputs)',
              ],
              'message': 'Generating FFmpeg export definitions.',
            },
          ],
        }
      ],
    }],
  ],  # conditions
  'targets': [
    {
      'target_name': 'ffmpeg',
      'sources': [
        # Files needed for stub generation rules.
        '<@(sig_files)',
      ],
      'defines': [
        '__STDC_CONSTANT_MACROS',  # FFmpeg uses INT64_C.
      ],
      'hard_dependency': 1,

      # Do not fear the massive conditional blocks!  They do the following:
      #   1) Use the Window stub generator on Windows
      #   2) Else, use the POSIX stub generator on non-Windows
      #     a) Use system includes when use_system_ffmpeg != 0
      #     b) Else, use our local copy
      'conditions': [
        ['OS == "win"', {
          'msvs_guid': 'D7A94F58-576A-45D9-A45F-EB87C63ABBB0',
          'variables': {
            'outfile_type': 'windows_lib',
            'output_dir': '<(PRODUCT_DIR)/lib',
            'intermediate_dir': '<(INTERMEDIATE_DIR)',
          },
          'type': 'none',
          'sources': [
            # Adds C99 types for Visual C++.
            'chromium/include/win/inttypes.h',
          ],
          'dependencies': [
            'ffmpegsumo',
          ],
          'direct_dependent_settings': {
            'include_dirs': [
              '<(platform_config_root)',
              'chromium/include/win',
              '.',
            ],
            'link_settings': {
              'libraries': [
                '<(output_dir)/ffmpegsumo.lib',
              ],
              'msvs_settings': {
                'VCLinkerTool': {
                  'DelayLoadDLLs': [
                    'ffmpegsumo.dll',
                  ],
                },
              },
            },
          },
          'rules': [
            {
              'rule_name': 'generate_libs',
              'extension': 'sigs',
              'inputs': [
                '<(generate_stubs_script)',
                '<@(sig_files)',
              ],
              'outputs': [
                '<(output_dir)/<(RULE_INPUT_ROOT).lib',
              ],
              'action': ['python', '<(generate_stubs_script)',
                         '-i', '<(intermediate_dir)',
                         '-o', '<(output_dir)',
                         '-t', '<(outfile_type)',
                         '<@(RULE_INPUT_PATH)',
              ],
              'message': 'Generating FFmpeg import libraries.',
            },
          ],
        }, {  # else OS != "win", use POSIX stub generator
          'variables': {
            'outfile_type': 'posix_stubs',
            'stubs_filename_root': 'ffmpeg_stubs',
            'project_path': 'third_party/ffmpeg',
            'intermediate_dir': '<(INTERMEDIATE_DIR)',
            'output_root': '<(SHARED_INTERMEDIATE_DIR)/ffmpeg',
          },
          'sources': [
            '<(extra_header)',
          ],
          'type': '<(component)',
          'include_dirs': [
            '<(output_root)',
            '../..',  # The chromium 'src' directory.
          ],
          'dependencies': [
            # Required for the logging done in the stubs generator.
            '../../base/base.gyp:base',
          ],
          'direct_dependent_settings': {
            'defines': [
              '__STDC_CONSTANT_MACROS',  # FFmpeg uses INT64_C.
            ],
            'include_dirs': [
              '<(output_root)',
              '../..',  # The chromium 'src' directory.
            ],
          },
          'conditions': [
            ['use_system_ffmpeg==0', {
              'actions': [
                {
                  'action_name': 'generate_stubs',
                  'inputs': [
                    '<(generate_stubs_script)',
                    '<(extra_header)',
                    '<@(sig_files)',
                  ],
                  'outputs': [
                    '<(intermediate_dir)/<(stubs_filename_root).cc',
                    '<(output_root)/<(project_path)/<(stubs_filename_root).h',
                  ],
                  'action': ['python',
                             '<(generate_stubs_script)',
                             '-i', '<(intermediate_dir)',
                             '-o', '<(output_root)/<(project_path)',
                             '-t', '<(outfile_type)',
                             '-e', '<(extra_header)',
                             '-s', '<(stubs_filename_root)',
                             '-p', '<(project_path)',
                             '<@(_inputs)',
                  ],
                  'process_outputs_as_sources': 1,
                  'message': 'Generating FFmpeg stubs for dynamic loading.',
                },
              ],

            }],

            # Linux/Solaris need libdl for dlopen() and friends.
            ['OS == "linux" or OS == "solaris"', {
              'link_settings': {
                'libraries': [
                  '-ldl',
                ],
              },
            }],

            ['component == "shared_library"', {
              'cflags!': ['-fvisibility=hidden'],
              'xcode_settings': {
                'GCC_SYMBOLS_PRIVATE_EXTERN': 'NO',  # no -fvisibility=hidden
              },
            }],

            # Add pkg-config result to include path when use_system_ffmpeg != 0
            ['use_system_ffmpeg != 0', {
              'cflags': [
                '<!@(pkg-config --cflags libavcodec libavformat libavutil)',
              ],
              'direct_dependent_settings': {
                'cflags': [
                  '<!@(pkg-config --cflags libavcodec libavformat libavutil)',
                ],
                'defines': [
                  'USE_SYSTEM_FFMPEG',
                ],
              },
              'link_settings': {
                'ldflags': [
                  '<!@(pkg-config --libs-only-L --libs-only-other libavcodec libavformat libavutil)',
                ],
                'libraries': [
                  '<!@(pkg-config --libs-only-l libavcodec libavformat libavutil)',
                ],
              },
            }, {  # else use_system_ffmpeg == 0, add local copy to include path
              'include_dirs': [
                '<(platform_config_root)',
                '.',
              ],
              'direct_dependent_settings': {
                'include_dirs': [
                 '<(platform_config_root)',
                  '.',
                ],
              },
              'conditions': [
                ['build_ffmpegsumo != 0', {
                  'dependencies': [
                    'ffmpegsumo',
                  ],
                }],
              ],
            }],
          ],  # conditions
        }],
      ],  # conditions
    },
    {
      'target_name': 'ffmpeg_yasm',
      'type': 'static_library',
      # VS2010 does not correctly incrementally link obj files generated
      # from asm files. This flag disables UseLibraryDependencyInputs to
      # avoid this problem.
      'msvs_2010_disable_uldi_when_referenced': 1,
      'includes': [
        'ffmpeg_generated.gypi',
        '../yasm/yasm_compile.gypi',
      ],
      'sources': [
        '<@(asm_sources)',
        # XCode doesn't want to link a pure assembly target and will fail
        # to link when it creates an empty file list.  So add a dummy file
        # keep the linker happy.  See http://crbug.com/157073
        'xcode_hack.c',
      ],
      'variables': {
        # Path to platform configuration files.
        'platform_config_root': 'chromium/config/<(ffmpeg_branding)/<(os_config)/<(ffmpeg_config)',
        'conditions': [
          ['target_arch == "ia32"', {
            'more_yasm_flags': [
              '-DARCH_X86_32',
             ],
          }, {
            'more_yasm_flags': [
              '-DARCH_X86_64',
            ],
          }],
          ['OS == "mac"', {
            'more_yasm_flags': [
              # Necessary to ensure symbols end up with a _ prefix; added by
              # yasm_compile.gypi for Windows, but not Mac.
              '-DPREFIX',
            ]
          }],
        ],
        'yasm_flags': [
          '-DPIC',
          '>@(more_yasm_flags)',
          '-I', '<(platform_config_root)',
          '-I', 'libavcodec/x86/',
          '-I', 'libavutil/x86/',
          '-I', '.',
          # Disable warnings, prevents log spam for things we won't fix.
          '-w',
          '-P', 'config.asm',
        ],
        'yasm_output_path': '<(shared_generated_dir)/yasm'
      },
    },
  ],  # targets
}
