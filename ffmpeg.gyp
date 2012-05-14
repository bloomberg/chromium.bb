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
#     When set to zero will build Chromium against the patched ffmpegsumo
#     headers, but not build ffmpegsumo itself.  Users are expected to build
#     and provide their own version of ffmpegsumo.  Default value is 1.
#

{
  'target_defaults': {
    'conditions': [
      ['os_posix != 1 or OS == "mac" or OS == "openbsd"', {
        'sources/': [['exclude', '/linux/']]
      }],
      ['OS != "mac"', {'sources/': [['exclude', '/mac/']]}],
      ['OS != "win"', {'sources/': [['exclude', '/win/']]}],
      ['OS != "openbsd"', {'sources/': [['exclude', '/openbsd/']]}],
    ],
    'variables': {
      # Since we are not often debugging FFmpeg, and performance is
      # unacceptable without optimization, freeze the optimizations to -O2.
      # If someone really wants -O1 , they can change these in their checkout.
      # If you want -O0, see the Gotchas in README.Chromium for why that
      # won't work.
      'release_optimize': '2',
      'debug_optimize': '2',
      'mac_debug_optimization': '2',
    },
  },
  'variables': {
    # Allow overridding the selection of which FFmpeg binaries to copy via an
    # environment variable.  Affects the ffmpeg_binaries target.

    'conditions': [
      ['armv7 == 1 and arm_neon == 1', {
        # Need a separate config for arm+neon vs arm
        'ffmpeg_config%': 'arm-neon',
      }, {
        'ffmpeg_config%': '<(target_arch)',
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
    'asm_library': 'ffmpegasm',
  },
  'conditions': [
    ['OS != "win" and use_system_ffmpeg == 0 and build_ffmpegsumo != 0', {
      'variables': {
        'platform_config_root': 'chromium/config/<(ffmpeg_branding)/<(os_config)/<(ffmpeg_config)',
      },
      'targets': [
        {
          'target_name': 'ffmpegsumo',
          'type': 'loadable_module',
          'sources': [
            '<(platform_config_root)/config.h',
            'chromium/config/libavutil/avconfig.h',
          ],
          'include_dirs': [
            '<(platform_config_root)',
            '.',
            'chromium/config',
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
          'includes': [
            'ffmpeg_generated.gypi',
          ],
          'conditions': [
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
            ['use_system_yasm == 0', {
              'dependencies': [
                '../yasm/yasm.gyp:yasm#host',
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
              'variables': {
                'obj_format': 'elf',
                'conditions': [
                  [ 'use_system_yasm == 1', {
                    'yasm_path': '<!(which yasm)',
                  }, {
                    'yasm_path': '<(PRODUCT_DIR)/yasm',
                  }],
                  [ 'target_arch == "ia32"', {
                    'yasm_flags': [
                      '-DARCH_X86_32',
                      # The next two lines are needed to enable AVX assembly.
                      '-I<(platform_config_root)',
                      '-Pconfig.asm',
                      '-m', 'x86',
                    ],
                  }, {
                    'yasm_flags': [
                      '-DARCH_X86_64',
                      '-m', 'amd64',
                      '-DPIC',
                      # The next two lines are needed to enable AVX assembly.
                      '-I<(platform_config_root)',
                      '-Pconfig.asm',
                    ],
                  }],
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
              'variables': {
                'obj_format': 'macho',
                'yasm_flags': [ '-DPREFIX' ],
                'conditions': [
                  [ 'use_system_yasm == 1', {
                    'yasm_path': '<!(which yasm)',
                  }, {
                    'yasm_path': '<(PRODUCT_DIR)/yasm',
                  }],
                  [ 'target_arch == "ia32"', {
                    'yasm_flags': [
                      '-DARCH_X86_32',
                      # The next two lines are needed to enable AVX assembly.
                      '-I<(platform_config_root)',
                      '-Pconfig.asm',
                      '-m', 'x86',
                    ],
                  }, {
                    'yasm_flags': [
                      '-DARCH_X86_64',
                      '-m', 'amd64',
                      '-DPIC',
                      # The next two lines are needed to enable AVX assembly.
                      '-I<(platform_config_root)',
                      '-Pconfig.asm',
                    ],
                  }],
                ],
              },
            }],  # OS == "mac"
          ],
          'rules': [
            {
              'rule_name': 'assemble',
              'extension': 'asm',
              'inputs': [ '<(yasm_path)', ],
              'outputs': [
                '<(shared_generated_dir)/<(RULE_INPUT_ROOT).o',
              ],
              'action': [
                '<(yasm_path)',
                '-f', '<(obj_format)',
                '<@(yasm_flags)',
                '-I', 'libavcodec/x86/',
                '-I', 'libavutil/x86/',
                '-I', '.',
                '-o', '<(shared_generated_dir)/<(RULE_INPUT_ROOT).o',
                '<(RULE_INPUT_PATH)',
              ],
              'process_outputs_as_sources': 1,
              'message': 'Build ffmpeg yasm build <(RULE_INPUT_PATH).',
            },
          ],
        },
      ],
    }],
  ],  # conditions
  'targets': [
    {
      'variables': {
        'platform_binary_root': 'chromium/binaries/<(ffmpeg_branding)/<(os_config)/<(ffmpeg_config)',
        'generate_stubs_script': '../../tools/generate_stubs/generate_stubs.py',
        'extra_header': 'chromium/ffmpeg_stub_headers.fragment',
        'sig_files': [
          # Note that these must be listed in dependency order.
          # (i.e. if A depends on B, then B must be listed before A.)
          'chromium/avutil-51.sigs',
          'chromium/avcodec-54.sigs',
          'chromium/avformat-54.sigs',
        ],
      },

      'target_name': 'ffmpeg',
      'msvs_guid': 'D7A94F58-576A-45D9-A45F-EB87C63ABBB0',
      'dependencies': [
        '../../base/base.gyp:base',
      ],
      'sources': [
        # Hacks to introduce C99 types into Visual Studio.
        'chromium/include/win/inttypes.h',
        'chromium/include/win/stdint.h',

        # Files needed for stub generation rules.
        '<@(sig_files)',
        '<(extra_header)'
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
          'variables': {
            'outfile_type': 'windows_lib',
            'output_dir': '<(PRODUCT_DIR)/lib',
            'intermediate_dir': '<(INTERMEDIATE_DIR)',
          },
          'type': 'none',
          'sources!': [
            '<(extra_header)',
          ],
          'direct_dependent_settings': {
            'include_dirs': [
              'chromium/include/win',
              'chromium/config',
              '.',
            ],
            'link_settings': {
              'libraries': [
                '<(output_dir)/avcodec-54.lib',
                '<(output_dir)/avformat-54.lib',
                '<(output_dir)/avutil-51.lib',
              ],
              'msvs_settings': {
                'VCLinkerTool': {
                  'DelayLoadDLLs': [
                    'avcodec-54.dll',
                    'avformat-54.dll',
                    'avutil-51.dll',
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

          # Copy prebuilt binaries to build directory.
          'copies': [{
            'destination': '<(PRODUCT_DIR)/',
            'files': [
              '<(platform_binary_root)/avcodec-54.dll',
              '<(platform_binary_root)/avformat-54.dll',
              '<(platform_binary_root)/avutil-51.dll',
            ],
          }],
        }, {  # else OS != "win", use POSIX stub generator
          'variables': {
            'outfile_type': 'posix_stubs',
            'stubs_filename_root': 'ffmpeg_stubs',
            'project_path': 'third_party/ffmpeg',
            'intermediate_dir': '<(INTERMEDIATE_DIR)',
            'output_root': '<(SHARED_INTERMEDIATE_DIR)/ffmpeg',
          },
          'type': '<(component)',
          'include_dirs': [
            '<(output_root)',
            '../..',  # The chromium 'src' directory.
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

          'conditions': [
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
              },
            }, {  # else use_system_ffmpeg == 0, add local copy to include path
              'include_dirs': [
                'chromium/config',
                '.',
              ],
              'direct_dependent_settings': {
                'include_dirs': [
                  'chromium/config',
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
  ],  # targets
}
