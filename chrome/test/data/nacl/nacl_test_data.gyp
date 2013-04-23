# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../../../../native_client/build/untrusted.gypi',
  ],
  'targets': [
    {
      'target_name': 'shared_test_files',
      'type': 'none',
      'variables': {
        'build_newlib': 1,
        'build_glibc': 1,
        'build_pnacl_newlib': 1,
        'nexe_destination_dir': 'nacl_test_data',
        'current_depth': '<(DEPTH)',
        'test_files': [
          # TODO(ncbray) move into chrome/test/data/nacl when all tests are
          # converted.
          '<(DEPTH)/ppapi/native_client/tests/ppapi_browser/progress_event_listener.js',
          '<(DEPTH)/ppapi/native_client/tools/browser_tester/browserdata/nacltest.js',
        ],
      },
      'dependencies': [
        '<(DEPTH)/ppapi/ppapi_nacl_test_common.gyp:nacl_test_common',
      ],
    },
    {
      'target_name': 'simple_test',
      'type': 'none',
      'variables': {
        'nexe_target': 'simple',
        'build_newlib': 1,
        'build_glibc': 1,
        'build_pnacl_newlib': 1,
        'nexe_destination_dir': 'nacl_test_data',
        'current_depth': '<(DEPTH)',
        'sources': [
          'simple.cc',
        ],
        'test_files': [
          'nacl_load_test.html',
        ],
      },
      'dependencies': [
        '<(DEPTH)/ppapi/ppapi_nacl_test_common.gyp:nacl_test_common',
      ],
    },
    {
      'target_name': 'exit_status_test',
      'type': 'none',
      'variables': {
        'nexe_target': 'pm_exit_status_test',
        'build_newlib': 1,
        'build_glibc': 1,
        'build_pnacl_newlib': 1,
        'nexe_destination_dir': 'nacl_test_data',
        'current_depth': '<(DEPTH)',
        'sources': [
          'exit_status/pm_exit_status_test.cc',
        ],
        'test_files': [
          'exit_status/pm_exit_status_test.html',
        ],
      },
      'dependencies': [
        '<(DEPTH)/ppapi/ppapi_nacl_test_common.gyp:nacl_test_common',
      ],
    },
    {
      'target_name': 'ppapi_test_lib',
      'type': 'none',
      'variables': {
        'nlib_target': 'libppapi_test_lib.a',
        'nso_target': 'libppapi_test_lib.so',
        'build_newlib': 1,
        'build_glibc': 1,
        'build_pnacl_newlib': 1,
        'nexe_destination_dir': 'nacl_test_data',
        'current_depth': '<(DEPTH)',
        'sources': [
          # TODO(ncbray) move these files once SCons no longer depends on them.
          '../../../../ppapi/native_client/tests/ppapi_test_lib/get_browser_interface.cc',
          '../../../../ppapi/native_client/tests/ppapi_test_lib/internal_utils.cc',
          '../../../../ppapi/native_client/tests/ppapi_test_lib/module_instance.cc',
          '../../../../ppapi/native_client/tests/ppapi_test_lib/testable_callback.cc',
          '../../../../ppapi/native_client/tests/ppapi_test_lib/test_interface.cc',
        ]
      },
      'dependencies': [
        '<(DEPTH)/native_client/tools.gyp:prep_toolchain',
        '<(DEPTH)/ppapi/ppapi_nacl_test_common.gyp:nacl_test_common',
      ],
    },
    {
      'target_name': 'ppapi_progress_events',
      'type': 'none',
      'variables': {
        'nexe_target': 'ppapi_progress_events',
        'build_newlib': 1,
        'build_glibc': 1,
        'build_pnacl_newlib': 1,
        'nexe_destination_dir': 'nacl_test_data',
        'current_depth': '<(DEPTH)',
        'link_flags': [
          '-lppapi',
          '-lppapi_test_lib',
          '-lplatform',
          '-lgio',
        ],
        'sources': [
          'progress_events/ppapi_progress_events.cc',
        ],
        'test_files': [
          'progress_events/ppapi_progress_events.html',
        ],
      },
      'dependencies': [
        '<(DEPTH)/native_client/tools.gyp:prep_toolchain',
        '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform_lib',
        '<(DEPTH)/native_client/src/shared/gio/gio.gyp:gio_lib',
        '<(DEPTH)/ppapi/ppapi_nacl_test_common.gyp:nacl_test_common',
        'ppapi_test_lib',
      ],
    },
    {
      'target_name': 'pnacl_options_test',
      'type': 'none',
      'variables': {
        'nexe_target': 'pnacl_options',
        'build_pnacl_newlib': 1,
        'nexe_destination_dir': 'nacl_test_data',
        'current_depth': '<(DEPTH)',
        'sources': [
          'simple.cc',
        ],
        'test_files': [
          'pnacl_nmf_options/pnacl_options.html',
          'pnacl_nmf_options/pnacl_o_0.nmf',
          'pnacl_nmf_options/pnacl_o_2.nmf',
          'pnacl_nmf_options/pnacl_o_large.nmf',
          'pnacl_nmf_options/pnacl_time_passes.nmf',
        ],
      },
      'dependencies': [
        '<(DEPTH)/native_client/tools.gyp:prep_toolchain',
        '<(DEPTH)/ppapi/ppapi_nacl_test_common.gyp:nacl_test_common',
      ]
    },
    # Legacy NaCl PPAPI interface tests being here.
    {
      'target_name': 'ppapi_ppb_core',
      'type': 'none',
      'variables': {
        'nexe_target': 'ppapi_ppb_core',
        'build_newlib': 1,
        'build_glibc': 1,
        'build_pnacl_newlib': 1,
        'nexe_destination_dir': 'nacl_test_data',
        'current_depth': '<(DEPTH)',
        'link_flags': [
          '-lppapi',
          '-lppapi_test_lib',
          '-lplatform',
          '-lgio',
        ],
        'sources': [
          'ppapi/ppb_core/ppapi_ppb_core.cc',
        ],
        'test_files': [
          'ppapi/ppb_core/ppapi_ppb_core.html',
        ],
      },
      'dependencies': [
        '<(DEPTH)/native_client/tools.gyp:prep_toolchain',
        '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform_lib',
        '<(DEPTH)/native_client/src/shared/gio/gio.gyp:gio_lib',
        '<(DEPTH)/ppapi/ppapi_nacl_test_common.gyp:nacl_test_common',
        'ppapi_test_lib',
      ],
    },
  ],
}
