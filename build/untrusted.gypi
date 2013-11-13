# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'variables': {
      # Enable -Werror by default, but put it in a variable so it can
      # be optionally disabled.
      'werror%': '-Werror',

      'conditions': [
        ['"<(target_arch)"=="arm"', {
          # Settings for the newer GCC versions currently used only on ARM.
          'NACL_RODATA_FLAG%': '-Trodata-segment',
        }, {
          # Settings for the older GCC version used on x86_32 and x86_64.
          'NACL_RODATA_FLAG%': '--build-id,--section-start,.note.gnu.build-id',
        }]
      ],
    },
    # See native_client/Sconstruct for more details.
    # Expected address for beginning of data in for the IRT.
    'NACL_IRT_DATA_START': '0x3ef00000',
    # Expected address for beginning of code in for the IRT.
    'NACL_IRT_TEXT_START': '0x0fa00000',
    # Flag to pass to linker to set start of RODATA segment.
    'NACL_RODATA_FLAG%': '<(NACL_RODATA_FLAG)',
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
      '-fdiagnostics-show-option',
      '<(werror)',
    ]
  },
  'conditions': [
    ['target_arch=="ia32" or target_arch=="x64"', {
      # Common defaults for all x86 nacl-gcc targets
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
          'newlib_tls_flags': [ '-mtls-use-call' ],
          'nexe_target': '',
          'nlib_target': '',
          'nso_target': '',
          'build_newlib': 0,
          'build_glibc': 0,
          'build_irt': 0,
          'disable_glibc%': 0,
          'extra_args': [],
          'enable_x86_32': 1,
          'enable_x86_64': 1,
          'enable_arm': 0,
          'tc_lib_dir_newlib32': '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib32',
          'tc_lib_dir_newlib64': '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib64',
          'tc_lib_dir_glibc32': '<(SHARED_INTERMEDIATE_DIR)/tc_glibc/lib32',
          'tc_lib_dir_glibc64': '<(SHARED_INTERMEDIATE_DIR)/tc_glibc/lib64',
          'tc_lib_dir_irt32': '<(SHARED_INTERMEDIATE_DIR)/tc_irt/lib32',
          'tc_lib_dir_irt64': '<(SHARED_INTERMEDIATE_DIR)/tc_irt/lib64',
          'tc_include_dir_newlib': '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/include',
          'tc_include_dir_glibc': '<(SHARED_INTERMEDIATE_DIR)/tc_glibc/include',
          'extra_deps_newlib64': [],
          'extra_deps_newlib32': [],
          'extra_deps_glibc64': [],
          'extra_deps_glibc32': [],
          'include_dirs': ['<(DEPTH)'],
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
    }],
    ['target_arch=="arm"', {
      # Common defaults for all ARM nacl-gcc targets
      'target_defaults': {
        'defines': [],
        'sources': [],
        'compile_flags': [],
        'link_flags': [],
        'include_dirs': [],
        'variables': {
          'newlib_tls_flags': [ '-mtp=soft' ],
          'python_exe': 'python',
          'nexe_target': '',
          'nlib_target': '',
          'nso_target': '',
          'force_arm_pnacl': 0,
          'build_newlib': 0,
          'build_glibc': 0,
          'build_irt': 0,
          'disable_glibc%': 1,
          'extra_args': [],
          'enable_x86_32': 0,
          'enable_x86_64': 0,
          'enable_arm': 1,
          'extra_deps_newlib_arm': [],
          'native_sources': [],
          'tc_lib_dir_newlib_arm': '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/libarm',
          'tc_lib_dir_irt_arm': '<(SHARED_INTERMEDIATE_DIR)/tc_irt/libarm',
          'tc_include_dir_newlib': '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/include',
          'include_dirs': ['<(DEPTH)'],
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
    ['target_arch=="mipsel"', {
      # Common defaults for all mips pnacl-clang targets
      'target_defaults': {
        'defines': [],
        'sources': [],
        'compile_flags': [],
        'link_flags': [],
        'include_dirs': [],
        'variables': {
          'newlib_tls_flags': [],
          'python_exe': 'python',
          'nexe_target': '',
          'nlib_target': '',
          'nso_target': '',
          'build_newlib': 0,
          'build_glibc': 0,
          'build_irt': 0,
          'disable_glibc%': 1,
          'extra_args': [],
          'enable_x86_32': 0,
          'enable_x86_64': 0,
          'enable_arm': 0,
          'extra_deps_newlib_mips': [],
          'native_sources': [],
          'tc_lib_dir_newlib_mips': '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/libmips',
          'tc_lib_dir_irt_mips': '<(SHARED_INTERMEDIATE_DIR)/tc_irt/libmips',
          'tc_include_dir_newlib': '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/include',
          'include_dirs': ['<(DEPTH)'],
          'defines': [
            '<@(nacl_default_defines)',
            'NACL_BUILD_ARCH=mips',
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
    ['target_arch=="ia32" or target_arch=="x64"', {
      'target_defaults': {
        # x86-64 newlib nexe action
        'target_conditions': [
           ['nexe_target!="" and build_newlib!=0 and enable_x86_64!=0', {
             'variables': {
                'tool_name': 'newlib',
                'out_newlib64%': '<(PRODUCT_DIR)/>(nexe_target)_newlib_x64.nexe',
                'objdir_newlib64%': '>(INTERMEDIATE_DIR)/<(tool_name)-x86-64/>(_target_name)',
             },
             'actions': [
               {
                 'action_name': 'build newlib x86-64 nexe',
                 'variables': {
                   'source_list_newlib64%': '^|(<(tool_name)-x86-64.>(_target_name).source_list.gypcmd ^(_sources) ^(sources))',
                 },
                 'msvs_cygwin_shell': 0,
                 'description': 'building >(out_newlib64)',
                 'inputs': [
                    '<(DEPTH)/native_client/build/build_nexe.py',
                    '>!@pymod_do_main(>(get_sources) >(sources) >(_sources))',
                    '>@(extra_deps_newlib64)',
                    '^(source_list_newlib64)',
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
                   '--include-dirs=>(tc_include_dir_newlib) ^(include_dirs) >(_include_dirs)',
                   '--compile_flags=-m64 ^(gcc_compile_flags) >(_gcc_compile_flags) ^(compile_flags) >(_compile_flags)',
                   '--defines=^(defines) >(_defines)',
                   '--link_flags=-B>(tc_lib_dir_newlib64) ^(link_flags) >(_link_flags)',
                   '--source-list=^(source_list_newlib64)',
                 ],
               },
             ],
           }],
           # x86-64 newlib library action
           ['nlib_target!="" and build_newlib!=0 and enable_x86_64!=0', {
             'variables': {
                'tool_name': 'newlib',
                'objdir_newlib64%': '>(INTERMEDIATE_DIR)/<(tool_name)-x86-64/>(_target_name)',
                'out_newlib64%': '<(SHARED_INTERMEDIATE_DIR)/tc_<(tool_name)/lib64/>(nlib_target)',
             },
             'actions': [
               {
                 'action_name': 'build newlib x86-64 nlib',
                 'variables': {
                   'source_list_newlib64%': '^|(<(tool_name)-x86-64.>(_target_name).source_list.gypcmd ^(_sources) ^(sources))',
                 },
                 'msvs_cygwin_shell': 0,
                 'description': 'building >(out_newlib64)',
                 'inputs': [
                    '<(DEPTH)/native_client/build/build_nexe.py',
                    '>!@pymod_do_main(>(get_sources) >(sources) >(_sources))',
                    '>@(extra_deps_newlib64)',
                    '^(source_list_newlib64)',
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
                   '--include-dirs=>(tc_include_dir_newlib) ^(include_dirs) >(_include_dirs)',
                   '--compile_flags=-m64 ^(gcc_compile_flags) >(_gcc_compile_flags) ^(compile_flags) >(_compile_flags)',
                   '--defines=^(defines) >(_defines)',
                   '--link_flags=-B>(tc_lib_dir_newlib64) ^(link_flags) >(_link_flags)',
                   '--source-list=^(source_list_newlib64)',
                 ],
               },
             ],
           }],
           # x86-64 IRT nexe action
           ['nexe_target!="" and build_irt!=0 and enable_x86_64!=0', {
             'variables': {
                'tool_name': 'irt',
                'out_newlib64%': '<(PRODUCT_DIR)/>(nexe_target)_newlib_x64.nexe',
                'objdir_newlib64%': '>(INTERMEDIATE_DIR)/<(tool_name)-x86-64/>(_target_name)',
             },
             'actions': [
               {
                 'action_name': 'build IRT x86-64 nexe',
                 'variables': {
                   'source_list_newlib64%': '^|(<(tool_name)-x86-64.>(_target_name).source_list.gypcmd ^(_sources) ^(sources))',
                 },
                 'msvs_cygwin_shell': 0,
                 'description': 'building >(out_newlib64)',
                 'inputs': [
                    '<(DEPTH)/native_client/build/build_nexe.py',
                    '>!@pymod_do_main(>(get_sources) >(sources) >(_sources))',
                    '>@(extra_deps_newlib64)',
                    '^(source_list_newlib64)',
                    '<(SHARED_INTERMEDIATE_DIR)/sdk/toolchain/<(OS)_x86_newlib/stamp.prep',
                 ],
                 'outputs': ['>(out_newlib64)'],
                 'action': [
                   '>(python_exe)',
                   '<(DEPTH)/native_client/build/build_nexe.py',
                   '-t', '<(SHARED_INTERMEDIATE_DIR)/sdk/toolchain/',
                   '>@(extra_args)',
                   '--arch', 'x86-64',
                   '--build', 'newlib_nexe_pnacl',
                   '--root', '<(DEPTH)',
                   '--name', '>(out_newlib64)',
                   '--objdir', '>(objdir_newlib64)',
                   '--config-name', '<(CONFIGURATION_NAME)',
                   '--include-dirs=>(tc_include_dir_newlib) ^(include_dirs) >(_include_dirs)',
                   '--compile_flags=--target=x86_64-nacl ^(compile_flags) >(_compile_flags) -gline-tables-only ^(pnacl_compile_flags) >(_pnacl_compile_flags)',
                   '--defines=^(defines) >(_defines)',
                   '--link_flags=--target=x86_64-nacl -arch x86-64 --pnacl-allow-translate --pnacl-allow-native -Wt,-mtls-use-call -Wn,-Trodata-segment=<(NACL_IRT_DATA_START) -Wn,-Ttext-segment=<(NACL_IRT_TEXT_START) -B>(tc_lib_dir_irt64) ^(link_flags) >(_link_flags)',
                   '--source-list=^(source_list_newlib64)',
                 ],
               },
             ],
           }],
           # x86-64 IRT library action
           ['nlib_target!="" and build_irt!=0 and enable_x86_64!=0', {
             'variables': {
                'tool_name': 'irt',
                'objdir_newlib64%': '>(INTERMEDIATE_DIR)/<(tool_name)-x86-64/>(_target_name)',
                'out_newlib64%': '<(SHARED_INTERMEDIATE_DIR)/tc_<(tool_name)/lib64/>(nlib_target)',
             },
             'actions': [
               {
                 'action_name': 'build irt x86-64 nlib',
                 'variables': {
                   'source_list_newlib64%': '^|(<(tool_name)-x86-64.>(_target_name).source_list.gypcmd ^(_sources) ^(sources))',
                 },
                 'msvs_cygwin_shell': 0,
                 'description': 'building >(out_newlib64)',
                 'inputs': [
                    '<(DEPTH)/native_client/build/build_nexe.py',
                    '>!@pymod_do_main(>(get_sources) >(sources) >(_sources))',
                    '>@(extra_deps_newlib64)',
                    '^(source_list_newlib64)',
                    '<(SHARED_INTERMEDIATE_DIR)/sdk/toolchain/<(OS)_x86_newlib/stamp.prep',
                 ],
                 'outputs': ['>(out_newlib64)'],
                 'action': [
                   '>(python_exe)',
                   '<(DEPTH)/native_client/build/build_nexe.py',
                   '-t', '<(SHARED_INTERMEDIATE_DIR)/sdk/toolchain/',
                   '>@(extra_args)',
                   '--arch', 'x86-64',
                   '--build', 'newlib_nlib_pnacl',
                   '--root', '<(DEPTH)',
                   '--name', '>(out_newlib64)',
                   '--objdir', '>(objdir_newlib64)',
                   '--config-name', '<(CONFIGURATION_NAME)',
                   '--include-dirs=>(tc_include_dir_newlib) ^(include_dirs) >(_include_dirs)',
                   '--compile_flags=--target=x86_64-nacl ^(compile_flags) >(_compile_flags) -gline-tables-only ^(pnacl_compile_flags) >(_pnacl_compile_flags)',
                   '--defines=^(defines) >(_defines)',
                   '--link_flags=--target=x86_64-nacl -B>(tc_lib_dir_irt64) ^(link_flags) >(_link_flags)',
                   '--source-list=^(source_list_newlib64)',
                 ],
               },
             ],
           }],
           # x86-32 newlib nexe action
           ['nexe_target!="" and build_newlib!=0 and enable_x86_32!=0', {
             'variables': {
                'tool_name': 'newlib',
                'out_newlib32%': '<(PRODUCT_DIR)/>(nexe_target)_newlib_x32.nexe',
                'objdir_newlib32%': '>(INTERMEDIATE_DIR)/<(tool_name)-x86-32/>(_target_name)',
             },
             'actions': [
               {
                 'action_name': 'build newlib x86-32 nexe',
                 'variables': {
                   'source_list_newlib32%': '^|(<(tool_name)-x86-32.>(_target_name).source_list.gypcmd ^(_sources) ^(sources))',
                 },
                 'msvs_cygwin_shell': 0,
                 'description': 'building >(out_newlib32)',
                 'inputs': [
                    '<(DEPTH)/native_client/build/build_nexe.py',
                    '>!@pymod_do_main(>(get_sources) >(sources) >(_sources))',
                    '>@(extra_deps_newlib32)',
                    '^(source_list_newlib32)',
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
                   '--include-dirs=>(tc_include_dir_newlib) ^(include_dirs) >(_include_dirs)',
                   '--compile_flags=-m32 ^(gcc_compile_flags) >(_gcc_compile_flags) ^(compile_flags) >(_compile_flags)',
                   '--defines=^(defines) >(_defines)',
                   '--link_flags=-m32 -B>(tc_lib_dir_newlib32) ^(link_flags) >(_link_flags)',
                   '--source-list=^(source_list_newlib32)',
                 ],
               },
             ],
           }],
           # x86-32 newlib library action
           ['nlib_target!="" and build_newlib!=0 and enable_x86_32!=0', {
             'variables': {
                'tool_name': 'newlib',
                'out_newlib32%': '<(SHARED_INTERMEDIATE_DIR)/tc_<(tool_name)/lib32/>(nlib_target)',
                'objdir_newlib32%': '>(INTERMEDIATE_DIR)/<(tool_name)-x86-32/>(_target_name)',
             },
             'actions': [
               {
                 'action_name': 'build newlib x86-32 nlib',
                 'variables': {
                   'source_list_newlib32%': '^|(<(tool_name)-x86-32.>(_target_name).source_list.gypcmd ^(_sources) ^(sources))',
                 },
                 'msvs_cygwin_shell': 0,
                 'description': 'building >(out_newlib32)',
                 'inputs': [
                    '<(DEPTH)/native_client/build/build_nexe.py',
                    '>!@pymod_do_main(>(get_sources) >(sources) >(_sources))',
                    '>@(extra_deps_newlib32)',
                    '^(source_list_newlib32)',
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
                   '--include-dirs=>(tc_include_dir_newlib) ^(include_dirs) >(_include_dirs)',
                   '--compile_flags=-m32 ^(gcc_compile_flags) >(_gcc_compile_flags) ^(compile_flags) >(_compile_flags)',
                   '--defines=^(defines) >(_defines)',
                   '--link_flags=-m32 -B>(tc_lib_dir_newlib32) ^(link_flags) >(_link_flags)',
                   '--source-list=^(source_list_newlib32)',
                 ],
               },
             ],
           }],
           # x86-32 IRT nexe build
           ['nexe_target!="" and build_irt!=0 and enable_x86_32!=0', {
             'variables': {
                'tool_name': 'irt',
                'out_newlib32%': '<(PRODUCT_DIR)/>(nexe_target)_newlib_x32.nexe',
                'objdir_newlib32%': '>(INTERMEDIATE_DIR)/<(tool_name)-x86-32/>(_target_name)',
             },
             'actions': [
               {
                 'action_name': 'build IRT x86-32 nexe',
                 'variables': {
                   'source_list_newlib32%': '^|(<(tool_name)-x86-32.>(_target_name).source_list.gypcmd ^(_sources) ^(sources))',
                 },
                 'msvs_cygwin_shell': 0,
                 'description': 'building >(out_newlib32)',
                 'inputs': [
                    '<(DEPTH)/native_client/build/build_nexe.py',
                    '>!@pymod_do_main(>(get_sources) >(sources) >(_sources))',
                    '>@(extra_deps_newlib32)',
                    '^(source_list_newlib32)',
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
                   '--include-dirs=>(tc_include_dir_newlib) ^(include_dirs) >(_include_dirs)',
                   '--compile_flags=-m32 ^(newlib_tls_flags) ^(gcc_compile_flags) >(_gcc_compile_flags) ^(compile_flags) >(_compile_flags)',
                   '--defines=^(defines) >(_defines)',
                   '--link_flags=-m32 -B>(tc_lib_dir_irt32) -Wl,--build-id,--section-start,.note.gnu.build-id=<(NACL_IRT_DATA_START) -Wl,-Ttext-segment=<(NACL_IRT_TEXT_START) ^(link_flags) >(_link_flags)',
                   '--source-list=^(source_list_newlib32)',
                 ],
               },
             ],
           }],
           # x86-32 IRT library build
           ['nlib_target!="" and build_irt!=0 and enable_x86_32!=0', {
             'variables': {
                'tool_name': 'irt',
                'out_newlib32%': '<(SHARED_INTERMEDIATE_DIR)/tc_<(tool_name)/lib32/>(nlib_target)',
                'objdir_newlib32%': '>(INTERMEDIATE_DIR)/<(tool_name)-x86-32/>(_target_name)',
             },
             'actions': [
               {
                 'action_name': 'build IRT x86-32 nlib',
                 'variables': {
                   'source_list_newlib32%': '^|(<(tool_name)-x86-32.>(_target_name).source_list.gypcmd ^(_sources) ^(sources))',
                 },
                 'msvs_cygwin_shell': 0,
                 'description': 'building >(out_newlib32)',
                 'inputs': [
                    '<(DEPTH)/native_client/build/build_nexe.py',
                    '>!@pymod_do_main(>(get_sources) >(sources) >(_sources))',
                    '>@(extra_deps_newlib32)',
                    '^(source_list_newlib32)',
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
                   '--include-dirs=>(tc_include_dir_newlib) ^(include_dirs) >(_include_dirs)',
                   '--compile_flags=-m32 ^(newlib_tls_flags) ^(gcc_compile_flags) >(_gcc_compile_flags) ^(compile_flags) >(_compile_flags)',
                   '--defines=^(defines) >(_defines)',
                   '--link_flags=-m32 -B>(tc_lib_dir_irt32) ^(link_flags) >(_link_flags)',
                   '--source-list=^(source_list_newlib32)',
                 ],
               },
             ],
           }],
        ],
      },
    }], # end x86 gcc nexe/nlib actions
    ['target_arch=="arm"', {
      'target_defaults': {
        'target_conditions': [
          # arm newlib nexe action
          ['force_arm_pnacl==0 and nexe_target!="" and build_newlib!=0', {
            'variables': {
               'tool_name': 'newlib',
               'out_newlib_arm%': '<(PRODUCT_DIR)/>(nexe_target)_newlib_arm.nexe',
               'objdir_newlib_arm%': '>(INTERMEDIATE_DIR)/<(tool_name)-arm/>(_target_name)',
            },
            'actions': [
              {
                'action_name': 'build newlib arm nexe',
                'variables': {
                  'source_list_newlib_arm%': '^|(<(tool_name)-arm.>(_target_name).source_list.gypcmd ^(_sources) ^(sources) ^(native_sources))',
                },
                'msvs_cygwin_shell': 0,
                'description': 'building >(out_newlib_arm)',
                'inputs': [
                   '<(DEPTH)/native_client/build/build_nexe.py',
                   '>!@pymod_do_main(>(get_sources) >(sources) >(_sources) >(native_sources))',
                   '>@(extra_deps_newlib_arm)',
                   '^(source_list_newlib_arm)',
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
                  '--include-dirs=>(tc_include_dir_newlib) ^(include_dirs) >(_include_dirs)',
                  '--compile_flags=-Wno-unused-local-typedefs -Wno-psabi ^(gcc_compile_flags) >(_gcc_compile_flags) ^(compile_flags) >(_compile_flags)',
                  '--defines=^(defines) >(_defines)',
                  '--link_flags=-B>(tc_lib_dir_newlib_arm) ^(link_flags) >(_link_flags)',
                  '--source-list=^(source_list_newlib_arm)',
                ],
              },
            ],
          }],
          # arm newlib library action
          ['force_arm_pnacl==0 and nlib_target!="" and build_newlib!=0', {
            'variables': {
              'tool_name': 'newlib',
              'out_newlib_arm%': '<(SHARED_INTERMEDIATE_DIR)/tc_<(tool_name)/libarm/>(nlib_target)',
              'objdir_newlib_arm%': '>(INTERMEDIATE_DIR)/<(tool_name)-arm/>(_target_name)',
            },
            'actions': [
              {
                'action_name': 'build newlib arm nlib',
                 'variables': {
                   'source_list_newlib_arm%': '^|(<(tool_name)-arm.>(_target_name).source_list.gypcmd ^(_sources) ^(sources) ^(native_sources))',
                 },
                'msvs_cygwin_shell': 0,
                'description': 'building >(out_newlib_arm)',
                'inputs': [
                   '<(DEPTH)/native_client/build/build_nexe.py',
                   '>!@pymod_do_main(>(get_sources) >(sources) >(_sources) >(native_sources))',
                   '>@(extra_deps_newlib_arm)',
                   '^(source_list_newlib_arm)',
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
                  '--include-dirs=>(tc_include_dir_newlib) ^(include_dirs) >(_include_dirs)',
                  '--compile_flags=-Wno-unused-local-typedefs -Wno-psabi ^(gcc_compile_flags) >(_gcc_compile_flags) ^(compile_flags) >(_compile_flags)',
                  '--defines=^(defines) >(_defines)',
                  '--link_flags=-B>(tc_lib_dir_newlib_arm) ^(link_flags) >(_link_flags)',
                  '--source-list=^(source_list_newlib_arm)',
                ],
              },
            ],
          }],
          # arm irt nexe action
          ['force_arm_pnacl==0 and nexe_target!="" and build_irt!=0', {
            'variables': {
               'tool_name': 'irt',
               'out_newlib_arm%': '<(PRODUCT_DIR)/>(nexe_target)_newlib_arm.nexe',
               'objdir_newlib_arm%': '>(INTERMEDIATE_DIR)/<(tool_name)-arm/>(_target_name)',
            },
            'actions': [
              {
                'action_name': 'build IRT arm nexe',
                'variables': {
                  'source_list_newlib_arm%': '^|(<(tool_name)-arm.>(_target_name).source_list.gypcmd ^(_sources) ^(sources) ^(native_sources))',
                },
                'msvs_cygwin_shell': 0,
                'description': 'building >(out_newlib_arm)',
                'inputs': [
                   '<(DEPTH)/native_client/build/build_nexe.py',
                   '>!@pymod_do_main(>(get_sources) >(sources) >(_sources) >(native_sources))',
                   '>@(extra_deps_newlib_arm)',
                   '^(source_list_newlib_arm)',
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
                  '--include-dirs=>(tc_include_dir_newlib) ^(include_dirs) >(_include_dirs)',
                  '--compile_flags=-Wno-unused-local-typedefs -Wno-psabi ^(newlib_tls_flags) ^(gcc_compile_flags) >(_gcc_compile_flags) ^(compile_flags) >(_compile_flags)',
                  '--defines=^(defines) >(_defines)',
                  '--link_flags=-B>(tc_lib_dir_irt_arm) -Wl,-Trodata-segment=<(NACL_IRT_DATA_START) -Wl,-Ttext-segment=<(NACL_IRT_TEXT_START) ^(link_flags) >(_link_flags)',
                  '--source-list=^(source_list_newlib_arm)',
                ],
              },
            ],
          }],
          # arm IRT library action
          ['force_arm_pnacl==0 and nlib_target!="" and build_irt!=0', {
            'variables': {
              'tool_name': 'irt',
              'out_newlib_arm%': '<(SHARED_INTERMEDIATE_DIR)/tc_<(tool_name)/libarm/>(nlib_target)',
              'objdir_newlib_arm%': '>(INTERMEDIATE_DIR)/<(tool_name)-arm/>(_target_name)',
            },
            'actions': [
              {
                'action_name': 'build IRT arm nlib',
                'variables': {
                  'source_list_newlib_arm%': '^|(<(tool_name)-arm.>(_target_name).source_list.gypcmd ^(_sources) ^(sources) ^(native_sources))',
                },
                'msvs_cygwin_shell': 0,
                'description': 'building >(out_newlib_arm)',
                'inputs': [
                   '<(DEPTH)/native_client/build/build_nexe.py',
                   '>!@pymod_do_main(>(get_sources) >(sources) >(_sources) >(native_sources))',
                   '>@(extra_deps_newlib_arm)',
                   '^(source_list_newlib_arm)',
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
                  '--include-dirs=>(tc_include_dir_newlib) ^(include_dirs) >(_include_dirs)',
                  '--compile_flags=-Wno-unused-local-typedefs -Wno-psabi ^(newlib_tls_flags) ^(gcc_compile_flags) >(_gcc_compile_flags) ^(compile_flags) >(_compile_flags)',
                  '--defines=^(defines) >(_defines)',
                  '--link_flags=-B>(tc_lib_dir_irt_arm) ^(link_flags) >(_link_flags)',
                  '--source-list=^(source_list_newlib_arm)',
                ],
              },
            ],
          }],
          # pnacl ARM library build using biased bitcode. This is currently
          # used to build the IRT shim. TODO(dschuff): see if this can be
          # further simplified, perhaps using re-using the plib build below
          ['force_arm_pnacl==1 and nlib_target!="" and build_newlib!=0', {
            'variables': {
              'tool_name': 'newlib',
              'out_newlib_arm%': '<(SHARED_INTERMEDIATE_DIR)/tc_<(tool_name)/libarm/>(nlib_target)',
              'objdir_newlib_arm%': '>(INTERMEDIATE_DIR)/<(tool_name)-arm/>(_target_name)',
            },
            'actions': [
              {
                'action_name': 'build newlib arm nlib (via pnacl)',
                'variables': {
                  'source_list_newlib_arm%': '^|(<(tool_name)-arm.>(_target_name).source_list.gypcmd ^(_sources) ^(sources))',
                },
                'msvs_cygwin_shell': 0,
                'description': 'building >(out_newlib_arm)',
                'inputs': [
                  '<(DEPTH)/native_client/build/build_nexe.py',
                  '>!@pymod_do_main(>(get_sources) >(sources) >(_sources))',
                  '>@(extra_deps_newlib_arm)',
                  '^(source_list_newlib_arm)',
                  '<(SHARED_INTERMEDIATE_DIR)/sdk/toolchain/<(OS)_pnacl/stamp.prep'
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
                  '--include-dirs=>(tc_include_dir_newlib) ^(include_dirs) >(_include_dirs)',
                  '--compile_flags=--target=armv7-unknown-nacl-gnueabi -mfloat-abi=hard ^(compile_flags) >(_compile_flags) ^(pnacl_compile_flags) >(_pnacl_compile_flags)',
                  '--defines=^(defines) >(_defines)',
                  '--link_flags=-B>(tc_lib_dir_newlib_arm) ^(link_flags) >(_link_flags)',
                  '--source-list=^(source_list_newlib_arm)',
                ],
              },
            ],
          }],
        ], # end target_conditions for arm newlib (nexe/nlib, force_arm_pnacl)
      },
    }], # end target_arch = arm
    ['target_arch=="mipsel"', {
      'target_defaults': {
        'target_conditions': [
          # mips newlib nexe action
          ['nexe_target!="" and build_newlib!=0', {
            'variables': {
               'tool_name': 'newlib',
               'out_newlib_mips%': '<(PRODUCT_DIR)/>(nexe_target)_newlib_mips.nexe',
               'objdir_newlib_mips%': '>(INTERMEDIATE_DIR)/<(tool_name)-mips/>(_target_name)',
            },
            'actions': [
              {
                'action_name': 'build newlib mips nexe',
                'variables': {
                  'source_list_newlib_mips%': '^|(<(tool_name)-mips.>(_target_name).source_list.gypcmd ^(_sources) ^(sources) ^(native_sources))',
                },
                'msvs_cygwin_shell': 0,
                'description': 'building >(out_newlib_mips)',
                'inputs': [
                   '<(DEPTH)/native_client/build/build_nexe.py',
                   '>!@pymod_do_main(>(get_sources) >(sources) >(_sources) >(native_sources))',
                   '>@(extra_deps_newlib_mips)',
                   '^(source_list_newlib_mips)',
                   '<(SHARED_INTERMEDIATE_DIR)/sdk/toolchain/<(OS)_pnacl/stamp.prep',
                ],
                'outputs': ['>(out_newlib_mips)'],
                'action': [
                  '>(python_exe)',
                  '<(DEPTH)/native_client/build/build_nexe.py',
                  '-t', '<(SHARED_INTERMEDIATE_DIR)/sdk/toolchain/',
                  '>@(extra_args)',
                  '--arch', 'mips',
                  '--build', 'newlib_nexe',
                  '--root', '<(DEPTH)',
                  '--name', '>(out_newlib_mips)',
                  '--objdir', '>(objdir_newlib_mips)',
                  '--include-dirs=>(tc_include_dir_newlib) ^(include_dirs) >(_include_dirs)',
                  '--compile_flags= ^(pnacl_compile_flags) >(_pnacl_compile_flags) ^(compile_flags) >(_compile_flags)',
                  '--defines=^(defines) >(_defines)',
                  '--link_flags=-B>(tc_lib_dir_newlib_mips) ^(link_flags) >(_link_flags)',
                  '--source-list=^(source_list_newlib_mips)',
                ],
              },
            ],
          }],
          # mips newlib library action
          ['nlib_target!="" and build_newlib!=0', {
            'variables': {
              'tool_name': 'newlib',
              'out_newlib_mips%': '<(SHARED_INTERMEDIATE_DIR)/tc_<(tool_name)/libmips/>(nlib_target)',
              'objdir_newlib_mips%': '>(INTERMEDIATE_DIR)/<(tool_name)-mips/>(_target_name)',
            },
            'actions': [
              {
                'action_name': 'build newlib mips nlib',
                'variables': {
                  'source_list_newlib_mips%': '^|(<(tool_name)-mips.>(_target_name).source_list.gypcmd ^(_sources) ^(sources))',
                },
                'msvs_cygwin_shell': 0,
                'description': 'building >(out_newlib_mips)',
                'inputs': [
                   '<(DEPTH)/native_client/build/build_nexe.py',
                   '>!@pymod_do_main(>(get_sources) >(sources) >(_sources))',
                   '>@(extra_deps_newlib_mips)',
                   '^(source_list_newlib_mips)',
                   '<(SHARED_INTERMEDIATE_DIR)/sdk/toolchain/<(OS)_pnacl/stamp.prep',
                ],
                'outputs': ['>(out_newlib_mips)'],
                'action': [
                  '>(python_exe)',
                  '<(DEPTH)/native_client/build/build_nexe.py',
                  '-t', '<(SHARED_INTERMEDIATE_DIR)/sdk/toolchain/',
                  '>@(extra_args)',
                  '--arch', 'mips',
                  '--build', 'newlib_nlib',
                  '--root', '<(DEPTH)',
                  '--name', '>(out_newlib_mips)',
                  '--objdir', '>(objdir_newlib_mips)',
                  '--include-dirs=>(tc_include_dir_newlib) ^(include_dirs) >(_include_dirs)',
                  '--compile_flags= ^(compile_flags) >(_compile_flags) ^(pnacl_compile_flags) >(_pnacl_compile_flags)',
                  '--defines=^(defines) >(_defines)',
                  '--link_flags=-B>(tc_lib_dir_newlib_mips) ^(link_flags) >(_link_flags)',
                  '--source-list=^(source_list_newlib_mips)',
                ],
              },
            ],
          }],
          # mips irt nexe action
          ['nexe_target!="" and build_irt!=0', {
            'variables': {
               'tool_name': 'irt',
               'out_newlib_mips%': '<(PRODUCT_DIR)/>(nexe_target)_newlib_mips.nexe',
               'objdir_newlib_mips%': '>(INTERMEDIATE_DIR)/<(tool_name)-mips/>(_target_name)',
            },
            'actions': [
              {
                'action_name': 'build IRT mips nexe',
                'variables': {
                  'source_list_newlib_mips%': '^|(<(tool_name)-mips.>(_target_name).source_list.gypcmd ^(_sources) ^(sources))',
                },
                'msvs_cygwin_shell': 0,
                'description': 'building >(out_newlib_mips)',
                'inputs': [
                   '<(DEPTH)/native_client/build/build_nexe.py',
                   '>!@pymod_do_main(>(get_sources) >(sources) >(_sources))',
                   '>@(extra_deps_newlib_mips)',
                   '^(source_list_newlib_mips)',
                   '<(SHARED_INTERMEDIATE_DIR)/sdk/toolchain/<(OS)_pnacl/stamp.prep',
                ],
                'outputs': ['>(out_newlib_mips)'],
                'action': [
                  '>(python_exe)',
                  '<(DEPTH)/native_client/build/build_nexe.py',
                  '-t', '<(SHARED_INTERMEDIATE_DIR)/sdk/toolchain/',
                  '>@(extra_args)',
                  '--arch', 'mips',
                  '--build', 'newlib_nexe',
                  '--root', '<(DEPTH)',
                  '--name', '>(out_newlib_mips)',
                  '--objdir', '>(objdir_newlib_mips)',
                  '--include-dirs=>(tc_include_dir_newlib) ^(include_dirs) >(_include_dirs)',
                  '--compile_flags= ^(compile_flags) >(_compile_flags) ^(pnacl_compile_flags) >(_pnacl_compile_flags)',
                  '--defines=^(defines) >(_defines)',
                  '--link_flags= -arch mips --pnacl-allow-translate --pnacl-allow-native -Wt,-mtls-use-call --pnacl-disable-abi-check -Wl,-Trodata-segment=<(NACL_IRT_DATA_START) -Wl,-Ttext-segment=<(NACL_IRT_TEXT_START) -B>(tc_lib_dir_irt_mips) ^(link_flags) >(_link_flags)',
                  '--source-list=^(source_list_newlib_mips)',
                ],
              },
            ],
          }],
          # mips IRT library action
          ['nlib_target!="" and build_irt!=0', {
            'variables': {
              'tool_name': 'irt',
              'out_newlib_mips%': '<(SHARED_INTERMEDIATE_DIR)/tc_<(tool_name)/libmips/>(nlib_target)',
              'objdir_newlib_mips%': '>(INTERMEDIATE_DIR)/<(tool_name)-mips/>(_target_name)',
            },
            'actions': [
              {
                'action_name': 'build IRT mips nlib',
                'variables': {
                  'source_list_newlib_mips%': '^|(<(tool_name)-mips.>(_target_name).source_list.gypcmd ^(_sources) ^(sources))',
                },
                'msvs_cygwin_shell': 0,
                'description': 'building >(out_newlib_mips)',
                'inputs': [
                   '<(DEPTH)/native_client/build/build_nexe.py',
                   '>!@pymod_do_main(>(get_sources) >(sources) >(_sources))',
                   '>@(extra_deps_newlib_mips)',
                   '^(source_list_newlib_mips)',
                   '<(SHARED_INTERMEDIATE_DIR)/sdk/toolchain/<(OS)_pnacl/stamp.prep',
                ],
                'outputs': ['>(out_newlib_mips)'],
                'action': [
                  '>(python_exe)',
                  '<(DEPTH)/native_client/build/build_nexe.py',
                  '-t', '<(SHARED_INTERMEDIATE_DIR)/sdk/toolchain/',
                  '>@(extra_args)',
                  '--arch', 'mips',
                  '--build', 'newlib_nlib',
                  '--root', '<(DEPTH)',
                  '--name', '>(out_newlib_mips)',
                  '--objdir', '>(objdir_newlib_mips)',
                  '--include-dirs=>(tc_include_dir_newlib) ^(include_dirs) >(_include_dirs)',
                  '--compile_flags= ^(compile_flags) >(_compile_flags) ^(pnacl_compile_flags) >(_pnacl_compile_flags)',
                  '--defines=^(defines) >(_defines)',
                  '--link_flags=-B>(tc_lib_dir_irt_mips) ^(link_flags) >(_link_flags)',
                  '--source-list=^(source_list_newlib_mips)',
                ],
              },
            ],
          }],
        ], # end target_conditions for mips newlib
      },
    }], # end target_arch = mips
    ['target_arch=="ia32" or target_arch=="x64"', {
      'target_defaults': {
        # x86-64 glibc nexe action
        'target_conditions': [
           ['nexe_target!="" and build_glibc!=0 and enable_x86_64!=0 and disable_glibc==0', {
             'variables': {
                'tool_name': 'glibc',
                'out_glibc64%': '<(PRODUCT_DIR)/>(nexe_target)_glibc_x64.nexe',
                'objdir_glibc64%': '>(INTERMEDIATE_DIR)/<(tool_name)-x86-64/>(_target_name)',
             },
             'actions': [
               {
                 'action_name': 'build glibc x86-64 nexe',
                 'variables': {
                   'source_list_glibc64%': '^|(<(tool_name)-x86-64.>(_target_name).source_list.gypcmd ^(_sources) ^(sources))',
                 },
                 'msvs_cygwin_shell': 0,
                 'description': 'building >(out_glibc64)',
                 'inputs': [
                    '<(DEPTH)/native_client/build/build_nexe.py',
                    '>!@pymod_do_main(>(get_sources) >(sources) >(_sources))',
                    '>@(extra_deps_glibc64)',
                    '^(source_list_glibc64)',
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
                   '--include-dirs=>(tc_include_dir_glibc) ^(include_dirs) >(_include_dirs)',
                   '--compile_flags=-m64 ^(gcc_compile_flags) >(_gcc_compile_flags) ^(compile_flags) >(_compile_flags)',
                   '--defines=^(defines) >(_defines)',
                   '--link_flags=-B>(tc_lib_dir_glibc64) ^(link_flags) >(_link_flags)',
                   '--source-list=^(source_list_glibc64)',
                 ],
               },
             ],
           }],
           # x86-32 glibc nexe action
           ['nexe_target!="" and build_glibc!=0 and enable_x86_32!=0 and disable_glibc==0', {
             'variables': {
                'tool_name': 'glibc',
                'out_glibc32%': '<(PRODUCT_DIR)/>(nexe_target)_glibc_x32.nexe',
                'objdir_glibc32%': '>(INTERMEDIATE_DIR)/<(tool_name)-x86-32/>(_target_name)',
             },
             'actions': [
               {
                 'action_name': 'build glibc x86-32 nexe',
                 'variables': {
                   'source_list_glibc32%': '^|(<(tool_name)-x86-32.>(_target_name).source_list.gypcmd ^(_sources) ^(sources))',
                 },
                 'msvs_cygwin_shell': 0,
                 'description': 'building >(out_glibc32)',
                 'inputs': [
                    '<(DEPTH)/native_client/build/build_nexe.py',
                    '>!@pymod_do_main(>(get_sources) >(sources) >(_sources))',
                    '>@(extra_deps_glibc32)',
                    '^(source_list_glibc32)',
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
                   '--include-dirs=>(tc_include_dir_glibc) ^(include_dirs) >(_include_dirs)',
                   '--compile_flags=-m32 ^(gcc_compile_flags) >(_gcc_compile_flags) ^(compile_flags) >(_compile_flags)',
                   '--defines=^(defines) >(_defines)',
                   '--link_flags=-m32 -B>(tc_lib_dir_glibc32) ^(link_flags) >(_link_flags)',
                   '--source-list=^(source_list_glibc32)',
                 ],
               },
             ],
           }],
           # x86-64 glibc static library action
           ['nlib_target!="" and build_glibc!=0 and enable_x86_64!=0 and disable_glibc==0', {
             'variables': {
                'tool_name': 'glibc',
                'objdir_glibc64%': '>(INTERMEDIATE_DIR)/<(tool_name)-x86-64/>(_target_name)',
                'out_glibc64%': '<(SHARED_INTERMEDIATE_DIR)/tc_<(tool_name)/lib64/>(nlib_target)',
             },
             'actions': [
               {
                 'action_name': 'build glibc x86-64 nlib',
                 'variables': {
                   'source_list_glibc64%': '^|(<(tool_name)-x86-64.>(_target_name).source_list.gypcmd ^(_sources) ^(sources))',
                 },
                 'msvs_cygwin_shell': 0,
                 'description': 'building >(out_glibc64)',
                 'inputs': [
                    '<(DEPTH)/native_client/build/build_nexe.py',
                    '>!@pymod_do_main(>(get_sources) >(sources) >(_sources))',
                    '>@(extra_deps_glibc64)',
                    '^(source_list_glibc64)',
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
                   '--include-dirs=>(tc_include_dir_glibc) ^(include_dirs) >(_include_dirs)',
                   '--compile_flags=-m64 ^(gcc_compile_flags) >(_gcc_compile_flags) ^(compile_flags) >(_compile_flags)',
                   '--defines=^(defines) >(_defines)',
                   '--link_flags=-B>(tc_lib_dir_glibc64) ^(link_flags) >(_link_flags)',
                   '--source-list=^(source_list_glibc64)',
                 ],
               },
             ],
           }],
           # x86-32 glibc static library action
           ['nlib_target!="" and build_glibc!=0 and enable_x86_32!=0 and disable_glibc==0', {
             'variables': {
                'tool_name': 'glibc',
                'out_glibc32%': '<(SHARED_INTERMEDIATE_DIR)/tc_<(tool_name)/lib32/>(nlib_target)',
                'objdir_glibc32%': '>(INTERMEDIATE_DIR)/<(tool_name)-x86-32/>(_target_name)',
             },
             'actions': [
               {
                 'action_name': 'build glibc x86-32 nlib',
                 'variables': {
                   'source_list_glibc32%': '^|(<(tool_name)-x86-32.>(_target_name).source_list.gypcmd ^(_sources) ^(sources))',
                 },
                 'msvs_cygwin_shell': 0,
                 'description': 'building >(out_glibc32)',
                 'inputs': [
                    '<(DEPTH)/native_client/build/build_nexe.py',
                    '>!@pymod_do_main(>(get_sources) >(sources) >(_sources))',
                    '>@(extra_deps_glibc32)',
                    '^(source_list_glibc32)',
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
                   '--include-dirs=>(tc_include_dir_glibc) ^(include_dirs) >(_include_dirs)',
                   '--compile_flags=-m32 ^(gcc_compile_flags) >(_gcc_compile_flags) ^(compile_flags) >(_compile_flags)',
                   '--defines=^(defines) >(_defines)',
                   '--link_flags=-m32 -B>(tc_lib_dir_glibc32) ^(link_flags) >(_link_flags)',
                   '--source-list=^(source_list_glibc32)',
                 ],
               },
             ],
           }],
           # x86-64 glibc shared library action
           ['nso_target!="" and build_glibc!=0 and enable_x86_64!=0 and disable_glibc==0', {
             'variables': {
                'tool_name': 'glibc',
                'objdir_glibc64%': '>(INTERMEDIATE_DIR)/<(tool_name)-x86-64-so/>(_target_name)',
                'out_glibc64%': '<(SHARED_INTERMEDIATE_DIR)/tc_<(tool_name)/lib64/>(nso_target)',
             },
             'actions': [
               {
                 'action_name': 'build glibc x86-64 nso',
                 'variables': {
                   'source_list_glibc64%': '^|(<(tool_name)-x86-64-so.>(_target_name).source_list.gypcmd ^(_sources) ^(sources))',
                 },
                 'msvs_cygwin_shell': 0,
                 'description': 'building >(out_glibc64)',
                 'inputs': [
                    '<(DEPTH)/native_client/build/build_nexe.py',
                    '>!@pymod_do_main(>(get_sources) >(sources) >(_sources))',
                    '>@(extra_deps_glibc64)',
                    '^(source_list_glibc64)',
                    '<(SHARED_INTERMEDIATE_DIR)/sdk/toolchain/<(OS)_x86_glibc/stamp.prep',
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
                   '--include-dirs=>(tc_include_dir_glibc) ^(include_dirs) >(_include_dirs)',
                   '--compile_flags=-m64 -fPIC ^(gcc_compile_flags) >(_gcc_compile_flags) ^(compile_flags) >(_compile_flags)',
                   '--defines=^(defines) >(_defines)',
                   '--link_flags=-B>(tc_lib_dir_glibc64) ^(link_flags) >(_link_flags)',
                   '--source-list=^(source_list_glibc64)',
                 ],
               },
             ],
           }],
           # x86-32 glibc shared library action
           ['nso_target!="" and build_glibc!=0 and enable_x86_32!=0 and disable_glibc==0', {
             'variables': {
                'tool_name': 'glibc',
                'out_glibc32%': '<(SHARED_INTERMEDIATE_DIR)/tc_<(tool_name)/lib32/>(nso_target)',
                'objdir_glibc32%': '>(INTERMEDIATE_DIR)/<(tool_name)-x86-32-so/>(_target_name)',
             },
             'actions': [
               {
                 'action_name': 'build glibc x86-32 nso',
                 'variables': {
                   'source_list_glibc32%': '^|(<(tool_name)-x86-32-so.>(_target_name).source_list.gypcmd ^(_sources) ^(sources))',
                 },
                 'msvs_cygwin_shell': 0,
                 'description': 'building >(out_glibc32)',
                 'inputs': [
                    '<(DEPTH)/native_client/build/build_nexe.py',
                    '>!@pymod_do_main(>(get_sources) >(sources) >(_sources))',
                    '>@(extra_deps_glibc32)',
                    '^(source_list_glibc32)',
                    '<(SHARED_INTERMEDIATE_DIR)/sdk/toolchain/<(OS)_x86_glibc/stamp.prep',
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
                   '--include-dirs=>(tc_include_dir_glibc) ^(include_dirs) >(_include_dirs)',
                   '--compile_flags=-m32 -fPIC ^(gcc_compile_flags) >(_gcc_compile_flags) ^(compile_flags) >(_compile_flags)',
                   '--defines=^(defines) >(_defines)',
                   '--link_flags=-m32 -B>(tc_lib_dir_glibc32) ^(link_flags) >(_link_flags)',
                   '--source-list=^(source_list_glibc32)',
                 ],
               },
             ],
           }],
        ], # end target_conditions for glibc (nexe/nlib/nso, x86-32/64)
      },
    }], # end target_arch == ia32 or x64
  ],
  # Common defaults for pnacl targets
  'target_defaults': {
    'gcc_compile_flags': [],
    'pnacl_compile_flags': [],
    'variables': {
      'disable_pnacl%': 0,
      'build_pnacl_newlib': 0,
      'nlib_target': '',
      'extra_deps_pnacl_newlib': [],
      'tc_lib_dir_pnacl_newlib': '<(SHARED_INTERMEDIATE_DIR)/tc_pnacl_newlib/lib',
      'tc_lib_dir_pnacl_translate' :'<(SHARED_INTERMEDIATE_DIR)/tc_pnacl_translate',
      'tc_include_dir_pnacl_newlib': '<(SHARED_INTERMEDIATE_DIR)/tc_pnacl_newlib/include',
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
      'pnacl_compile_flags': [
        '-Wno-extra-semi',
        '-Wno-unused-private-field',
        '-Wno-char-subscripts',
        '-Wno-unused-function',
      ],
    },
    'target_conditions': [
      # pnacl actions for building pexes and translating them
      ['nexe_target!="" and disable_pnacl==0 and build_pnacl_newlib!=0', {
        'variables': {
            'out_pnacl_newlib_x86_32_nexe%': '<(PRODUCT_DIR)/>(nexe_target)_pnacl_newlib_x32.nexe',
            'out_pnacl_newlib_x86_64_nexe%': '<(PRODUCT_DIR)/>(nexe_target)_pnacl_newlib_x64.nexe',
            'out_pnacl_newlib_arm_nexe%': '<(PRODUCT_DIR)/>(nexe_target)_pnacl_newlib_arm.nexe',
            'out_pnacl_newlib_mips_nexe%': '<(PRODUCT_DIR)/>(nexe_target)_pnacl_newlib_mips.nexe',
          'tool_name': 'pnacl_newlib',
          'inst_dir': '<(SHARED_INTERMEDIATE_DIR)/tc_pnacl_newlib',
          'out_pnacl_newlib%': '<(PRODUCT_DIR)/>(nexe_target)_newlib.pexe',
          'objdir_pnacl_newlib%': '>(INTERMEDIATE_DIR)/<(tool_name)/>(_target_name)',
          'link_flags': [
            '-O3',
          ],
          'translate_flags': [],
        },
        'actions': [
          {
            'action_name': 'build newlib pexe',
            'variables': {
              'source_list_pnacl_newlib%': '^|(<(tool_name).>(_target_name).source_list.gypcmd ^(_sources) ^(sources))',
            },
            'msvs_cygwin_shell': 0,
            'description': 'building >(out_pnacl_newlib)',
            'inputs': [
              '<(DEPTH)/native_client/build/build_nexe.py',
              '>!@pymod_do_main(>(get_sources) >(sources) >(_sources))',
              '>@(extra_deps_pnacl_newlib)',
              '^(source_list_pnacl_newlib)',
              '<(SHARED_INTERMEDIATE_DIR)/sdk/toolchain/<(OS)_pnacl/stamp.prep',
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
              '--include-dirs=>(tc_include_dir_pnacl_newlib) ^(include_dirs) >(_include_dirs)',
              '--compile_flags=^(compile_flags) >(_compile_flags) ^(pnacl_compile_flags) >(_pnacl_compile_flags)',
              '--defines=^(defines) >(_defines)',
              '--link_flags=-B<(SHARED_INTERMEDIATE_DIR)/tc_pnacl_newlib/lib ^(link_flags) >(_link_flags)',
              '--source-list=^(source_list_pnacl_newlib)',
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
                  '--link_flags=^(translate_flags) >(translate_flags) -Wl,-L>(tc_lib_dir_pnacl_translate)/lib-x86-32',
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
                  '--link_flags=^(translate_flags) >(translate_flags) -Wl,-L>(tc_lib_dir_pnacl_translate)/lib-x86-64',
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
                  '--link_flags=^(translate_flags) >(translate_flags) -Wl,-L>(tc_lib_dir_pnacl_translate)/lib-arm',
                  '>(out_pnacl_newlib)',
                ],
              }],
            }],
          ],
      }],
      # pnacl action for building libraries
      ['nlib_target!="" and disable_pnacl==0 and build_pnacl_newlib!=0', {
        'variables': {
          'tool_name': 'pnacl_newlib',
          'objdir_pnacl_newlib%': '>(INTERMEDIATE_DIR)/<(tool_name)-pnacl/>(_target_name)',
          'out_pnacl_newlib%': '<(SHARED_INTERMEDIATE_DIR)/tc_<(tool_name)/lib/>(nlib_target)',
        },
        'actions': [
          {
            'action_name': 'build newlib plib',
            'variables': {
              'source_list_pnacl_newlib%': '^|(<(tool_name).>(_target_name).source_list.gypcmd ^(_sources) ^(sources))',
            },
            'msvs_cygwin_shell': 0,
            'description': 'building >(out_pnacl_newlib)',
            'inputs': [
              '<(DEPTH)/native_client/build/build_nexe.py',
              '>!@pymod_do_main(>(get_sources) >(sources) >(_sources))',
              '>@(extra_deps_pnacl_newlib)',
              '^(source_list_pnacl_newlib)',
              '<(SHARED_INTERMEDIATE_DIR)/sdk/toolchain/<(OS)_pnacl/stamp.prep',
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
              '--include-dirs=>(tc_include_dir_pnacl_newlib) ^(include_dirs) >(_include_dirs)',
              '--compile_flags=^(compile_flags) >(_compile_flags) ^(pnacl_compile_flags) >(_pnacl_compile_flags)',
              '--defines=^(defines) >(_defines)',
              '--link_flags=-B>(tc_lib_dir_pnacl_newlib) ^(link_flags) >(_link_flags)',
              '--source-list=^(source_list_pnacl_newlib)',
            ],
          },
        ],
      }],
    ], # end target_conditions for pnacl pexe/plib
  },
}
