# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    # See native_client/Sconstruct for more details.
    # Expected address for beginning of data in for the IRT.
    'NACL_IRT_DATA_START': '0x3ef00000',
    # Expected address for beginning of code in for the IRT.
    'NACL_IRT_TEXT_START': '0x0fc00000',
    # Default C compiler defines.
    'default_defines': [
      '-D__linux__',
      '-D__STDC_LIMIT_MACROS=1',
      '-D__STDC_FORMAT_MACROS=1',
      '-D_GNU_SOURCE=1',
      '-D_BSD_SOURCE=1',
      '-D_POSIX_C_SOURCE=199506',
      '-D_XOPEN_SOURCE=600',
      '-DDYNAMIC_ANNOTATIONS_ENABLED=1',
      '-DDYNAMIC_ANNOTATIONS_PREFIX=NACL_',
    ],
    'default_compile_flags': [
      #'-std=gnu99',  Added by build_nexe
      '-O3',
      '-g',
    ],
    # TODO(bbudge) Remove after the proxy switch.
    'build_ppapi_ipc_proxy_untrusted%': 0,
  },
  'conditions': [
    ['build_ppapi_ipc_proxy_untrusted==1', {
      'variables': {
        'default_defines': [
          '-DNACL_PPAPI_IPC_PROXY',
        ],
      },
    }],
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
        'link_flags': [],
        'include_dirs': [],
        'variables': {
          'nexe_target': '',
          'nlib_target': '',
          'build_newlib': 0,
          'build_glibc': 0,
          'disable_glibc%': 0,
          'extra_args': [],
          'enable_x86_32': 1,
          'enable_x86_64': 1,
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
            '<@(default_defines)',
            '-DNACL_BUILD_ARCH=x86',
          ],
          'compile_flags': [
            '<@(default_compile_flags)',
            '-fomit-frame-pointer',
            '-mtls-use-call'
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
        'link_flags': [],
        'include_dirs': [],
        'variables': {
          'python_exe': 'python',
          'nexe_target': '',
          'nlib_target': '',
          'build_newlib': 0,
          'build_glibc': 0,
          'disable_glibc%': 1,
          'extra_args': [],
          'enable_arm': 1,
          'extra_deps_newlib_arm': [],
          'lib_dirs_newlib_arm': [],
          'include_dirs': ['<(DEPTH)','<(DEPTH)/ppapi'],
          'defines': [
            '<@(default_defines)',
            '-DNACL_BUILD_ARCH=arm',
           ],
           'compile_flags': [
             '<@(default_compile_flags)',
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
                    '<(DEPTH)/ppapi/ppapi_cpp.gypi',
                    '>!@pymod_do_main(>(get_sources) >(sources) >(_sources))',
                    '>@(extra_deps_newlib64)',
                 ],
                 'outputs': ['>(out_newlib64)'],
                 'action': [
                   '>(python_exe)',
                   '<(DEPTH)/native_client/build/build_nexe.py',
                   '>@(extra_args)',
                   '--arch', 'x86-64',
                   '--build', 'newlib_nexe',
                   '--root', '<(DEPTH)',
                   '--name', '>(out_newlib64)',
                   '--objdir', '>(objdir_newlib64)',
                   '--include-dirs', '<(inst_dir)/include >(include_dirs) >(include_dirs) >(_include_dirs)',
                   '--lib-dirs', '>(lib_dirs_newlib64) ',
                   '--compile_flags', '-m64 >(compile_flags) ',
                   '>@(defines)',
                   '>@(_defines)',
                   '-DNACL_BUILD_SUBARCH=64',
                   '--link_flags', '-B<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib64 >(link_flags) >(_link_flags)',
                   '--source-list',
                   '^|(<(source_list_newlib64) ^(_sources) ^(sources))',
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
                    '<(DEPTH)/ppapi/ppapi_cpp.gypi',
                    '>!@pymod_do_main(>(get_sources) >(sources) >(_sources))',
                    '>@(extra_deps_newlib64)',
                 ],
                 'outputs': ['>(out_newlib64)'],
                 'action': [
                   '>(python_exe)',
                   '<(DEPTH)/native_client/build/build_nexe.py',
                   '>@(extra_args)',
                   '--arch', 'x86-64',
                   '--build', 'newlib_nlib',
                   '--root', '<(DEPTH)',
                   '--name', '>(out_newlib64)',
                   '--objdir', '>(objdir_newlib64)',
                   '--include-dirs', '<(inst_dir)/include >(include_dirs) >(include_dirs) >(_include_dirs)',
                   '--lib-dirs', '>(lib_dirs_newlib64) ',
                   '--compile_flags', '-m64 >(compile_flags)',
                   '>@(defines)',
                   '>@(_defines)',
                   '-DNACL_BUILD_SUBARCH=64',
                   '--link_flags', '-B<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib64 >(link_flags) >(_link_flags)',
                   '--source-list',
                   '^|(<(source_list_newlib64) ^(_sources) ^(sources))',
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
                    '<(DEPTH)/ppapi/ppapi_cpp.gypi',
                    '>!@pymod_do_main(>(get_sources) >(sources) >(_sources))',
                    '>@(extra_deps_newlib32)',
                 ],
                 'outputs': ['>(out_newlib32)'],
                 'action': [
                   '>(python_exe)',
                   '<(DEPTH)/native_client/build/build_nexe.py',
                   '>@(extra_args)',
                   '--arch', 'x86-32',
                   '--build', 'newlib_nexe',
                   '--root', '<(DEPTH)',
                   '--name', '>(out_newlib32)',
                   '--objdir', '>(objdir_newlib32)',
                   '--include-dirs', '<(inst_dir)/include >(include_dirs) >(include_dirs) >(_include_dirs)',
                   '--lib-dirs', '>(lib_dirs_newlib32)',
                   '--compile_flags', '-m32 >(compile_flags)',
                   '>@(defines)',
                   '>@(_defines)',
                   '-DNACL_BUILD_SUBARCH=32',
                   '--link_flags', '-m32 -B<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib32 >(link_flags) >(_link_flags)',
                   '--source-list',
                   '^|(<(source_list_newlib32) ^(_sources) ^(sources))',
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
                    '<(DEPTH)/ppapi/ppapi_cpp.gypi',
                    '>!@pymod_do_main(>(get_sources) >(sources) >(_sources))',
                    '>@(extra_deps_newlib32)',
                 ],
                 'outputs': ['>(out_newlib32)'],
                 'action': [
                   '>(python_exe)',
                   '<(DEPTH)/native_client/build/build_nexe.py',
                   '>@(extra_args)',
                   '--arch', 'x86-32',
                   '--build', 'newlib_nlib',
                   '--root', '<(DEPTH)',
                   '--name', '>(out_newlib32)',
                   '--objdir', '>(objdir_newlib32)',
                   '--include-dirs', '<(inst_dir)/include >(include_dirs) >(include_dirs) >(_include_dirs)',
                   '--lib-dirs', '>(lib_dirs_newlib32) ',
                   '--compile_flags', '-m32 >(compile_flags)',
                   '>@(defines)',
                   '>@(_defines)',
                   '-DNACL_BUILD_SUBARCH=32',
                   '--link_flags', '-m32 -B<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib32 >(link_flags) >(_link_flags)',
                   '--source-list',
                   '^|(<(source_list_newlib32) ^(_sources) ^(sources))',
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
          ['nexe_target!="" and build_newlib!=0', {
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
                  '<(DEPTH)/ppapi/ppapi_cpp.gypi',
                  '>!@pymod_do_main(>(get_sources) >(sources) >(_sources))',
                  '>@(extra_deps_newlib_arm)',
                ],
                'outputs': ['>(out_newlib_arm)'],
                'action': [
                  '>(python_exe)',
                  '<(DEPTH)/native_client/build/build_nexe.py',
                  '>@(extra_args)',
                  '--arch', 'arm',
                  '--build', 'newlib_nexe',
                  '--root', '<(DEPTH)',
                  '--name', '>(out_newlib_arm)',
                  '--objdir', '>(objdir_newlib_arm)',
                  '--include-dirs', '<(inst_dir)/include >(include_dirs) >(include_dirs) >(_include_dirs)',
                  '--lib-dirs', '>(lib_dirs_newlib_arm) ',
                  '--compile_flags', '>(compile_flags)',
                  '>@(defines)',
                  '>@(_defines)',
                  '-DNACL_BUILD_SUBARCH=32',
                  '--link_flags', '-B<(SHARED_INTERMEDIATE_DIR)/tc_newlib/libarm >(link_flags) >(_link_flags)',
                  '--source-list',
                  '^|(<(source_list_newlib_arm) ^(_sources) ^(sources))',
                 ],
              },
            ],
          }],
          ['nlib_target!="" and build_newlib!=0', {
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
                  '<(DEPTH)/ppapi/ppapi_cpp.gypi',
                  '>!@pymod_do_main(>(get_sources) >(sources) >(_sources))',
                  '>@(extra_deps_newlib_arm)',
                ],
                'outputs': ['>(out_newlib_arm)'],
                'action': [
                  '>(python_exe)',
                  '<(DEPTH)/native_client/build/build_nexe.py',
                  '>@(extra_args)',
                  '--arch', 'arm',
                  '--build', 'newlib_nlib',
                  '--root', '<(DEPTH)',
                  '--name', '>(out_newlib_arm)',
                  '--objdir', '>(objdir_newlib_arm)',
                  '--include-dirs', '<(inst_dir)/include >(include_dirs) >(include_dirs) >(_include_dirs)',
                  '--lib-dirs', '>(lib_dirs_newlib_arm) ',
                  '--compile_flags', '>(compile_flags)',
                  '>@(defines)',
                  '>@(_defines)',
                  '-DNACL_BUILD_SUBARCH=32',
                  '--link_flags', '-B<(SHARED_INTERMEDIATE_DIR)/tc_newlib/libarm >(link_flags) >(_link_flags)',
                  '--source-list',
                  '^|(<(source_list_newlib_arm) ^(_sources) ^(sources))',
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
                    '<(DEPTH)/ppapi/ppapi_cpp.gypi',
                    '>!@pymod_do_main(>(get_sources) >(sources) >(_sources))',
                    '>@(extra_deps_glibc64)',
                 ],
                 'outputs': ['>(out_glibc64)'],
                 'action': [
                   '>(python_exe)',
                   '<(DEPTH)/native_client/build/build_nexe.py',
                   '>@(extra_args)',
                   '--arch', 'x86-64',
                   '--build', 'glibc_nexe',
                   '--root', '<(DEPTH)',
                   '--name', '>(out_glibc64)',
                   '--objdir', '>(objdir_glibc64)',
                   '--include-dirs', '<(inst_dir)/include >(include_dirs) >(include_dirs) >(_include_dirs)',
                   '--lib-dirs', '>(lib_dirs_glibc64) ',
                   '--compile_flags', '-m64 >(compile_flags) ',
                   '>@(defines)',
                   '>@(_defines)',
                   '-DNACL_BUILD_SUBARCH=64',
                   '--link_flags', '-B<(SHARED_INTERMEDIATE_DIR)/tc_glibc/lib64 >(link_flags) >(_link_flags)',
                   '--source-list',
                   '^|(<(source_list_glibc64) ^(_sources) ^(sources))',
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
                    '<(DEPTH)/ppapi/ppapi_cpp.gypi',
                    '>!@pymod_do_main(>(get_sources) >(sources) >(_sources))',
                    '>@(extra_deps_glibc32)',
                 ],
                 'outputs': ['>(out_glibc32)'],
                 'action': [
                   '>(python_exe)',
                   '<(DEPTH)/native_client/build/build_nexe.py',
                   '>@(extra_args)',
                   '--arch', 'x86-32',
                   '--build', 'glibc_nexe',
                   '--root', '<(DEPTH)',
                   '--name', '>(out_glibc32)',
                   '--objdir', '>(objdir_glibc32)',
                   '--include-dirs', '<(inst_dir)/include >(include_dirs) >(include_dirs) >(_include_dirs)',
                   '--lib-dirs', '>(lib_dirs_glibc32) ',
                   '--compile_flags', '-m32 >(compile_flags)',
                   '>@(defines)',
                   '>@(_defines)',
                   '-DNACL_BUILD_SUBARCH=32',
                   '--link_flags', '-m32 -B<(SHARED_INTERMEDIATE_DIR)/tc_glibc/lib32 >(link_flags) >(_link_flags)',
                   '--source-list',
                   '^|(<(source_list_glibc32) ^(_sources) ^(sources))',
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
                    '<(DEPTH)/ppapi/ppapi_cpp.gypi',
                    '>!@pymod_do_main(>(get_sources) >(sources) >(_sources))',
                    '>@(extra_deps_glibc64)',
                 ],
                 'outputs': ['>(out_glibc64)'],
                 'action': [
                   '>(python_exe)',
                   '<(DEPTH)/native_client/build/build_nexe.py',
                   '>@(extra_args)',
                   '--arch', 'x86-64',
                   '--build', 'glibc_nlib',
                   '--root', '<(DEPTH)',
                   '--name', '>(out_glibc64)',
                   '--objdir', '>(objdir_glibc64)',
                   '--include-dirs', '<(inst_dir)/include >(include_dirs) >(include_dirs) >(_include_dirs)',
                   '--lib-dirs', '>(lib_dirs_glibc64) ',
                   '--compile_flags', ' -m64 >(compile_flags)',
                   '>@(defines)',
                   '>@(_defines)',
                   '-DNACL_BUILD_SUBARCH=64',
                   '--link_flags', '-B<(SHARED_INTERMEDIATE_DIR)/tc_glibc/lib64 >(link_flags) >(_link_flags)',
                   '--source-list',
                   '^|(<(source_list_glibc64) ^(_sources) ^(sources))',
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
                    '<(DEPTH)/ppapi/ppapi_cpp.gypi',
                    '>!@pymod_do_main(>(get_sources) >(sources) >(_sources))',
                    '>@(extra_deps_glibc32)',
                 ],
                 'outputs': ['>(out_glibc32)'],
                 'action': [
                   '>(python_exe)',
                   '<(DEPTH)/native_client/build/build_nexe.py',
                   '>@(extra_args)',
                   '--arch', 'x86-32',
                   '--build', 'glibc_nlib',
                   '--root', '<(DEPTH)',
                   '--name', '>(out_glibc32)',
                   '--objdir', '>(objdir_glibc32)',
                   '--include-dirs', '<(inst_dir)/include >(include_dirs) >(include_dirs) >(_include_dirs)',
                   '--lib-dirs', '>(lib_dirs_glibc32) ',
                   '--compile_flags', '-m32 >(compile_flags)',
                   '>@(defines)',
                   '>@(_defines)',
                   '-DNACL_BUILD_SUBARCH=32',
                   '--link_flags', '-m32 -B<(SHARED_INTERMEDIATE_DIR)/tc_glibc/lib32 >(link_flags) >(_link_flags)',
                   '--source-list',
                   '^|(<(source_list_glibc32) ^(_sources) ^(sources))',
                 ],
               },
             ],
           }],
        ],
      },
    }],
  ],
}
