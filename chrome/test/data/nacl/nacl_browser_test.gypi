# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../../../../native_client/build/untrusted.gypi',
  ],
  'target_defaults': {
    # We need to override the variables in untrusted.gypi outside of a
    # target_condition block because the target_condition block in
    # untrusted gypi is fully evaluated and interpolated before any of the
    # target_condition blocks in this file are expanded.  This means that any
    # variables overriden inside a target_condition block in this file will not
    # affect the values in untrusted.gypi.
    'variables': {
      'test_files': [],
      'nacl_newlib_out_dir': '<(PRODUCT_DIR)/nacl_test_data/newlib',
      'nacl_glibc_out_dir': '<(PRODUCT_DIR)/nacl_test_data/glibc',
      'nacl_pnacl_newlib_out_dir': '<(PRODUCT_DIR)/nacl_test_data/pnacl',
      'target_conditions': [
        ['nexe_target!=""', {
          # These variables are used for nexe building and for library building,
          # so they should be unconditionally re-defined.
          'out_newlib32': '>(nacl_newlib_out_dir)/>(nexe_target)_newlib_x86_32.nexe',
          'out_newlib64': '>(nacl_newlib_out_dir)/>(nexe_target)_newlib_x86_64.nexe',
          'out_newlib_arm': '>(nacl_newlib_out_dir)/>(nexe_target)_newlib_arm.nexe',
          'nmf_newlib': '>(nacl_newlib_out_dir)/>(nexe_target).nmf',
          'out_glibc32': '>(nacl_glibc_out_dir)/>(nexe_target)_glibc_x86_32.nexe',
          'out_glibc64': '>(nacl_glibc_out_dir)/>(nexe_target)_glibc_x86_64.nexe',
          'out_glibc_arm': '>(nacl_glibc_out_dir)/>(nexe_target)_glibc_arm.nexe',
          'nmf_glibc': '>(nacl_glibc_out_dir)/>(nexe_target).nmf',
          'out_pnacl_newlib': '>(nacl_pnacl_newlib_out_dir)/>(nexe_target)_newlib_pnacl.pexe',
          'nmf_pnacl_newlib': '>(nacl_pnacl_newlib_out_dir)/>(nexe_target).nmf',
        }],
      ],
    },
    'dependencies': [
       '<(DEPTH)/native_client/src/untrusted/nacl/nacl.gyp:nacl_lib',
       '<(DEPTH)/ppapi/ppapi_untrusted.gyp:ppapi_cpp_lib',
       '<(DEPTH)/ppapi/native_client/native_client.gyp:ppapi_lib',
    ],
    'target_conditions': [
      ['test_files!=[] and build_newlib==1', {
        'copies': [
          {
            'destination': '>(nacl_newlib_out_dir)',
            'files': [
              '>@(test_files)',
            ],
          },
        ],
      }],
      ['test_files!=[] and "<(target_arch)"!="arm" and disable_glibc==0 and build_glibc==1', {
        'copies': [
          {
            'destination': '>(nacl_glibc_out_dir)',
            'files': [
              '>@(test_files)',
            ],
          },
        ],
      }],
      ['test_files!=[] and build_pnacl_newlib==1 and disable_pnacl==0', {
        'copies': [
          {
            'destination': '>(nacl_pnacl_newlib_out_dir)',
            'files': [
              '>@(test_files)',
            ],
          },
        ],
      }],
      ['nexe_target!=""', {
        'variables': {
          # Patch over the fact that untrusted.gypi doesn't define these in all
          # cases.
          'enable_x86_64%': 0,
          'enable_x86_32%': 0,
          'enable_arm%': 0,
          'include_dirs': [
            '<(DEPTH)',
          ],
          'link_flags': [
            '-lppapi_cpp',
            '-lppapi',
            '-pthread',
          ],
        },
        'target_conditions': [
          ['build_newlib==1', {
            'actions': [
              {
                'action_name': 'Generate NEWLIB NMF',
                # Unlike glibc, nexes are not actually inputs - only the names matter.
                # We don't have the nexes as inputs because the ARM nexe may not
                # exist.  However, VS 2010 seems to blackhole this entire target if
                # there are no inputs to this action.  To work around this we add a
                # bogus input.
                'inputs': [],
                'outputs': ['>(nmf_newlib)'],
                'action': [
                  'python',
                  '<(DEPTH)/native_client_sdk/src/tools/create_nmf.py',
                  '>@(_inputs)',
                  '--output=>(nmf_newlib)',
                ],
                'target_conditions': [
                  ['enable_x86_64==1', {
                    'inputs': ['>(out_newlib64)'],
                  }],
                  ['enable_x86_32==1', {
                    'inputs': ['>(out_newlib32)'],
                  }],
                  ['enable_arm==1', {
                    'inputs': ['>(out_newlib_arm)'],
                  }],
                ],
              },
            ],
          }],
          ['"<(target_arch)"!="arm" and disable_glibc==0 and build_glibc==1', {
            'variables': {
              # NOTE: Use /lib, not /lib64 here; it is a symbolic link which
              # doesn't work on Windows.
              'libdir_glibc64': '>(nacl_glibc_tc_root)/x86_64-nacl/lib',
              'libdir_glibc32': '>(nacl_glibc_tc_root)/x86_64-nacl/lib32',
              'nacl_objdump': '>(nacl_glibc_tc_root)/bin/x86_64-nacl-objdump',
            },
            'actions': [
              {
                'action_name': 'Generate GLIBC NMF and copy libs',
                'inputs': [],
                # NOTE: There is no explicit dependency for the lib32
                # and lib64 directories created in the PRODUCT_DIR.
                # They are created as a side-effect of NMF creation.
                'outputs': ['>(nmf_glibc)'],
                'action': [
                  'python',
                  '<(DEPTH)/native_client_sdk/src/tools/create_nmf.py',
                  '>@(_inputs)',
                  '--objdump=>(nacl_objdump)',
                  '--output=>(nmf_glibc)',
                  '--path-prefix=>(nexe_target)_libs',
                  '--stage-dependencies=<(nacl_glibc_out_dir)',
                ],
                'target_conditions': [
                  ['enable_x86_64==1', {
                    'inputs': ['>(out_glibc64)'],
                    'action': [
                      '--library-path=>(libdir_glibc64)',
                      '--library-path=<(SHARED_INTERMEDIATE_DIR)/tc_glibc/lib64',
                    ],
                  }],
                  ['enable_x86_32==1', {
                    'inputs': ['>(out_glibc32)'],
                    'action': [
                      '--library-path=>(libdir_glibc32)',
                      '--library-path=<(SHARED_INTERMEDIATE_DIR)/tc_glibc/lib32',
                    ],
                  }],
                  # TODO(ncbray) handle arm case.  We don't have ARM glibc yet.
                ],
              },
            ],
          }],
          ['build_pnacl_newlib==1 and disable_pnacl==0', {
            'actions': [
              {
                'action_name': 'Generate PNACL NEWLIB NMF',
                'inputs': ['>(out_pnacl_newlib)'],
                'outputs': ['>(nmf_pnacl_newlib)'],
                'action': [
                  'python',
                  '<(DEPTH)/native_client_sdk/src/tools/create_nmf.py',
                  '>@(_inputs)',
                  '--output=>(nmf_pnacl_newlib)',
                ],
              },
            ],
          }],
        ],
      }],
    ],
  },
}
