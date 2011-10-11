# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'conditions': [
    ['disable_untrusted==0 and OS!="mac" and target_arch!="arm"', {
      'target_defaults': {
        'conditions': [
          ['OS=="win"', {
            'variables': {
              'python_exe': 'call <(DEPTH)/native_client/tools/win_py.cmd',
            },
          }, {
            'variables': {
              'python_exe': 'python',
            },
          }],
        ],
        'variables': {
          'nexe_target': '',
          'nlib_target': '',
          'build_newlib': 0,
          'build_glibc': 0,
          'extra_args': [],
          'lib_dirs': [],
          'include_dirs': ['<(DEPTH)','<(DEPTH)/ppapi'],
          'defines': [
            '-DNACL_BLOCK_SHIFT=5',
            '-DNACL_BLOCK_SIZE=32',
            '-DNACL_BUILD_ARCH=x86',
            '-D__linux__',
            '-D__STDC_LIMIT_MACROS=1',
            '-D__STDC_FORMAT_MACROS=1',
            '-D_GNU_SOURCE=1',
            '-D_BSD_SOURCE=1',
            '-D_POSIX_C_SOURCE=199506',
            '-D_XOPEN_SOURCE=600',
           ],
          'compile_flags': [
            #'-std=gnu99',  Added by build_nexe
            '-O2',
            '-fomit-frame-pointer',
            '-mtls-use-call'
           ],
          'link_flags': [],
          'get_sources': [
              '>(python_exe)', '<(DEPTH)/native_client/build/scan_sources.py',
          ],
        },
      },
    }],
    ['disable_untrusted==0 and OS!="mac" and target_arch=="x64"', {
      'target_defaults': {
        'target_conditions': [
           ['nexe_target!="" and build_newlib!=0', {
             'variables': {
                'tool_name': 'newlib',
                'inst_dir': '<(SHARED_INTERMEDIATE_DIR)/tc_newlib',
                'outname': '<(PRODUCT_DIR)/>(nexe_target)_newlib_x64.nexe',
                'objdir%': '>(INTERMEDIATE_DIR)/>(tool_name)',
             },
             'actions': [
               {
                 'action_name': 'build newlib x86-64 nexe',
                 'msvs_cygwin_shell': 0,
                 'description': 'building >(outname)',
                 'inputs': [
                    '>!@(>(get_sources) -I. -I.. >(sources))',
                 ],
                 'outputs': ['>(outname)'],
                 'action': [
                   '>(python_exe)',
                   '<(DEPTH)/native_client/build/build_nexe.py',
                   '>@(extra_args)',
                   '--arch', 'x86-64',
                   '--build', 'newlib_nexe',
                   '--root', '<(DEPTH)',
                   '--name', '>(outname)',
                   '--objdir', '>(objdir)',
                   '--include-dirs', '>(inst_dir)/include >(include_dirs) >(include_dirs)',
                   '--lib-dirs', '>(lib_dirs) ',
                   '--compile_flags', '-m64 >@(compile_flags) ',
                   '>@(defines)', '-DNACL_BUILD_SUBARCH=64',
                   '--link_flags', '-B<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib64 >(link_flags) ',
                   '>@(sources)',
                 ],
               },
             ],
           }],
        ],
      },
    }],
    ['disable_untrusted==0 and OS!="mac" and target_arch=="x64"', {
      'target_defaults': {
        'target_conditions': [
           ['nlib_target!="" and build_newlib!=0', {
             'variables': {
                'tool_name': 'newlib',
                'inst_dir': '<(SHARED_INTERMEDIATE_DIR)/tc_newlib',
                'outname': '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib64/>(nlib_target)',
                'objdir%': '>(INTERMEDIATE_DIR)/>(tool_name)',
             },
             'actions': [
               {
                 'action_name': 'build newlib x86-64 nlib',
                 'msvs_cygwin_shell': 0,
                 'description': 'building >(outname)',
                 'inputs': [
                    '>!@(>(get_sources) -I. -I.. >(sources))',
                 ],
                 'outputs': ['>(outname)'],
                 'action': [
                   '>(python_exe)',
                   '<(DEPTH)/native_client/build/build_nexe.py',
                   '>@(extra_args)',
                   '--arch', 'x86-64',
                   '--build', 'newlib_nlib',
                   '--root', '<(DEPTH)',
                   '--name', '>(outname)',
                   '--objdir', '>(objdir)',
                   '--include-dirs', '>(inst_dir)/include >(include_dirs) >(include_dirs)',
                   '--lib-dirs', '>(lib_dirs) ',
                   '--compile_flags', ' -m64 >@(compile_flags)',
                   '>@(defines)', '-DNACL_BUILD_SUBARCH=64',
                   '--link_flags', '-B<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib64 >(link_flags)',
                   '>@(sources)',
                 ],
               },
             ],
           }],
        ],
      },
    }],
    ['disable_untrusted==0 and OS!="mac" and target_arch=="ia32"', {
      'target_defaults': {
        'target_conditions': [
           ['nexe_target!="" and build_newlib!=0', {
             'variables': {
                'tool_name': 'newlib',
                'inst_dir': '<(SHARED_INTERMEDIATE_DIR)/tc_newlib',
                'outname': '<(PRODUCT_DIR)/>(nexe_target)_newlib_x32.nexe',
                'objdir%': '>(INTERMEDIATE_DIR)/>(tool_name)',
             },
             'target_conditions': [
                ['objdir=="undefined"', {
                  'variables': {
                    'objdir':  '>(INTERMEDIATE_DIR)/>(tool_name)',
                  },
                }],
             ],
             'actions': [
               {
                 'action_name': 'build newlib x86-32 nexe',
                 'msvs_cygwin_shell': 0,
                 'description': 'building >(outname)',
                 'inputs': [
                    '>!@(>(get_sources) -I. -I.. >(sources))',
                 ],
                 'outputs': ['>(outname)'],
                 'action': [
                   '>(python_exe)',
                   '<(DEPTH)/native_client/build/build_nexe.py',
                   '>@(extra_args)',
                   '--arch', 'x86-32',
                   '--build', 'newlib_nexe',
                   '--root', '<(DEPTH)',
                   '--name', '>(outname)',
                   '--objdir', '>(objdir)',
                   '--include-dirs', '>(inst_dir)/include >(include_dirs) >(include_dirs)',
                   '--lib-dirs', '>(lib_dirs) ',
                   '--compile_flags', '-m32 >@(compile_flags)',
                   '>@(defines)', '-DNACL_BUILD_SUBARCH=32',
                   '--link_flags', '-m32 -B<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib32 >(link_flags)',
                   '>@(sources)',
                 ],
               },
             ],
           }],
        ],
      },
    }],
    ['disable_untrusted==0 and OS!="mac" and target_arch=="ia32"', {
      'target_defaults': {
        'target_conditions': [
           ['nlib_target!="" and build_newlib!=0', {
             'variables': {
                'tool_name': 'newlib',
                'inst_dir': '<(SHARED_INTERMEDIATE_DIR)/tc_newlib',
                'outname': '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib32/>(nlib_target)',
                'objdir%': '>(INTERMEDIATE_DIR)/>(tool_name)',
             },
             'condition': [
                ['objdir=="undefined"', {
                  'variables': {
                    'objdir':  '>(INTERMEDIATE_DIR)/>(tool_name)',
                  },
                }],
             ],
             'actions': [
               {
                 'action_name': 'build newlib x86-32 nlib',
                 'msvs_cygwin_shell': 0,
                 'description': 'building >(outname)',
                 'inputs': [
                    '>!@(>(get_sources) -I. -I.. >(sources))',
                 ],
                 'outputs': ['>(outname)'],
                 'action': [
                   '>(python_exe)',
                   '<(DEPTH)/native_client/build/build_nexe.py',
                   '>@(extra_args)',
                   '--arch', 'x86-32',
                   '--build', 'newlib_nlib',
                   '--root', '<(DEPTH)',
                   '--name', '>(outname)',
                   '--objdir', '>(objdir)',
                   '--include-dirs', '>(inst_dir)/include >(include_dirs) >(include_dirs)',
                   '--lib-dirs', '>(lib_dirs) ',
                   '--compile_flags', '-m32 >@(compile_flags)',
                   '>@(defines)', '-DNACL_BUILD_SUBARCH=32',
                   '--link_flags', '-m32 -B<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib32 >(link_flags)',
                   '>@(sources)',
                 ],
               },
             ],
           }],
        ],
      },
    }],
  ],
}
