# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    'nacl_browser_test.gypi',
  ],
  'targets': [
    {
      'target_name': 'shared_test_files',
      'type': 'none',
      'variables': {
        'build_newlib': 1,
        'build_glibc': 1,
        'build_pnacl_newlib': 1,
        'test_files': [
          # TODO(ncbray) move into chrome/test/data/nacl when all tests are
          # converted.
          '<(DEPTH)/ppapi/native_client/tests/ppapi_browser/progress_event_listener.js',
          '<(DEPTH)/ppapi/native_client/tests/ppapi_browser/bad/ppapi_bad.js',
          '<(DEPTH)/ppapi/native_client/tools/browser_tester/browserdata/nacltest.js',
        ],
      },
    },
    {
      'target_name': 'simple_test',
      'type': 'none',
      'variables': {
        'nexe_target': 'simple',
        'build_newlib': 1,
        'build_glibc': 1,
        'build_pnacl_newlib': 1,
        'sources': [
          'simple.cc',
        ],
        'test_files': [
          'nacl_load_test.html',
        ],
      },
    },
    {
      'target_name': 'exit_status_test',
      'type': 'none',
      'variables': {
        'nexe_target': 'pm_exit_status_test',
        'build_newlib': 1,
        'build_glibc': 1,
        'build_pnacl_newlib': 1,
        'sources': [
          'exit_status/pm_exit_status_test.cc',
        ],
        'test_files': [
          'exit_status/pm_exit_status_test.html',
        ],
      },
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
        '<(DEPTH)/ppapi/native_client/native_client.gyp:ppapi_lib',
        '<(DEPTH)/ppapi/ppapi_untrusted.gyp:ppapi_cpp_lib',
        'ppapi_test_lib',
      ],
    },
    {
      'target_name': 'pnacl_error_handling_test',
      'type': 'none',
      'variables': {
        'nexe_target': 'pnacl_error_handling',
        'build_pnacl_newlib': 1,
        'sources': [
          'pnacl_error_handling/program_fragment.cc',
        ],
        # Only compile the program_fragment and avoid linking so that
        # it will be just a program fragment to test error handling
        # of link failures.
        'extra_args': [
          '--compile',
        ],
        # Explicitly state the name of the gyp output.  The default is a
        # ".pexe" and --compile causes the compilation to stop with a ".o".
        'out_pnacl_newlib': '>(nacl_pnacl_newlib_out_dir)/program_fragment.o',
        'objdir_pnacl_newlib': '>(nacl_pnacl_newlib_out_dir)',
        # Keep debug metadata out, so that the "program" can roughly
        # follow the PNaCl stable ABI.
        'compile_flags!': [
          '-g',
        ],
        # Need to not translate this program_fragment since linking will fail.
        'enable_x86_32': 0,
        'enable_x86_64': 0,
        'enable_arm': 0,
        # Use a prebuilt nmf file referring to the .o file instead of
        # trying to use the generate NMF rules, which will look for a .pexe.
        'generate_nmf': 0,
        'test_files': [
          'pnacl_error_handling/pnacl_error_handling.html',
          'pnacl_error_handling/bad.pexe',
          'pnacl_error_handling/bad2.pexe',
          'pnacl_error_handling/pnacl_bad_pexe.nmf',
          'pnacl_error_handling/pnacl_bad2_pexe.nmf',
          'pnacl_error_handling/pnacl_bad_doesnotexist.nmf',
          'pnacl_error_handling/pnacl_bad_pexe_undefined_syms.nmf',
        ],
      },
      'dependencies': [
        '<(DEPTH)/native_client/tools.gyp:prep_toolchain',
      ]
    },
    {
      'target_name': 'pnacl_options_test',
      'type': 'none',
      'variables': {
        'nexe_target': 'pnacl_options',
        'build_pnacl_newlib': 1,
        # No need to translate these AOT, when we just need the pexe.
        'enable_x86_32': 0,
        'enable_x86_64': 0,
        'enable_arm': 0,
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
      ]
    },
    {
      'target_name': 'pnacl_dyncode_syscall_disabled_test',
      'type': 'none',
      'variables': {
        # This tests that nexes produced by translation in the browser are not
        # able to use the dyncode syscalls.  Pre-translated nexes are not
        # subject to this constraint, so we do not test them.
        'enable_x86_32': 0,
        'enable_x86_64': 0,
        'enable_arm': 0,
        'nexe_target': 'pnacl_dyncode_syscall_disabled',
        'build_pnacl_newlib': 1,
        'link_flags': [
          '-lppapi',
          '-lppapi_test_lib',
          '-lplatform',
          '-lgio',
          '-lnacl_dyncode',
        ],
        'sources': [
          'pnacl_dyncode_syscall_disabled/pnacl_dyncode_syscall_disabled.cc',
        ],
        'test_files': [
          'pnacl_dyncode_syscall_disabled/pnacl_dyncode_syscall_disabled.html',
        ],
      },
      'dependencies': [
        '<(DEPTH)/native_client/src/shared/gio/gio.gyp:gio_lib',
        '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform_lib',
        '<(DEPTH)/native_client/src/untrusted/nacl/nacl.gyp:nacl_dynacode_lib',
        '<(DEPTH)/native_client/tools.gyp:prep_toolchain',
        '<(DEPTH)/ppapi/native_client/native_client.gyp:ppapi_lib',
        '<(DEPTH)/ppapi/ppapi_untrusted.gyp:ppapi_cpp_lib',
        'ppapi_test_lib',
      ],
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
        '<(DEPTH)/ppapi/native_client/native_client.gyp:ppapi_lib',
        '<(DEPTH)/ppapi/ppapi_untrusted.gyp:ppapi_cpp_lib',
        'ppapi_test_lib',
      ],
    },
  ],
}
