# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'conditions': [
      [ 'OS=="linux" or OS=="freebsd" or OS=="openbsd"', {
        # Link to system .so since we already use it due to GTK.
        'use_system_libjpeg%': 1,
      }, {  # OS!="linux" and OS!="freebsd" and OS!="openbsd"
        'use_system_libjpeg%': 0,
      }],
      [ 'OS=="win"', {
        'object_suffix': 'obj',
      }, {
        'object_suffix': 'o',
      }],
      [ 'OS=="linux" or OS=="freebsd" or OS=="openbsd"', {
        # This is a workaround for GYP issue 102.
        # TODO(hbono): Delete this workaround when this issue is fixed.
        'shared_generated_dir': '<(INTERMEDIATE_DIR)/third_party/libjpeg_turbo',
      }, {
        'shared_generated_dir': '<(SHARED_INTERMEDIATE_DIR)/third_party/libjpeg_turbo',
      }],
    ],
  },
  'conditions': [
    [ 'use_system_libjpeg==0', {
      'targets': [
        {
          'target_name': 'libjpeg',
          'type': '<(library)',
          'include_dirs': [
            '.',
          ],
          'defines': [
            'WITH_SIMD',
          ],
          'dependencies': [
            'libjpeg_asm',
          ],
          'sources': [
            'jconfig.h',
            'jpeglib.h',
            'jpeglibmangler.h',
            'jcapimin.c',
            'jcapistd.c',
            'jccoefct.c',
            'jccolor.c',
            'jcdctmgr.c',
            'jchuff.c',
            'jchuff.h',
            'jcinit.c',
            'jcmainct.c',
            'jcmarker.c',
            'jcmaster.c',
            'jcomapi.c',
            'jcparam.c',
            'jcphuff.c',
            'jcprepct.c',
            'jcsample.c',
            'jdapimin.c',
            'jdapistd.c',
            'jdatadst.c',
            'jdatasrc.c',
            'jdcoefct.c',
            'jdcolor.c',
            'jdct.h',
            'jddctmgr.c',
            'jdhuff.c',
            'jdhuff.h',
            'jdinput.c',
            'jdmainct.c',
            'jdmarker.c',
            'jdmaster.c',
            'jdmerge.c',
            'jdphuff.c',
            'jdpostct.c',
            'jdsample.c',
            'jerror.c',
            'jerror.h',
            'jfdctflt.c',
            'jfdctfst.c',
            'jfdctint.c',
            'jidctflt.c',
            'jidctfst.c',
            'jidctint.c',
            'jidctred.c',
            'jinclude.h',
            'jmemmgr.c',
            'jmemnobs.c',
            'jmemsys.h',
            'jmorecfg.h',
            'jpegint.h',
            'jquant1.c',
            'jquant2.c',
            'jutils.c',
            'jversion.h',
          ],
          'direct_dependent_settings': {
            'include_dirs': [
              '.',
            ],
          },
          'conditions': [
            [ 'OS!="win"', {'product_name': 'jpeg_turbo'}],
            # Add target-specific source files.
            [ 'target_arch=="ia32"', {
              'sources': [
                'simd/jsimd_i386.c',
                # Object files assembled by the 'libjpeg_asm' project.
                '<(shared_generated_dir)/jsimdcpu.<(object_suffix)',
                '<(shared_generated_dir)/jccolmmx.<(object_suffix)',
                '<(shared_generated_dir)/jdcolmmx.<(object_suffix)',
                '<(shared_generated_dir)/jcsammmx.<(object_suffix)',
                '<(shared_generated_dir)/jdsammmx.<(object_suffix)',
                '<(shared_generated_dir)/jdmermmx.<(object_suffix)',
                '<(shared_generated_dir)/jcqntmmx.<(object_suffix)',
                '<(shared_generated_dir)/jfmmxfst.<(object_suffix)',
                '<(shared_generated_dir)/jfmmxint.<(object_suffix)',
                '<(shared_generated_dir)/jimmxred.<(object_suffix)',
                '<(shared_generated_dir)/jimmxint.<(object_suffix)',
                '<(shared_generated_dir)/jimmxfst.<(object_suffix)',
                '<(shared_generated_dir)/jcqnt3dn.<(object_suffix)',
                '<(shared_generated_dir)/jf3dnflt.<(object_suffix)',
                '<(shared_generated_dir)/ji3dnflt.<(object_suffix)',
                '<(shared_generated_dir)/jcqntsse.<(object_suffix)',
                '<(shared_generated_dir)/jfsseflt.<(object_suffix)',
                '<(shared_generated_dir)/jisseflt.<(object_suffix)',
                '<(shared_generated_dir)/jccolss2.<(object_suffix)',
                '<(shared_generated_dir)/jdcolss2.<(object_suffix)',
                '<(shared_generated_dir)/jcsamss2.<(object_suffix)',
                '<(shared_generated_dir)/jdsamss2.<(object_suffix)',
                '<(shared_generated_dir)/jdmerss2.<(object_suffix)',
                '<(shared_generated_dir)/jcqnts2i.<(object_suffix)',
                '<(shared_generated_dir)/jfss2fst.<(object_suffix)',
                '<(shared_generated_dir)/jfss2int.<(object_suffix)',
                '<(shared_generated_dir)/jiss2red.<(object_suffix)',
                '<(shared_generated_dir)/jiss2int.<(object_suffix)',
                '<(shared_generated_dir)/jiss2fst.<(object_suffix)',
                '<(shared_generated_dir)/jcqnts2f.<(object_suffix)',
                '<(shared_generated_dir)/jiss2flt.<(object_suffix)',
              ],
            }],
            [ 'target_arch=="x64"', {
              'sources': [
                'simd/jsimd_x86_64.c',
                # Object files assembled by the 'libjpeg_asm' project.
                '<(shared_generated_dir)/jfsseflt-64.<(object_suffix)',
                '<(shared_generated_dir)/jccolss2-64.<(object_suffix)',
                '<(shared_generated_dir)/jdcolss2-64.<(object_suffix)',
                '<(shared_generated_dir)/jcsamss2-64.<(object_suffix)',
                '<(shared_generated_dir)/jdsamss2-64.<(object_suffix)',
                '<(shared_generated_dir)/jdmerss2-64.<(object_suffix)',
                '<(shared_generated_dir)/jcqnts2i-64.<(object_suffix)',
                '<(shared_generated_dir)/jfss2fst-64.<(object_suffix)',
                '<(shared_generated_dir)/jfss2int-64.<(object_suffix)',
                '<(shared_generated_dir)/jiss2red-64.<(object_suffix)',
                '<(shared_generated_dir)/jiss2int-64.<(object_suffix)',
                '<(shared_generated_dir)/jiss2fst-64.<(object_suffix)',
                '<(shared_generated_dir)/jcqnts2f-64.<(object_suffix)',
                '<(shared_generated_dir)/jiss2flt-64.<(object_suffix)',
              ],
            }],
          ],
        },
        {
          # A project that assembles asm files and creates object files.
          'target_name': 'libjpeg_asm',
          'type': 'none',
          'conditions': [
            # Add platform-dependent source files.
            [ 'target_arch=="ia32"', {
              'sources': [
                # The asm files for ia32.
                'simd/jsimdcpu.asm',
                'simd/jccolmmx.asm',
                'simd/jdcolmmx.asm',
                'simd/jcsammmx.asm',
                'simd/jdsammmx.asm',
                'simd/jdmermmx.asm',
                'simd/jcqntmmx.asm',
                'simd/jfmmxfst.asm',
                'simd/jfmmxint.asm',
                'simd/jimmxred.asm',
                'simd/jimmxint.asm',
                'simd/jimmxfst.asm',
                'simd/jcqnt3dn.asm',
                'simd/jf3dnflt.asm',
                'simd/ji3dnflt.asm',
                'simd/jcqntsse.asm',
                'simd/jfsseflt.asm',
                'simd/jisseflt.asm',
                'simd/jccolss2.asm',
                'simd/jdcolss2.asm',
                'simd/jcsamss2.asm',
                'simd/jdsamss2.asm',
                'simd/jdmerss2.asm',
                'simd/jcqnts2i.asm',
                'simd/jfss2fst.asm',
                'simd/jfss2int.asm',
                'simd/jiss2red.asm',
                'simd/jiss2int.asm',
                'simd/jiss2fst.asm',
                'simd/jcqnts2f.asm',
                'simd/jiss2flt.asm',
              ],
            }],
            [ 'target_arch=="x64"', {
              'sources': [
                # The asm files for x64.
                'simd/jfsseflt-64.asm',
                'simd/jccolss2-64.asm',
                'simd/jdcolss2-64.asm',
                'simd/jcsamss2-64.asm',
                'simd/jdsamss2-64.asm',
                'simd/jdmerss2-64.asm',
                'simd/jcqnts2i-64.asm',
                'simd/jfss2fst-64.asm',
                'simd/jfss2int-64.asm',
                'simd/jiss2red-64.asm',
                'simd/jiss2int-64.asm',
                'simd/jiss2fst-64.asm',
                'simd/jcqnts2f-64.asm',
                'simd/jiss2flt-64.asm',
              ],
            }],

            # Build rules for an asm file.
            # On Windows, we use the precompiled yasm binary. On Linux and Mac,
            # we build yasm and use it as ffmpeg does.
            [ 'OS=="win"', {
              'variables': {
                'yasm_path': '../yasm/binaries/win/yasm<(EXECUTABLE_SUFFIX)',
              },
              'rules': [
                {
                  'rule_name': 'assemble',
                  'extension': 'asm',
                  'inputs': [ '<(yasm_path)', ],
                  'outputs': [
                    '<(shared_generated_dir)/<(RULE_INPUT_ROOT).<(object_suffix)',
                  ],
                  'action': [
                    '<(yasm_path)',
                    '-fwin32',
                    '-DWIN32',
                    '-DMSVC',
                    '-Iwin/',
                    '-Isimd/',
                    '-o', '<(shared_generated_dir)/<(RULE_INPUT_ROOT).<(object_suffix)',
                    '<(RULE_INPUT_PATH)',
                  ],
                  'process_outputs_as_sources': 0,
                  'message': 'Building <(RULE_INPUT_ROOT).<(object_suffix)',
                },
              ],
            }],
            [ 'OS=="mac"', {
              'dependencies': [
                '../yasm/yasm.gyp:yasm#host',
              ],
              'variables': {
                'yasm_path': '<(PRODUCT_DIR)/yasm',
              },
              'rules': [
                {
                  'rule_name': 'assemble',
                  'extension': 'asm',
                  'inputs': [ '<(yasm_path)', ],
                  'outputs': [
                    '<(shared_generated_dir)/<(RULE_INPUT_ROOT).<(object_suffix)',
                  ],
                  'action': [
                    '<(yasm_path)',
                    '-fmacho',
                    '-DMACHO',
                    '-Imac/',
                    '-Isimd/',
                    '-o', '<(shared_generated_dir)/<(RULE_INPUT_ROOT).<(object_suffix)',
                    '<(RULE_INPUT_PATH)',
                  ],
                  'process_outputs_as_sources': 0,
                  'message': 'Building <(RULE_INPUT_ROOT).<(object_suffix)',
                },
              ],
            }],
            [ 'OS=="linux"', {
              'dependencies': [
                '../yasm/yasm.gyp:yasm#host',
              ],
              'variables': {
                'yasm_path': '<(PRODUCT_DIR)/yasm',
                'conditions': [
                  [ 'target_arch=="ia32"', {
                    'yasm_format': '-felf',
                    'yasm_flag': '-D__X86__',
                  }, {
                    'yasm_format': '-felf64',
                    'yasm_flag': '-D__x86_64__',
                  }],
                ],
              },
              'rules': [
                {
                  'rule_name': 'assemble',
                  'extension': 'asm',
                  'inputs': [ '<(yasm_path)', ],
                  'outputs': [
                    '<(shared_generated_dir)/<(RULE_INPUT_ROOT).<(object_suffix)',
                  ],
                  'action': [
                    '<(yasm_path)',
                    '<(yasm_format)',
                    '-DELF',
                    '<(yasm_flag)',
                    '-Ilinux/',
                    '-Isimd/',
                    '-o', '<(shared_generated_dir)/<(RULE_INPUT_ROOT).<(object_suffix)',
                    '<(RULE_INPUT_PATH)',
                  ],
                  'process_outputs_as_sources': 0,
                  'message': 'Building <(RULE_INPUT_ROOT).<(object_suffix)',
                },
              ],
            }],
          ],
        },
      ],
    }, { # else: use_system_libjpeg != 0
      'targets': [
        {
          'target_name': 'libjpeg',
          'type': 'settings',
          'direct_dependent_settings': {
            'defines': [
              'USE_SYSTEM_LIBJPEG',
            ],
          },
          'link_settings': {
            'libraries': [
              '-ljpeg',
            ],
          },
        },
      ],
    }],
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
