# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    # See native_client/Sconstruct for more details.
    # Expected address for beginning of data in for the IRT.
    'NACL_IRT_DATA_START': '0x3ef00000',
    # Expected address for beginning of code in for the IRT.
    'NACL_IRT_TEXT_START': '0x0fa00000',
    'nacl_enable_arm_gcc%': 0,
    # Default C compiler defines.
    'nacl_default_defines': [
      '__linux__',
      '__STDC_LIMIT_MACROS=1',
      '__STDC_FORMAT_MACROS=1',
      '_GNU_SOURCE=1',
      '_BSD_SOURCE=1',
      '_POSIX_C_SOURCE=199506',
      '_XOPEN_SOURCE=600',
      'DYNAMIC_ANNOTATIONS_ENABLED=1',
      'DYNAMIC_ANNOTATIONS_PREFIX=NACL_',
    ],
    'nacl_default_compile_flags': [
      #'-std=gnu99',  Added by build_nexe
      '-O2',
      '-g',
      '-Wall',
      '-Werror',
      '-fdiagnostics-show-option',
    ],
  },
  'conditions': [
    ['target_arch!="arm"', {
      'target_defaults': {
        'conditions': [
          ['OS=="win"', {
            'variables': {
              # NOTE: Python is invoked differently by the Native Client
              # builders than the Chromium builders. Invoking using
              # this python_exe defn won't work in the Chrome tree.
              'python_exe': 'call <(DEPTH)/native_client/tools/win_py.cmd',
              'nacl_glibc_tc_root': '<(DEPTH)/native_client/toolchain/win_x86',
            },
          }],
          ['OS=="mac"', {
            'variables': {
              'python_exe': 'python',
              'nacl_glibc_tc_root': '<(DEPTH)/native_client/toolchain/mac_x86',
            },
          }],
          ['OS=="linux"', {
            'variables': {
              'python_exe': 'python',
              'nacl_glibc_tc_root': '<(DEPTH)/native_client/toolchain/linux_x86',
            },
          }],
        ],
        'defines': [],
        'sources': [],
        'compile_flags': [],
        'link_flags': [],
        'include_dirs': [],
        'variables': {
          'nexe_target': '',
          'nlib_target': '',
          'nso_target': '',
          'build_newlib': 0,
          'build_glibc': 0,
          'disable_glibc%': 0,
          'extra_args': [],
          'enable_x86_32': 1,
          'enable_x86_64': 1,
          'enable_arm': 0,
          'extra_deps_newlib64': [],
          'extra_deps_newlib32': [],
          'extra_deps_glibc64': [],
          'extra_deps_glibc32': [],
          'lib_dirs_newlib32': [],
          'lib_dirs_newlib64': [],
          'lib_dirs_glibc32': [],
          'lib_dirs_glibc64': [],
          'include_dirs': ['<(DEPTH)', '<(DEPTH)/ppapi'],
          'defines': [
            '<@(nacl_default_defines)',
            'NACL_BUILD_ARCH=x86',
          ],
          'sources': [],
          'link_flags': [],
          'get_sources': [
            'scan_sources',
            # This is needed to open the .c filenames, which are given
            # relative to the .gyp file.
            '-I.',
            # This is needed to open the .h filenames, which are given
            # relative to the native_client directory's parent.
            '-I<(DEPTH)',
          ],
        },
      },
    }, {
      # ARM case
      'target_defaults': {
        'defines': [],
        'sources': [],
        'compile_flags': [],
        'link_flags': [],
        'include_dirs': [],
        'variables': {
          'python_exe': 'python',
          'nexe_target': '',
          'nlib_target': '',
          'nso_target': '',
          'build_newlib': 0,
          'nacl_enable_arm_gcc%': 0,
          'build_glibc': 0,
          'disable_glibc%': 1,
          'extra_args': [],
          'enable_x86_32': 0,
          'enable_x86_64': 0,
          'enable_arm': 1,
          'extra_deps_newlib_arm': [],
          'native_sources': [],
          'lib_dirs_newlib_arm': [],
          'include_dirs': ['<(DEPTH)', '<(DEPTH)/ppapi'],
          'defines': [
            '<@(nacl_default_defines)',
            'NACL_BUILD_ARCH=arm',
           ],
           'sources': [],
           'link_flags': [],
           'get_sources': [
             'scan_sources',
             # This is needed to open the .c filenames, which are given
             # relative to the .gyp file.
             '-I.',
             # This is needed to open the .h filenames, which are given
             # relative to the native_client directory's parent.
             '-I<(DEPTH)',
           ],
         },
       },
    }],
    ['target_arch!="arm"', {
      'target_defaults': {
        'target_conditions': [
           ['nexe_target!="" and build_newlib!=0 and enable_x86_64!=0', {
             'variables': {
                'tool_name': 'newlib',
                'inst_dir': '<(SHARED_INTERMEDIATE_DIR)/tc_newlib',
                'out_newlib64%': '<(PRODUCT_DIR)/>(nexe_target)_newlib_x64.nexe',
                'objdir_newlib64%': '>(INTERMEDIATE_DIR)/<(tool_name)-x86-64/>(_target_name)',
                'source_list_newlib64%': '<(tool_name)-x86-64.>(_target_name).source_list.gypcmd',
             },
             'actions': [
               {
                 'action_name': 'build newlib x86-64 nexe',
                 'msvs_cygwin_shell': 0,
                 'description': 'building >(out_newlib64)',
                 'inputs': [
                    '<(DEPTH)/native_client/build/build_nexe.py',
                    '>!@pymod_do_main(>(get_sources) >(sources) >(_sources))',
                    '>@(extra_deps_newlib64)',
                    '>(source_list_newlib64)',
                    '<(SHARED_INTERMEDIATE_DIR)/sdk/toolchain/<(OS)_x86_newlib/stamp.prep',
                 ],
                 'outputs': ['>(out_newlib64)'],
                 'action': [
                   '>(python_exe)',
                   '<(DEPTH)/native_client/build/build_nexe.py',
                   '-t', '<(SHARED_INTERMEDIATE_DIR)/sdk/toolchain/',
                   '>@(extra_args)',
                   '--arch', 'x86-64',
                   '--build', 'newlib_nexe',
                   '--root', '<(DEPTH)',
                   '--name', '>(out_newlib64)',
                   '--objdir', '>(objdir_newlib64)',
                   '--include-dirs=<(inst_dir)/include ^(include_dirs) >(_include_dirs)',
                   '--lib-dirs=>(lib_dirs_newlib64) ',
                   '--compile_flags=-m64 ^(newlib_tls_flags) ^(gcc_compile_flags) >(_gcc_compile_flags) ^(compile_flags) >(_compile_flags)',
                   '--defines=^(defines) >(_defines)',
                   '--link_flags=-B<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib64 ^(link_flags) >(_link_flags)',
                   '--source-list=^|(<(source_list_newlib64) ^(_sources) ^(sources))',
                 ],
               },
             ],
           }],
           ['nlib_target!="" and build_newlib!=0 and enable_x86_64!=0', {
             'variables': {
                'tool_name': 'newlib',
                'inst_dir': '<(SHARED_INTERMEDIATE_DIR)/tc_newlib',
                'objdir_newlib64%': '>(INTERMEDIATE_DIR)/<(tool_name)-x86-64/>(_target_name)',
                'source_list_newlib64%': '<(tool_name)-x86-64.>(_target_name).source_list.gypcmd',
                'out_newlib64%': '<(SHARED_INTERMEDIATE_DIR)/tc_<(tool_name)/lib64/>(nlib_target)',
             },
             'actions': [
               {
                 'action_name': 'build newlib x86-64 nlib',
                 'msvs_cygwin_shell': 0,
                 'description': 'building >(out_newlib64)',
                 'inputs': [
                    '<(DEPTH)/native_client/build/build_nexe.py',
                    '>!@pymod_do_main(>(get_sources) >(sources) >(_sources))',
                    '>@(extra_deps_newlib64)',
                    '>(source_list_newlib64)',
                    '<(SHARED_INTERMEDIATE_DIR)/sdk/toolchain/<(OS)_x86_newlib/stamp.prep',
                 ],
                 'outputs': ['>(out_newlib64)'],
                 'action': [
                   '>(python_exe)',
                   '<(DEPTH)/native_client/build/build_nexe.py',
                   '-t', '<(SHARED_INTERMEDIATE_DIR)/sdk/toolchain/',
                   '>@(extra_args)',
                   '--arch', 'x86-64',
                   '--build', 'newlib_nlib',
                   '--root', '<(DEPTH)',
                   '--name', '>(out_newlib64)',
                   '--objdir', '>(objdir_newlib64)',
                   '--include-dirs=<(inst_dir)/include ^(include_dirs) >(_include_dirs)',
                   '--lib-dirs=>(lib_dirs_newlib64) ',
                   '--compile_flags=-m64 ^(newlib_tls_flags) ^(gcc_compile_flags) >(_gcc_compile_flags) ^(compile_flags) >(_compile_flags)',
                   '--defines=^(defines) >(_defines)',
                   '--link_flags=-B<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib64 ^(link_flags) >(_link_flags)',
                   '--source-list=^|(<(source_list_newlib64) ^(_sources) ^(sources))',
                 ],
               },
             ],
           }],
           ['nexe_target!="" and build_newlib!=0 and enable_x86_32!=0', {
             'variables': {
                'tool_name': 'newlib',
                'inst_dir': '<(SHARED_INTERMEDIATE_DIR)/tc_newlib',
                'out_newlib32%': '<(PRODUCT_DIR)/>(nexe_target)_newlib_x32.nexe',
                'objdir_newlib32%': '>(INTERMEDIATE_DIR)/<(tool_name)-x86-32/>(_target_name)',
                'source_list_newlib32%': '<(tool_name)-x86-32.>(_target_name).source_list.gypcmd',
             },
             'actions': [
               {
                 'action_name': 'build newlib x86-32 nexe',
                 'msvs_cygwin_shell': 0,
                 'description': 'building >(out_newlib32)',
                 'inputs': [
                    '<(DEPTH)/native_client/build/build_nexe.py',
                    '>!@pymod_do_main(>(get_sources) >(sources) >(_sources))',
                    '>@(extra_deps_newlib32)',
                    '>(source_list_newlib32)',
                    '<(SHARED_INTERMEDIATE_DIR)/sdk/toolchain/<(OS)_x86_newlib/stamp.prep',
                 ],
                 'outputs': ['>(out_newlib32)'],
                 'action': [
                   '>(python_exe)',
                   '<(DEPTH)/native_client/build/build_nexe.py',
                   '-t', '<(SHARED_INTERMEDIATE_DIR)/sdk/toolchain/',
                   '>@(extra_args)',
                   '--arch', 'x86-32',
                   '--build', 'newlib_nexe',
                   '--root', '<(DEPTH)',
                   '--name', '>(out_newlib32)',
                   '--objdir', '>(objdir_newlib32)',
                   '--include-dirs=<(inst_dir)/include ^(include_dirs) >(_include_dirs)',
                   '--lib-dirs=>(lib_dirs_newlib32)',
                   '--compile_flags=-m32 ^(newlib_tls_flags) ^(gcc_compile_flags) >(_gcc_compile_flags) ^(compile_flags) >(_compile_flags)',
                   '--defines=^(defines) >(_defines)',
                   '--link_flags=-m32 -B<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib32 ^(link_flags) >(_link_flags)',
                   '--source-list=^|(<(source_list_newlib32) ^(_sources) ^(sources))',
                 ],
               },
             ],
           }],
           ['nlib_target!="" and build_newlib!=0 and enable_x86_32!=0', {
             'variables': {
                'tool_name': 'newlib',
                'inst_dir': '<(SHARED_INTERMEDIATE_DIR)/tc_newlib',
                'out_newlib32%': '<(SHARED_INTERMEDIATE_DIR)/tc_<(tool_name)/lib32/>(nlib_target)',
                'objdir_newlib32%': '>(INTERMEDIATE_DIR)/<(tool_name)-x86-32/>(_target_name)',
                'source_list_newlib32%': '<(tool_name)-x86-32.>(_target_name).source_list.gypcmd',
             },
             'actions': [
               {
                 'action_name': 'build newlib x86-32 nlib',
                 'msvs_cygwin_shell': 0,
                 'description': 'building >(out_newlib32)',
                 'inputs': [
                    '<(DEPTH)/native_client/build/build_nexe.py',
                    '>!@pymod_do_main(>(get_sources) >(sources) >(_sources))',
                    '>@(extra_deps_newlib32)',
                    '>(source_list_newlib32)',
                    '<(SHARED_INTERMEDIATE_DIR)/sdk/toolchain/<(OS)_x86_newlib/stamp.prep',
                 ],
                 'outputs': ['>(out_newlib32)'],
                 'action': [
                   '>(python_exe)',
                   '<(DEPTH)/native_client/build/build_nexe.py',
                   '-t', '<(SHARED_INTERMEDIATE_DIR)/sdk/toolchain/',
                   '>@(extra_args)',
                   '--arch', 'x86-32',
                   '--build', 'newlib_nlib',
                   '--root', '<(DEPTH)',
                   '--name', '>(out_newlib32)',
                   '--objdir', '>(objdir_newlib32)',
                   '--include-dirs=<(inst_dir)/include ^(include_dirs) >(_include_dirs)',
                   '--lib-dirs=>(lib_dirs_newlib32)',
                   '--compile_flags=-m32 ^(newlib_tls_flags) ^(gcc_compile_flags) >(_gcc_compile_flags) ^(compile_flags) >(_compile_flags)',
                   '--defines=^(defines) >(_defines)',
                   '--link_flags=-m32 -B<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib32 ^(link_flags) >(_link_flags)',
                   '--source-list=^|(<(source_list_newlib32) ^(_sources) ^(sources))',
                 ],
               },
             ],
           }],
        ],
      },
    }],
    ['target_arch=="arm"', {
      'target_defaults': {
        'target_conditions': [
          # GCC ARM build
          ['nacl_enable_arm_gcc!=0 and nexe_target!="" and build_newlib!=0', {
            'variables': {
               'tool_name': 'newlib',
               'inst_dir': '<(SHARED_INTERMEDIATE_DIR)/tc_newlib',
               'out_newlib_arm%': '<(PRODUCT_DIR)/>(nexe_target)_newlib_arm.nexe',
               'objdir_newlib_arm%': '>(INTERMEDIATE_DIR)/<(tool_name)-arm/>(_target_name)',
               'source_list_newlib_arm%': '<(tool_name)-arm.>(_target_name).source_list.gypcmd',
            },
            'actions': [
              {
                'action_name': 'build newlib arm nexe',
                'msvs_cygwin_shell': 0,
                'description': 'building >(out_newlib_arm)',
                'inputs': [
                   '<(DEPTH)/native_client/build/build_nexe.py',
                   '>!@pymod_do_main(>(get_sources) >(sources) >(_sources) >(native_sources))',
                   '>@(extra_deps_newlib_arm)',
                   '>(source_list_newlib_arm)',
                   '<(SHARED_INTERMEDIATE_DIR)/sdk/toolchain/<(OS)_arm_newlib/stamp.prep',
                ],
                'outputs': ['>(out_newlib_arm)'],
                'action': [
                  '>(python_exe)',
                  '<(DEPTH)/native_client/build/build_nexe.py',
                  '-t', '<(SHARED_INTERMEDIATE_DIR)/sdk/toolchain/',
                  '>@(extra_args)',
                  '--arch', 'arm',
                  '--build', 'newlib_nexe',
                  '--root', '<(DEPTH)',
                  '--name', '>(out_newlib_arm)',
                  '--objdir', '>(objdir_newlib_arm)',
                  '--include-dirs=<(inst_dir)/include ^(include_dirs) >(_include_dirs)',
                  '--lib-dirs=>(lib_dirs_newlib_arm) ',
                  '--compile_flags=^(gcc_compile_flags) >(_gcc_compile_flags) ^(compile_flags) >(_compile_flags)',
                  '--defines=^(defines) >(_defines)',
                  '--link_flags=-B<(SHARED_INTERMEDIATE_DIR)/tc_newlib/libarm ^(link_flags) >(_link_flags)',
                  '--source-list=^|(<(source_list_newlib_arm) ^(_sources) ^(sources) ^(native_sources))',
                ],
              },
            ],
          }],
          # GCC ARM library build
          ['nacl_enable_arm_gcc!=0 and nlib_target!="" and build_newlib!=0', {
            'variables': {
              'tool_name': 'newlib',
              'inst_dir': '<(SHARED_INTERMEDIATE_DIR)/tc_newlib',
              'out_newlib_arm%': '<(SHARED_INTERMEDIATE_DIR)/tc_<(tool_name)/libarm/>(nlib_target)',
              'objdir_newlib_arm%': '>(INTERMEDIATE_DIR)/<(tool_name)-arm/>(_target_name)',
              'source_list_newlib_arm%': '<(tool_name)-arm.>(_target_name).source_list.gypcmd',
            },
            'actions': [
              {
                'action_name': 'build newlib arm nlib',
                'msvs_cygwin_shell': 0,
                'description': 'building >(out_newlib_arm)',
                'inputs': [
                   '<(DEPTH)/native_client/build/build_nexe.py',
                   '>!@pymod_do_main(>(get_sources) >(sources) >(_sources) >(native_sources))',
                   '>@(extra_deps_newlib_arm)',
                   '>(source_list_newlib_arm)',
                   '<(SHARED_INTERMEDIATE_DIR)/sdk/toolchain/<(OS)_arm_newlib/stamp.prep',
                ],
                'outputs': ['>(out_newlib_arm)'],
                'action': [
                  '>(python_exe)',
                  '<(DEPTH)/native_client/build/build_nexe.py',
                  '-t', '<(SHARED_INTERMEDIATE_DIR)/sdk/toolchain/',
                  '>@(extra_args)',
                  '--arch', 'arm',
                  '--build', 'newlib_nlib',
                  '--root', '<(DEPTH)',
                  '--name', '>(out_newlib_arm)',
                  '--objdir', '>(objdir_newlib_arm)',
                  '--include-dirs=<(inst_dir)/include ^(include_dirs) >(_include_dirs)',
                  '--lib-dirs=>(lib_dirs_newlib_arm)',
                  '--compile_flags=^(gcc_compile_flags) >(_gcc_compile_flags) ^(compile_flags) >(_compile_flags)',
                  '--defines=^(defines) >(_defines)',
                  '--link_flags=-B<(SHARED_INTERMEDIATE_DIR)/tc_newlib/libarm ^(link_flags) >(_link_flags)',
                  '--source-list=^|(<(source_list_newlib_arm) ^(_sources) ^(sources) ^(native_sources))',
                ],
              },
            ],
          }],
          # pnacl ARM build is the default (unless nacl_enable_arm_gcc is set)
          ['nacl_enable_arm_gcc==0 and nexe_target!="" and build_newlib!=0', {
            'variables': {
              'tool_name': 'newlib',
              'inst_dir': '<(SHARED_INTERMEDIATE_DIR)/tc_newlib',
              'out_newlib_arm%': '<(PRODUCT_DIR)/>(nexe_target)_newlib_arm.nexe',
              'objdir_newlib_arm%': '>(INTERMEDIATE_DIR)/<(tool_name)-arm/>(_target_name)',
              'source_list_newlib_arm%': '<(tool_name)-arm.>(_target_name).source_list.gypcmd',
             },
            'actions': [
              {
                'action_name': 'build newlib arm nexe (via pnacl)',
                'msvs_cygwin_shell': 0,
                'description': 'building >(out_newlib_arm)',
                'inputs': [
                  '<(DEPTH)/native_client/build/build_nexe.py',
                  '>!@pymod_do_main(>(get_sources) >(sources) >(_sources))',
                  '>@(extra_deps_newlib_arm)',
                  '>(source_list_newlib_arm)',
                ],
                'outputs': ['>(out_newlib_arm)'],
                'action': [
                  '>(python_exe)',
                  '<(DEPTH)/native_client/build/build_nexe.py',
                  '-t', '<(SHARED_INTERMEDIATE_DIR)/sdk/toolchain/',
                  '>@(extra_args)',
                  '--arch', 'arm',
                  '--build', 'newlib_nexe_pnacl',
                  '--root', '<(DEPTH)',
                  '--name', '>(out_newlib_arm)',
                  '--objdir', '>(objdir_newlib_arm)',
                  '--include-dirs=<(inst_dir)/include ^(include_dirs) >(_include_dirs)',
                  '--lib-dirs=>(lib_dirs_newlib_arm) ',
                  '--compile_flags=--pnacl-frontend-triple=armv7-unknown-nacl-gnueabi -mfloat-abi=hard ^(compile_flags) >(_compile_flags) ^(pnacl_compile_flags) >(_pnacl_compile_flags)',
                  '--defines=^(defines) >(_defines)',
                  '--link_flags=-arch arm --pnacl-allow-translate --pnacl-allow-native -B<(SHARED_INTERMEDIATE_DIR)/tc_newlib/libarm ^(link_flags) >(_link_flags)',
                  '--source-list=^|(<(source_list_newlib_arm) ^(_sources) ^(sources))',
                 ],
              },
            ],
          }],
          # pnacl ARM library build
          ['nacl_enable_arm_gcc==0 and nlib_target!="" and build_newlib!=0', {
            'variables': {
              'tool_name': 'newlib',
              'inst_dir': '<(SHARED_INTERMEDIATE_DIR)/tc_newlib',
              'out_newlib_arm%': '<(SHARED_INTERMEDIATE_DIR)/tc_<(tool_name)/libarm/>(nlib_target)',
              'objdir_newlib_arm%': '>(INTERMEDIATE_DIR)/<(tool_name)-arm/>(_target_name)',
              'source_list_newlib_arm%': '<(tool_name)-arm.>(_target_name).source_list.gypcmd',
            },
            'actions': [
              {
                'action_name': 'build newlib arm nlib (via pnacl)',
                'msvs_cygwin_shell': 0,
                'description': 'building >(out_newlib_arm)',
                'inputs': [
                  '<(DEPTH)/native_client/build/build_nexe.py',
                  '>!@pymod_do_main(>(get_sources) >(sources) >(_sources))',
                  '>@(extra_deps_newlib_arm)',
                  '>(source_list_newlib_arm)',
                ],
                'outputs': ['>(out_newlib_arm)'],
                'action': [
                  '>(python_exe)',
                  '<(DEPTH)/native_client/build/build_nexe.py',
                  '-t', '<(SHARED_INTERMEDIATE_DIR)/sdk/toolchain/',
                  '>@(extra_args)',
                  '--arch', 'arm',
                  '--build', 'newlib_nlib_pnacl',
                  '--root', '<(DEPTH)',
                  '--name', '>(out_newlib_arm)',
                  '--objdir', '>(objdir_newlib_arm)',
                  '--include-dirs=<(inst_dir)/include ^(include_dirs) >(_include_dirs)',
                  '--lib-dirs=>(lib_dirs_newlib_arm) ',
                  '--compile_flags=--pnacl-frontend-triple=armv7-unknown-nacl-gnueabi -mfloat-abi=hard ^(compile_flags) >(_compile_flags) ^(pnacl_compile_flags) >(_pnacl_compile_flags)',
                  '--defines=^(defines) >(_defines)',
                  '--link_flags=-B<(SHARED_INTERMEDIATE_DIR)/tc_newlib/libarm ^(link_flags) >(_link_flags)',
                  '--source-list=^|(<(source_list_newlib_arm) ^(_sources) ^(sources))',
                ],
              },
            ],
          }],
        ],
      },
    }],
    ['target_arch!="arm"', {
      'target_defaults': {
        'target_conditions': [
           ['nexe_target!="" and build_glibc!=0 and enable_x86_64!=0 and disable_glibc==0', {
             'variables': {
                'tool_name': 'glibc',
                'inst_dir': '<(SHARED_INTERMEDIATE_DIR)/tc_glibc',
                'out_glibc64%': '<(PRODUCT_DIR)/>(nexe_target)_glibc_x64.nexe',
                'objdir_glibc64%': '>(INTERMEDIATE_DIR)/<(tool_name)-x86-64/>(_target_name)',
                'source_list_glibc64%': '<(tool_name)-x86-64.>(_target_name).source_list.gypcmd',
             },
             'actions': [
               {
                 'action_name': 'build glibc x86-64 nexe',
                 'msvs_cygwin_shell': 0,
                 'description': 'building >(out_glibc64)',
                 'inputs': [
                    '<(DEPTH)/native_client/build/build_nexe.py',
                    '>!@pymod_do_main(>(get_sources) >(sources) >(_sources))',
                    '>@(extra_deps_glibc64)',
                    '>(source_list_glibc64)',
                    '<(SHARED_INTERMEDIATE_DIR)/sdk/toolchain/<(OS)_x86_glibc/stamp.prep',
                 ],
                 'outputs': ['>(out_glibc64)'],
                 'action': [
                   '>(python_exe)',
                   '<(DEPTH)/native_client/build/build_nexe.py',
                   '-t', '<(SHARED_INTERMEDIATE_DIR)/sdk/toolchain/',
                   '>@(extra_args)',
                   '--arch', 'x86-64',
                   '--build', 'glibc_nexe',
                   '--root', '<(DEPTH)',
                   '--name', '>(out_glibc64)',
                   '--objdir', '>(objdir_glibc64)',
                   '--include-dirs=<(inst_dir)/include ^(include_dirs) >(_include_dirs)',
                   '--lib-dirs=>(lib_dirs_glibc64) ',
                   '--compile_flags=-m64 ^(gcc_compile_flags) >(_gcc_compile_flags) ^(compile_flags) >(_compile_flags)',
                   '--defines=^(defines) >(_defines)',
                   '--link_flags=-B<(SHARED_INTERMEDIATE_DIR)/tc_glibc/lib64 ^(link_flags) >(_link_flags)',
                   '--source-list=^|(<(source_list_glibc64) ^(_sources) ^(sources))',
                 ],
               },
             ],
           }],
           ['nexe_target!="" and build_glibc!=0 and enable_x86_32!=0 and disable_glibc==0', {
             'variables': {
                'tool_name': 'glibc',
                'inst_dir': '<(SHARED_INTERMEDIATE_DIR)/tc_glibc',
                'out_glibc32%': '<(PRODUCT_DIR)/>(nexe_target)_glibc_x32.nexe',
                'objdir_glibc32%': '>(INTERMEDIATE_DIR)/<(tool_name)-x86-32/>(_target_name)',
                'source_list_glibc32%': '<(tool_name)-x86-32.>(_target_name).source_list.gypcmd',
             },
             'actions': [
               {
                 'action_name': 'build glibc x86-32 nexe',
                 'msvs_cygwin_shell': 0,
                 'description': 'building >(out_glibc32)',
                 'inputs': [
                    '<(DEPTH)/native_client/build/build_nexe.py',
                    '>!@pymod_do_main(>(get_sources) >(sources) >(_sources))',
                    '>@(extra_deps_glibc32)',
                    '>(source_list_glibc32)',
                    '<(SHARED_INTERMEDIATE_DIR)/sdk/toolchain/<(OS)_x86_glibc/stamp.prep',
                 ],
                 'outputs': ['>(out_glibc32)'],
                 'action': [
                   '>(python_exe)',
                   '<(DEPTH)/native_client/build/build_nexe.py',
                   '-t', '<(SHARED_INTERMEDIATE_DIR)/sdk/toolchain/',
                   '>@(extra_args)',
                   '--arch', 'x86-32',
                   '--build', 'glibc_nexe',
                   '--root', '<(DEPTH)',
                   '--name', '>(out_glibc32)',
                   '--objdir', '>(objdir_glibc32)',
                   '--include-dirs=<(inst_dir)/include ^(include_dirs) >(_include_dirs)',
                   '--lib-dirs=>(lib_dirs_glibc32) ',
                   '--compile_flags=-m32 ^(gcc_compile_flags) >(_gcc_compile_flags) ^(compile_flags) >(_compile_flags)',
                   '--defines=^(defines) >(_defines)',
                   '--link_flags=-m32 -B<(SHARED_INTERMEDIATE_DIR)/tc_glibc/lib32 ^(link_flags) >(_link_flags)',
                   '--source-list=^|(<(source_list_glibc32) ^(_sources) ^(sources))',
                 ],
               },
             ],
           }],
           ['nlib_target!="" and build_glibc!=0 and enable_x86_64!=0 and disable_glibc==0', {
             'variables': {
                'tool_name': 'glibc',
                'inst_dir': '<(SHARED_INTERMEDIATE_DIR)/tc_glibc',
                'objdir_glibc64%': '>(INTERMEDIATE_DIR)/<(tool_name)-x86-64/>(_target_name)',
                'source_list_glibc64%': '<(tool_name)-x86-64.>(_target_name).source_list.gypcmd',
                'out_glibc64%': '<(SHARED_INTERMEDIATE_DIR)/tc_<(tool_name)/lib64/>(nlib_target)',
             },
             'actions': [
               {
                 'action_name': 'build glibc x86-64 nlib',
                 'msvs_cygwin_shell': 0,
                 'description': 'building >(out_glibc64)',
                 'inputs': [
                    '<(DEPTH)/native_client/build/build_nexe.py',
                    '>!@pymod_do_main(>(get_sources) >(sources) >(_sources))',
                    '>@(extra_deps_glibc64)',
                    '>(source_list_glibc64)',
                    '<(SHARED_INTERMEDIATE_DIR)/sdk/toolchain/<(OS)_x86_glibc/stamp.prep',
                 ],
                 'outputs': ['>(out_glibc64)'],
                 'action': [
                   '>(python_exe)',
                   '<(DEPTH)/native_client/build/build_nexe.py',
                   '-t', '<(SHARED_INTERMEDIATE_DIR)/sdk/toolchain/',
                   '>@(extra_args)',
                   '--arch', 'x86-64',
                   '--build', 'glibc_nlib',
                   '--root', '<(DEPTH)',
                   '--name', '>(out_glibc64)',
                   '--objdir', '>(objdir_glibc64)',
                   '--include-dirs=<(inst_dir)/include ^(include_dirs) >(_include_dirs)',
                   '--lib-dirs=>(lib_dirs_glibc64) ',
                   '--compile_flags=-m64 ^(gcc_compile_flags) >(_gcc_compile_flags) ^(compile_flags) >(_compile_flags)',
                   '--defines=^(defines) >(_defines)',
                   '--link_flags=-B<(SHARED_INTERMEDIATE_DIR)/tc_glibc/lib64 ^(link_flags) >(_link_flags)',
                   '--source-list=^|(<(source_list_glibc64) ^(_sources) ^(sources))',
                 ],
               },
             ],
           }],
           ['nlib_target!="" and build_glibc!=0 and enable_x86_32!=0 and disable_glibc==0', {
             'variables': {
                'tool_name': 'glibc',
                'inst_dir': '<(SHARED_INTERMEDIATE_DIR)/tc_glibc',
                'out_glibc32%': '<(SHARED_INTERMEDIATE_DIR)/tc_<(tool_name)/lib32/>(nlib_target)',
                'objdir_glibc32%': '>(INTERMEDIATE_DIR)/<(tool_name)-x86-32/>(_target_name)',
                'source_list_glibc32%': '<(tool_name)-x86-32.>(_target_name).source_list.gypcmd',
             },
             'actions': [
               {
                 'action_name': 'build glibc x86-32 nlib',
                 'msvs_cygwin_shell': 0,
                 'description': 'building >(out_glibc32)',
                 'inputs': [
                    '<(DEPTH)/native_client/build/build_nexe.py',
                    '>!@pymod_do_main(>(get_sources) >(sources) >(_sources))',
                    '>@(extra_deps_glibc32)',
                    '>(source_list_glibc32)',
                    '<(SHARED_INTERMEDIATE_DIR)/sdk/toolchain/<(OS)_x86_glibc/stamp.prep',
                 ],
                 'outputs': ['>(out_glibc32)'],
                 'action': [
                   '>(python_exe)',
                   '<(DEPTH)/native_client/build/build_nexe.py',
                   '-t', '<(SHARED_INTERMEDIATE_DIR)/sdk/toolchain/',
                   '>@(extra_args)',
                   '--arch', 'x86-32',
                   '--build', 'glibc_nlib',
                   '--root', '<(DEPTH)',
                   '--name', '>(out_glibc32)',
                   '--objdir', '>(objdir_glibc32)',
                   '--include-dirs=<(inst_dir)/include ^(include_dirs) >(_include_dirs)',
                   '--lib-dirs=>(lib_dirs_glibc32)',
                   '--compile_flags=-m32 ^(gcc_compile_flags) >(_gcc_compile_flags) ^(compile_flags) >(_compile_flags)',
                   '--defines=^(defines) >(_defines)',
                   '--link_flags=-m32 -B<(SHARED_INTERMEDIATE_DIR)/tc_glibc/lib32 ^(link_flags) >(_link_flags)',
                   '--source-list=^|(<(source_list_glibc32) ^(_sources) ^(sources))',
                 ],
               },
             ],
           }],
           ['nso_target!="" and build_glibc!=0 and enable_x86_64!=0 and disable_glibc==0', {
             'variables': {
                'tool_name': 'glibc',
                'inst_dir': '<(SHARED_INTERMEDIATE_DIR)/tc_glibc',
                'objdir_glibc64%': '>(INTERMEDIATE_DIR)/<(tool_name)-x86-64-so/>(_target_name)',
                'source_list_glibc64%': '<(tool_name)-x86-64-so.>(_target_name).source_list.gypcmd',
                'out_glibc64%': '<(SHARED_INTERMEDIATE_DIR)/tc_<(tool_name)/lib64/>(nso_target)',
             },
             'actions': [
               {
                 'action_name': 'build glibc x86-64 nso',
                 'msvs_cygwin_shell': 0,
                 'description': 'building >(out_glibc64)',
                 'inputs': [
                    '<(DEPTH)/native_client/build/build_nexe.py',
                    '>!@pymod_do_main(>(get_sources) >(sources) >(_sources))',
                    '>@(extra_deps_glibc64)',
                    '>(source_list_glibc64)',
                    '<(SHARED_INTERMEDIATE_DIR)/sdk/toolchain/<(OS)_x86_newlib/stamp.prep',
                 ],
                 'outputs': ['>(out_glibc64)'],
                 'action': [
                   '>(python_exe)',
                   '<(DEPTH)/native_client/build/build_nexe.py',
                   '>@(extra_args)',
                   '-t', '<(SHARED_INTERMEDIATE_DIR)/sdk/toolchain/',
                   '--arch', 'x86-64',
                   '--build', 'glibc_nso',
                   '--root', '<(DEPTH)',
                   '--name', '>(out_glibc64)',
                   '--objdir', '>(objdir_glibc64)',
                   '--include-dirs=<(inst_dir)/include ^(include_dirs) >(_include_dirs)',
                   '--lib-dirs=>(lib_dirs_glibc64) ',
                   '--compile_flags=-m64 -fPIC ^(gcc_compile_flags) >(_gcc_compile_flags) ^(compile_flags) >(_compile_flags)',
                   '--defines=^(defines) >(_defines)',
                   '--link_flags=-B<(SHARED_INTERMEDIATE_DIR)/tc_glibc/lib64 ^(link_flags) >(_link_flags)',
                   '--source-list=^|(<(source_list_glibc64) ^(_sources) ^(sources))',
                 ],
               },
             ],
           }],
           ['nso_target!="" and build_glibc!=0 and enable_x86_32!=0 and disable_glibc==0', {
             'variables': {
                'tool_name': 'glibc',
                'inst_dir': '<(SHARED_INTERMEDIATE_DIR)/tc_glibc',
                'out_glibc32%': '<(SHARED_INTERMEDIATE_DIR)/tc_<(tool_name)/lib32/>(nso_target)',
                'objdir_glibc32%': '>(INTERMEDIATE_DIR)/<(tool_name)-x86-32-so/>(_target_name)',
                'source_list_glibc32%': '<(tool_name)-x86-32-so.>(_target_name).source_list.gypcmd',
             },
             'actions': [
               {
                 'action_name': 'build glibc x86-32 nso',
                 'msvs_cygwin_shell': 0,
                 'description': 'building >(out_glibc32)',
                 'inputs': [
                    '<(DEPTH)/native_client/build/build_nexe.py',
                    '>!@pymod_do_main(>(get_sources) >(sources) >(_sources))',
                    '>@(extra_deps_glibc32)',
                    '>(source_list_glibc32)',
                    '<(SHARED_INTERMEDIATE_DIR)/sdk/toolchain/<(OS)_x86_newlib/stamp.prep',
                 ],
                 'outputs': ['>(out_glibc32)'],
                 'action': [
                   '>(python_exe)',
                   '<(DEPTH)/native_client/build/build_nexe.py',
                   '>@(extra_args)',
                   '-t', '<(SHARED_INTERMEDIATE_DIR)/sdk/toolchain/',
                   '--arch', 'x86-32',
                   '--build', 'glibc_nso',
                   '--root', '<(DEPTH)',
                   '--name', '>(out_glibc32)',
                   '--objdir', '>(objdir_glibc32)',
                   '--include-dirs=<(inst_dir)/include ^(include_dirs) >(_include_dirs)',
                   '--lib-dirs=>(lib_dirs_glibc32)',
                   '--compile_flags=-m32 -fPIC ^(gcc_compile_flags) >(_gcc_compile_flags) ^(compile_flags) >(_compile_flags)',
                   '--defines=^(defines) >(_defines)',
                   '--link_flags=-m32 -B<(SHARED_INTERMEDIATE_DIR)/tc_glibc/lib32 ^(link_flags) >(_link_flags)',
                   '--source-list=^|(<(source_list_glibc32) ^(_sources) ^(sources))',
                 ],
               },
             ],
           }],
        ],
      },
    }],
  ],
  'target_defaults': {
    'gcc_compile_flags': [],
    'pnacl_compile_flags': [],
    'variables': {
      'disable_pnacl%': 0,
      'build_pnacl_newlib': 0,
      'nlib_target': '',
      'extra_deps_pnacl_newlib': [],
      'lib_dirs_pnacl_newlib': [],
      'compile_flags': [
        '<@(nacl_default_compile_flags)',
      ],
      'gcc_compile_flags': [
        '-fomit-frame-pointer',
        # A debugger should be able to unwind IRT call frames. As the IRT is
        # compiled with high level of optimizations and without debug info,
        # compiler is requested to generate unwind tables explicitly. This
        # is the default behavior on x86-64 and when compiling C++ with
        # exceptions enabled, the change is for the benefit of x86-32 C.
        # These are only required for the IRT but are here for all
        # nacl-gcc-compiled binaries because the IRT depends on other libs
        '-fasynchronous-unwind-tables',
      ],
      'newlib_tls_flags': [
        # This option is currently only honored by x86/x64 builds.  The
        # equivalent arm option is apparently -mtp=soft but we don't need use
        # it at this point.
        '-mtls-use-call',
      ],
      'pnacl_compile_flags': [
        '-Wno-extra-semi',
        '-Wno-unused-private-field',
      ],
    },
    'target_conditions': [
      ['nexe_target!="" and disable_pnacl==0 and build_pnacl_newlib!=0', {
        'variables': {
            'out_pnacl_newlib_x86_32_nexe%': '<(PRODUCT_DIR)/>(nexe_target)_pnacl_newlib_x32.nexe',
            'out_pnacl_newlib_x86_64_nexe%': '<(PRODUCT_DIR)/>(nexe_target)_pnacl_newlib_x64.nexe',
            'out_pnacl_newlib_arm_nexe%': '<(PRODUCT_DIR)/>(nexe_target)_pnacl_newlib_arm.nexe',
          'tool_name': 'pnacl_newlib',
          'inst_dir': '<(SHARED_INTERMEDIATE_DIR)/tc_pnacl_newlib',
          'out_pnacl_newlib%': '<(PRODUCT_DIR)/>(nexe_target)_newlib.pexe',
          'objdir_pnacl_newlib%': '>(INTERMEDIATE_DIR)/<(tool_name)/>(_target_name)',
          'source_list_pnacl_newlib%': '<(tool_name).>(_target_name).source_list.gypcmd',
          'link_flags': [
            '-O3',
          ],
          'translate_flags': [],
        },
        'actions': [
          {
            'action_name': 'build newlib pexe',
            'msvs_cygwin_shell': 0,
            'description': 'building >(out_pnacl_newlib)',
            'inputs': [
              '<(DEPTH)/native_client/build/build_nexe.py',
              '>!@pymod_do_main(>(get_sources) >(sources) >(_sources))',
              '>@(extra_deps_pnacl_newlib)',
              '>(source_list_pnacl_newlib)',
              '<(SHARED_INTERMEDIATE_DIR)/sdk/toolchain/<(OS)_x86_pnacl/stamp.prep',
            ],
            'outputs': ['>(out_pnacl_newlib)'],
            'action': [
              '>(python_exe)',
              '<(DEPTH)/native_client/build/build_nexe.py',
              '-t', '<(SHARED_INTERMEDIATE_DIR)/sdk/toolchain/',
              '>@(extra_args)',
              '--arch', 'pnacl',
              '--build', 'newlib_pexe',
              '--root', '<(DEPTH)',
              '--name', '>(out_pnacl_newlib)',
              '--objdir', '>(objdir_pnacl_newlib)',
              '--include-dirs=<(inst_dir)/include ^(include_dirs) >(_include_dirs)',
              '--lib-dirs=>(lib_dirs_pnacl_newlib)',
              '--compile_flags=^(compile_flags) >(_compile_flags) ^(pnacl_compile_flags) >(_pnacl_compile_flags)',
              '--defines=^(defines) >(_defines)',
              '--link_flags=-B<(SHARED_INTERMEDIATE_DIR)/tc_pnacl_newlib/lib ^(link_flags) >(_link_flags)',
              '--source-list=^|(<(source_list_pnacl_newlib) ^(_sources) ^(sources))',
            ],
          }],
          'target_conditions': [
            [ 'enable_x86_32!=0', {
              'actions': [{
                'action_name': 'translate newlib pexe to x86-32 nexe',
                'msvs_cygwin_shell': 0,
                'description': 'translating >(out_pnacl_newlib_x86_32_nexe)',
                'inputs': [
                  # Having this in the input somehow causes devenv warnings
                  # when building pnacl browser tests.
                  # '<(DEPTH)/native_client/build/build_nexe.py',
                  '>(out_pnacl_newlib)',
                ],
                'outputs': [ '>(out_pnacl_newlib_x86_32_nexe)' ],
                'action' : [
                  '>(python_exe)',
                  '<(DEPTH)/native_client/build/build_nexe.py',
                  '-t', '<(SHARED_INTERMEDIATE_DIR)/sdk/toolchain/',
                  '--arch', 'x86-32',
                  '--build', 'newlib_translate',
                  '--root', '<(DEPTH)',
                  '--name', '>(out_pnacl_newlib_x86_32_nexe)',
                  '--link_flags=^(translate_flags) >(translate_flags) -Wl,-L<(SHARED_INTERMEDIATE_DIR)/tc_pnacl_translate/lib-x86-32',
                  '>(out_pnacl_newlib)',
                ],
              }],
            }],
            [ 'enable_x86_64!=0', {
              'actions': [{
                'action_name': 'translate newlib pexe to x86-64 nexe',
                'msvs_cygwin_shell': 0,
                'description': 'translating >(out_pnacl_newlib_x86_64_nexe)',
                'inputs': [
                  # Having this in the input somehow causes devenv warnings
                  # when building pnacl browser tests.
                  # '<(DEPTH)/native_client/build/build_nexe.py',
                  '>(out_pnacl_newlib)',
                ],
                'outputs': [ '>(out_pnacl_newlib_x86_64_nexe)' ],
                'action' : [
                  '>(python_exe)',
                  '<(DEPTH)/native_client/build/build_nexe.py',
                  '-t', '<(SHARED_INTERMEDIATE_DIR)/sdk/toolchain/',
                  '--arch', 'x86-64',
                  '--build', 'newlib_translate',
                  '--root', '<(DEPTH)',
                  '--name', '>(out_pnacl_newlib_x86_64_nexe)',
                  '--link_flags=^(translate_flags) >(translate_flags) -Wl,-L<(SHARED_INTERMEDIATE_DIR)/tc_pnacl_translate/lib-x86-64',
                  '>(out_pnacl_newlib)',
                ],
              }],
            }],
            [ 'enable_arm!=0', {
              'actions': [{
                'action_name': 'translate newlib pexe to ARM nexe',
                'msvs_cygwin_shell': 0,
                'description': 'translating >(out_pnacl_newlib_arm_nexe)',
                'inputs': [
                 # Having this in the input somehow causes devenv warnings
                 # when building pnacl browser tests.
                 # '<(DEPTH)/native_client/build/build_nexe.py',
                  '>(out_pnacl_newlib)',
                ],
                'outputs': [ '>(out_pnacl_newlib_arm_nexe)' ],
                'action' : [
                  '>(python_exe)',
                  '<(DEPTH)/native_client/build/build_nexe.py',
                  '-t', '<(SHARED_INTERMEDIATE_DIR)/sdk/toolchain/',
                  '--arch', 'arm',
                  '--build', 'newlib_translate',
                  '--root', '<(DEPTH)',
                  '--name', '>(out_pnacl_newlib_arm_nexe)',
                  '--link_flags=^(translate_flags) >(translate_flags) -Wl,-L<(SHARED_INTERMEDIATE_DIR)/tc_pnacl_translate/lib-arm',
                  '>(out_pnacl_newlib)',
                ],
              }],
            }],
          ],
      }],
      ['nlib_target!="" and disable_pnacl==0 and build_pnacl_newlib!=0', {
        'variables': {
          'tool_name': 'pnacl_newlib',
          'inst_dir': '<(SHARED_INTERMEDIATE_DIR)/tc_pnacl_newlib',
          'objdir_pnacl_newlib%': '>(INTERMEDIATE_DIR)/<(tool_name)-pnacl/>(_target_name)',
          'source_list_pnacl_newlib%': '<(tool_name).>(_target_name).source_list.gypcmd',
          'out_pnacl_newlib%': '<(SHARED_INTERMEDIATE_DIR)/tc_<(tool_name)/lib/>(nlib_target)',
        },
        'actions': [
          {
            'action_name': 'build newlib plib',
            'msvs_cygwin_shell': 0,
            'description': 'building >(out_pnacl_newlib)',
            'inputs': [
              '<(DEPTH)/native_client/build/build_nexe.py',
              '>!@pymod_do_main(>(get_sources) >(sources) >(_sources))',
              '>@(extra_deps_pnacl_newlib)',
              '>(source_list_pnacl_newlib)',
              '<(SHARED_INTERMEDIATE_DIR)/sdk/toolchain/<(OS)_x86_pnacl/stamp.prep',
            ],
            'outputs': ['>(out_pnacl_newlib)'],
            'action': [
              '>(python_exe)',
              '<(DEPTH)/native_client/build/build_nexe.py',
              '-t', '<(SHARED_INTERMEDIATE_DIR)/sdk/toolchain/',
              '>@(extra_args)',
              '--arch', 'pnacl',
              '--build', 'newlib_plib',
              '--root', '<(DEPTH)',
              '--name', '>(out_pnacl_newlib)',
              '--objdir', '>(objdir_pnacl_newlib)',
              '--include-dirs=<(inst_dir)/include ^(include_dirs) >(_include_dirs)',
              '--lib-dirs=>(lib_dirs_pnacl_newlib) ',
              '--compile_flags=^(compile_flags) >(_compile_flags) ^(pnacl_compile_flags) >(_pnacl_compile_flags)',
              '--defines=^(defines) >(_defines)',
              '--link_flags=-B<(SHARED_INTERMEDIATE_DIR)/tc_pnacl_newlib/lib ^(link_flags) >(_link_flags)',
              '--source-list=^|(<(source_list_pnacl_newlib) ^(_sources) ^(sources))',
            ],
          },
        ],
      }],
    ],
  },
}
