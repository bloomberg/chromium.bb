# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'includes': [
    '../build/common_untrusted.gypi',
  ],
  'conditions': [
    ['disable_nacl==0 and disable_nacl_untrusted==0', {
      'targets': [
        {
          # nacl_helper_nonsfi is similar to nacl_helper (built in nacl.gyp)
          # but for the NaCl plugin in Non-SFI mode.
          # This binary is built using the PNaCl toolchain, but it is native
          # linux binary and will run on Linux directly.
          # Most library code can be shared with the one for untrusted build
          # (i.e. the one for irt.nexe built by the NaCl/PNaCl toolchain), but
          # as nacl_helper_nonsfi runs on Linux, there are some differences,
          # such as MessageLoopForIO (which is based on libevent in Non-SFI
          # mode) or ipc_channel implementation.
          # Because of the toolchain, in both builds, OS_NACL macro (derived
          # from __native_client__ macro) is defined. Code can test whether
          # __native_client_nonsfi__ is #defined in order to determine
          # whether it is being compiled for SFI mode or Non-SFI mode.
          #
          # Currently, nacl_helper_nonsfi is under development and the binary
          # does nothing (i.e. it has only empty main(), now).
          # TODO(crbug.com/358465): Implement it then switch nacl_helper in
          # Non-SFI mode to nacl_helper_nonsfi.
          'target_name': 'nacl_helper_nonsfi',
          'type': 'none',
          'variables': {
            'nacl_untrusted_build': 1,
            'nexe_target': 'nacl_helper_nonsfi',
            # Rename the output binary file to nacl_helper_nonsfi and put it
            # directly under out/{Debug,Release}/.
            'out_newlib32_nonsfi': '<(PRODUCT_DIR)/nacl_helper_nonsfi',
            'out_newlib_arm_nonsfi': '<(PRODUCT_DIR)/nacl_helper_nonsfi',

            'build_glibc': 0,
            'build_newlib': 0,
            'build_irt': 0,
            'build_pnacl_newlib': 0,
            'build_nonsfi_helper': 1,

            'sources': [
              'nacl/common/nacl_messages.cc',
              'nacl/common/nacl_switches.cc',
              'nacl/common/nacl_types.cc',
              'nacl/common/nacl_types_param_traits.cc',
              'nacl/loader/nacl_helper_linux.cc',
              'nacl/loader/nacl_trusted_listener.cc',
              'nacl/loader/nonsfi/nonsfi_listener.cc',
              'nacl/loader/nonsfi/nonsfi_main.cc',
              'nacl/loader/sandbox_linux/nacl_sandbox_linux.cc',
            ],

            'link_flags': [
              '-lbase_nacl_nonsfi',
              '-lcommand_buffer_client_nacl',
              '-lcommand_buffer_common_nacl',
              '-lcontent_common_nacl_nonsfi',
              '-lelf_loader',
              '-levent_nacl_nonsfi',
              '-lgio',
              '-lgles2_cmd_helper_nacl',
              '-lgles2_implementation_nacl',
              '-lgles2_utils_nacl',
              '-lgpu_ipc_nacl',
              '-lipc_nacl_nonsfi',
              '-llatency_info_nacl',
              '-lplatform',
              '-lppapi_ipc_nacl',
              '-lppapi_proxy_nacl',
              '-lppapi_shared_nacl',
              '-lsandbox_nacl_nonsfi',
              '-lshared_memory_support_nacl',
              '-ltracing_nacl',
            ],

            'conditions': [
              ['target_arch=="ia32" or target_arch=="x64"', {
                'extra_deps_newlib32_nonsfi': [
                  '>(tc_lib_dir_nonsfi_helper32)/libbase_nacl_nonsfi.a',
                  '>(tc_lib_dir_nonsfi_helper32)/libcommand_buffer_client_nacl.a',
                  '>(tc_lib_dir_nonsfi_helper32)/libcommand_buffer_common_nacl.a',
                  '>(tc_lib_dir_nonsfi_helper32)/libcontent_common_nacl_nonsfi.a',
                  '>(tc_lib_dir_nonsfi_helper32)/libelf_loader.a',
                  '>(tc_lib_dir_nonsfi_helper32)/libevent_nacl_nonsfi.a',
                  '>(tc_lib_dir_nonsfi_helper32)/libgio.a',
                  '>(tc_lib_dir_nonsfi_helper32)/libgles2_cmd_helper_nacl.a',
                  '>(tc_lib_dir_nonsfi_helper32)/libgles2_implementation_nacl.a',
                  '>(tc_lib_dir_nonsfi_helper32)/libgles2_utils_nacl.a',
                  '>(tc_lib_dir_nonsfi_helper32)/libgpu_ipc_nacl.a',
                  '>(tc_lib_dir_nonsfi_helper32)/libipc_nacl_nonsfi.a',
                  '>(tc_lib_dir_nonsfi_helper32)/liblatency_info_nacl.a',
                  '>(tc_lib_dir_nonsfi_helper32)/libplatform.a',
                  '>(tc_lib_dir_nonsfi_helper32)/libppapi_ipc_nacl.a',
                  '>(tc_lib_dir_nonsfi_helper32)/libppapi_proxy_nacl.a',
                  '>(tc_lib_dir_nonsfi_helper32)/libppapi_shared_nacl.a',
                  '>(tc_lib_dir_nonsfi_helper32)/libsandbox_nacl_nonsfi.a',
                  '>(tc_lib_dir_nonsfi_helper32)/libshared_memory_support_nacl.a',
                  '>(tc_lib_dir_nonsfi_helper32)/libtracing_nacl.a',
                ],
              }],
              ['target_arch=="arm"', {
                'extra_deps_newlib_arm_nonsfi': [
                  '>(tc_lib_dir_nonsfi_helper_arm)/libbase_nacl_nonsfi.a',
                  '>(tc_lib_dir_nonsfi_helper_arm)/libcommand_buffer_client_nacl.a',
                  '>(tc_lib_dir_nonsfi_helper_arm)/libcommand_buffer_common_nacl.a',
                  '>(tc_lib_dir_nonsfi_helper_arm)/libcontent_common_nacl_nonsfi.a',
                  '>(tc_lib_dir_nonsfi_helper_arm)/libelf_loader.a',
                  '>(tc_lib_dir_nonsfi_helper_arm)/libevent_nacl_nonsfi.a',
                  '>(tc_lib_dir_nonsfi_helper_arm)/libgio.a',
                  '>(tc_lib_dir_nonsfi_helper_arm)/libgles2_cmd_helper_nacl.a',
                  '>(tc_lib_dir_nonsfi_helper_arm)/libgles2_implementation_nacl.a',
                  '>(tc_lib_dir_nonsfi_helper_arm)/libgles2_utils_nacl.a',
                  '>(tc_lib_dir_nonsfi_helper_arm)/libgpu_ipc_nacl.a',
                  '>(tc_lib_dir_nonsfi_helper_arm)/libipc_nacl_nonsfi.a',
                  '>(tc_lib_dir_nonsfi_helper_arm)/liblatency_info_nacl.a',
                  '>(tc_lib_dir_nonsfi_helper_arm)/libplatform.a',
                  '>(tc_lib_dir_nonsfi_helper_arm)/libppapi_ipc_nacl.a',
                  '>(tc_lib_dir_nonsfi_helper_arm)/libppapi_proxy_nacl.a',
                  '>(tc_lib_dir_nonsfi_helper_arm)/libppapi_shared_nacl.a',
                  '>(tc_lib_dir_nonsfi_helper_arm)/libsandbox_nacl_nonsfi.a',
                  '>(tc_lib_dir_nonsfi_helper_arm)/libshared_memory_support_nacl.a',
                  '>(tc_lib_dir_nonsfi_helper_arm)/libtracing_nacl.a',
                ],
              }],
            ],
          },
          'dependencies': [
            '../base/base_nacl.gyp:base_nacl_nonsfi',
            '../content/content_nacl_nonsfi.gyp:content_common_nacl_nonsfi',
            '../ipc/ipc_nacl.gyp:ipc_nacl_nonsfi',
            '../native_client/src/nonsfi/irt/irt.gyp:nacl_sys_private',
            '../native_client/src/nonsfi/loader/loader.gyp:elf_loader',
            '../native_client/src/untrusted/nacl/nacl.gyp:nacl_lib_newlib',
            '../native_client/tools.gyp:prep_toolchain',
            '../ppapi/ppapi_proxy_nacl.gyp:ppapi_proxy_nacl',
            '../sandbox/sandbox_nacl_nonsfi.gyp:sandbox_nacl_nonsfi',
          ],
        },
        # TODO(hidehiko): Add Non-SFI version of nacl_loader_unittests.
      ],
    }],
  ],
}
