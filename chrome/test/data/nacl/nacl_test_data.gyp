# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../../../../native_client/build/untrusted.gypi',
  ],
  # TODO(ncbray) factor nexe building code into a common file.
  'targets': [
    {
      'target_name': 'nacl_tests',
      'type': 'none',
      'dependencies': [
         '../../../../ppapi/ppapi_untrusted.gyp:ppapi_cpp_lib',
         '../../../../ppapi/native_client/native_client.gyp:ppapi_lib',
         '../../../../ppapi/native_client/native_client.gyp:nacl_irt',
      ],
      'copies': [
        {
          'destination': '<(PRODUCT_DIR)/nacl_test_data/newlib',
          'files': [
            'nacl_load_test.html',
          ],
        },
        {
          'destination': '<(PRODUCT_DIR)/nacl_test_data/glibc',
          'files': [
            'nacl_load_test.html',
          ],
        },
      ],
      'variables': {
        'nexe_target': 'simple',
        'build_newlib': 1,
        'out_newlib32': '<(PRODUCT_DIR)/nacl_test_data/newlib/>(nexe_target)_newlib_x86_32.nexe',
        'out_newlib64': '<(PRODUCT_DIR)/nacl_test_data/newlib/>(nexe_target)_newlib_x86_64.nexe',
        'out_newlib_arm': '<(PRODUCT_DIR)/nacl_test_data/newlib/>(nexe_target)_newlib_arm.nexe',
        'out_glibc32': '<(PRODUCT_DIR)/nacl_test_data/glibc/>(nexe_target)_glibc_x86_32.nexe',
        'out_glibc64': '<(PRODUCT_DIR)/nacl_test_data/glibc/>(nexe_target)_glibc_x86_64.nexe',
        'out_glibc_arm': '<(PRODUCT_DIR)/nacl_test_data/glibc/>(nexe_target)_glibc_arm.nexe',
        'nmf_newlib%': '<(PRODUCT_DIR)/nacl_test_data/newlib/>(nexe_target).nmf',
        'include_dirs': [
          '../../../..',
        ],
        'link_flags': [
          '-lppapi_cpp',
          '-lppapi',
          '-lpthread',
        ],
        'sources': [
          'simple.cc',
        ],
      },
      'actions': [
        {
          'action_name': 'Generate NEWLIB NMF',
          # Unlike glibc, nexes are not actually inputs - only the names matter.
          # We don't have the nexes as inputs because the ARM nexe may not
          # exist.  However, VS 2010 seems to blackhole this entire target if
          # there are no inputs to this action.  To work around this we add a
          # bogus input.
          'inputs': ['nacl_test_data.gyp'],
          'outputs': ['>(nmf_newlib)'],
          'action': [
            'python',
            '<(DEPTH)/native_client_sdk/src/tools/create_nmf.py',
            '>(out_newlib64)', '>(out_newlib32)', '>(out_newlib_arm)',
            '--output=>(nmf_newlib)',
            '--toolchain=newlib',
          ],
        },
      ],
      'conditions': [
        ['target_arch!="arm" and disable_glibc==0', {
          'variables': {
            'build_glibc': 1,
            # NOTE: Use /lib, not /lib64 here; it is a symbolic link which
            # doesn't work on Windows.
            'libdir_glibc64': '>(nacl_glibc_tc_root)/x86_64-nacl/lib',
            'libdir_glibc32': '>(nacl_glibc_tc_root)/x86_64-nacl/lib32',
            'nacl_objdump': '>(nacl_glibc_tc_root)/bin/x86_64-nacl-objdump',
            'nmf_glibc%': '<(PRODUCT_DIR)/nacl_test_data/glibc/>(nexe_target).nmf',
          },
          'actions': [
            {
              'action_name': 'Generate GLIBC NMF and copy libs',
              'inputs': ['>(out_glibc64)', '>(out_glibc32)'],
              # NOTE: There is no explicit dependency for the lib32
              # and lib64 directories created in the PRODUCT_DIR.
              # They are created as a side-effect of NMF creation.
              'outputs': ['>(nmf_glibc)'],
              'action': [
                'python',
                '<(DEPTH)/native_client_sdk/src/tools/create_nmf.py',
                '>@(_inputs)',
                '--objdump=>(nacl_objdump)',
                '--library-path=>(libdir_glibc64)',
                '--library-path=>(libdir_glibc32)',
                '--output=>(nmf_glibc)',
                '--stage-dependencies=<(PRODUCT_DIR)/nacl_test_data/glibc',
                '--toolchain=glibc',
              ],
            },
          ],
        }],
      ],
    },
  ],

}

