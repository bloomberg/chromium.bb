# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'includes': [
    'nacl/nacl_defines.gypi',
  ],
  'target_defaults': {
    'variables': {
      'nacl_target': 0,
    },
    'target_conditions': [
      # This part is shared between the targets defined below. Only files and
      # settings relevant for building the Win64 target should be added here.
      ['nacl_target==1', {
        'include_dirs': [
          '<(INTERMEDIATE_DIR)',
        ],
        'defines': [
          '<@(nacl_defines)',
        ],
        'sources': [
          # .cc, .h, and .mm files under nacl that are used on all
          # platforms, including both 32-bit and 64-bit Windows.
          # Test files are also not included.
          'nacl/loader/nacl_ipc_adapter.cc',
          'nacl/loader/nacl_ipc_adapter.h',
          'nacl/loader/nacl_main.cc',
          'nacl/loader/nacl_main_platform_delegate.h',
          'nacl/loader/nacl_main_platform_delegate_linux.cc',
          'nacl/loader/nacl_main_platform_delegate_mac.mm',
          'nacl/loader/nacl_main_platform_delegate_win.cc',
          'nacl/loader/nacl_listener.cc',
          'nacl/loader/nacl_listener.h',
          'nacl/loader/nacl_validation_db.h',
          'nacl/loader/nacl_validation_query.cc',
          'nacl/loader/nacl_validation_query.h',
        ],
        # TODO(gregoryd): consider switching NaCl to use Chrome OS defines
        'conditions': [
          ['OS=="win"', {
            'defines': [
              '__STDC_LIMIT_MACROS=1',
            ],
            'include_dirs': [
              '<(DEPTH)/third_party/wtl/include',
            ],
          },],
          ['OS=="linux"', {
            'defines': [
              '__STDC_LIMIT_MACROS=1',
            ],
            'sources': [
              '../components/nacl/common/nacl_paths.cc',
              '../components/nacl/common/nacl_paths.h',
              '../components/nacl/zygote/nacl_fork_delegate_linux.cc',
              '../components/nacl/zygote/nacl_fork_delegate_linux.h',
            ],
          },],
        ],
      }],
    ],
  },
  'conditions': [
    ['disable_nacl!=1', {
      'targets': [
        {
          'target_name': 'nacl',
          'type': 'static_library',
          'variables': {
            'nacl_target': 1,
          },
          'dependencies': [
            '../base/base.gyp:base',
            '../ipc/ipc.gyp:ipc',
            '../ppapi/native_client/src/trusted/plugin/plugin.gyp:ppGoogleNaClPluginChrome',
            '../ppapi/ppapi_internal.gyp:ppapi_shared',
            '../ppapi/ppapi_internal.gyp:ppapi_ipc',
            '../native_client/src/trusted/service_runtime/service_runtime.gyp:sel_main_chrome',
          ],
          'conditions': [
            ['disable_nacl_untrusted==0', {
              'dependencies': [
                '../ppapi/native_client/native_client.gyp:nacl_irt',
                '../ppapi/native_client/src/untrusted/pnacl_irt_shim/pnacl_irt_shim.gyp:pnacl_irt_shim',
                '../ppapi/native_client/src/untrusted/pnacl_support_extension/pnacl_support_extension.gyp:pnacl_support_extension',
              ],
            }],
            ['target_arch=="mipsel"', {
              'dependencies!': [
                '../ppapi/native_client/src/untrusted/pnacl_irt_shim/pnacl_irt_shim.gyp:pnacl_irt_shim',
              ],
            }],
          ],
          'direct_dependent_settings': {
            'defines': [
              '<@(nacl_defines)',
            ],
          },
        },
        {
          'target_name': 'nacl_browser',
          'type': 'static_library',
          'sources': [
            'nacl/browser/nacl_broker_host_win.cc',
            'nacl/browser/nacl_broker_host_win.h',
            'nacl/browser/nacl_broker_service_win.cc',
            'nacl/browser/nacl_broker_service_win.h',
            'nacl/browser/nacl_browser.cc',
            'nacl/browser/nacl_browser.h',
            'nacl/browser/nacl_file_host.cc',
            'nacl/browser/nacl_file_host.h',
            'nacl/browser/nacl_host_message_filter.cc',
            'nacl/browser/nacl_host_message_filter.h',
            'nacl/browser/nacl_process_host.cc',
            'nacl/browser/nacl_process_host.h',
            'nacl/browser/nacl_validation_cache.cc',
            'nacl/browser/nacl_validation_cache.h',
            'nacl/browser/pnacl_host.cc',
            'nacl/browser/pnacl_host.h',
            'nacl/browser/pnacl_translation_cache.cc',
            'nacl/browser/pnacl_translation_cache.h',
            'nacl/common/nacl_debug_exception_handler_win.cc',
            'nacl/common/nacl_debug_exception_handler_win.h',
          ],
          'include_dirs': [
            '..',
          ],
          'dependencies': [
            'nacl_common',
            'nacl_switches',
            '../native_client/src/trusted/service_runtime/service_runtime.gyp:sel',
            '../content/content.gyp:content_browser',
          ],
          'defines': [
            '<@(nacl_defines)',
          ],
          # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
          'msvs_disabled_warnings': [4267, ],
        },
        {
          'target_name': 'nacl_renderer',
          'type': 'static_library',
          'sources': [
            'nacl/renderer/pnacl_translation_resource_host.cc',
            'nacl/renderer/pnacl_translation_resource_host.h',
            'nacl/renderer/ppb_nacl_private_impl.cc',
            'nacl/renderer/ppb_nacl_private_impl.h',
          ],
          'include_dirs': [
            '..',
          ],
          'dependencies': [
            '../content/content.gyp:content_renderer',
            '../third_party/WebKit/public/blink.gyp:blink',
            '../webkit/common/webkit_common.gyp:webkit_common',
          ],
          'defines': [
            '<@(nacl_defines)',
          ],
          'direct_dependent_settings': {
            'defines': [
              '<@(nacl_defines)',
            ],
          },
        }
      ],
      'conditions': [
        ['OS=="linux"', {
          'targets': [
            {
              'target_name': 'nacl_helper',
              'type': 'executable',
              'include_dirs': [
                '..',
              ],
              'dependencies': [
                'nacl',
                'nacl_common',
                '../crypto/crypto.gyp:crypto',
                '../sandbox/sandbox.gyp:libc_urandom_override',
                '../sandbox/sandbox.gyp:sandbox',
              ],
              'defines': [
                '<@(nacl_defines)',
                # Allow .cc files to know if they're being compiled as part
                # of nacl_helper.
                'IN_NACL_HELPER=1',
              ],
              'sources': [
                'nacl/loader/nacl_helper_linux.cc',
                'nacl/loader/nacl_helper_linux.h',
                'nacl/loader/nacl_sandbox_linux.cc',
                'nacl/loader/nonsfi/abi_conversion.cc',
                'nacl/loader/nonsfi/abi_conversion.h',
                'nacl/loader/nonsfi/elf_loader.cc',
                'nacl/loader/nonsfi/elf_loader.h',
                'nacl/loader/nonsfi/irt_basic.cc',
                'nacl/loader/nonsfi/irt_clock.cc',
                'nacl/loader/nonsfi/irt_fdio.cc',
                'nacl/loader/nonsfi/irt_futex.cc',
                'nacl/loader/nonsfi/irt_interfaces.cc',
                'nacl/loader/nonsfi/irt_interfaces.h',
                'nacl/loader/nonsfi/irt_memory.cc',
                'nacl/loader/nonsfi/irt_thread.cc',
                'nacl/loader/nonsfi/irt_util.h',
                'nacl/loader/nonsfi/nonsfi_main.cc',
                'nacl/loader/nonsfi/nonsfi_main.h',
                '../base/posix/unix_domain_socket_linux.cc',
                '../content/common/child_process_sandbox_support_impl_shm_linux.cc',
                '../content/common/sandbox_linux/sandbox_bpf_base_policy_linux.cc',
                '../content/common/sandbox_linux/sandbox_init_linux.cc',
                '../content/common/sandbox_linux/sandbox_seccomp_bpf_linux.cc',
                '../content/public/common/content_switches.cc',
              ],
              'conditions': [
                ['toolkit_uses_gtk == 1', {
                  'dependencies': [
                    '../build/linux/system.gyp:gtk',
                  ],
                }],
                ['use_glib == 1', {
                  'dependencies': [
                    '../build/linux/system.gyp:glib',
                  ],
                }],
                ['os_posix == 1 and OS != "mac"', {
                  'conditions': [
                    ['linux_use_tcmalloc==1', {
                      'dependencies': [
                        '../base/allocator/allocator.gyp:allocator',
                      ],
                    }],
                  ],
                }],
                ['use_seccomp_bpf == 0', {
                  'sources!': [
                    '../content/common/sandbox_linux/sandbox_bpf_base_policy_linux.cc',
                    '../content/common/sandbox_linux/sandbox_init_linux.cc',
                  ],
                }, {
                  'defines': ['USE_SECCOMP_BPF'],
                }],
              ],
              'cflags': ['-fPIE'],
              'link_settings': {
                'ldflags': ['-pie'],
              },
            },
          ],
        }],
        ['OS=="win" and target_arch=="ia32"', {
          'targets': [
            {
              'target_name': 'nacl_win64',
              'type': 'static_library',
              'variables': {
                'nacl_target': 1,
              },
              'dependencies': [
                'nacl_common_win64',
                '../native_client/src/trusted/service_runtime/service_runtime.gyp:sel_main_chrome64',
                '../ppapi/ppapi_internal.gyp:ppapi_shared_win64',
                '../ppapi/ppapi_internal.gyp:ppapi_ipc_win64',
              ],
              'export_dependent_settings': [
                '../ppapi/ppapi_internal.gyp:ppapi_ipc_win64',
              ],
              'sources': [
                '../components/nacl/broker/nacl_broker_listener.cc',
                '../components/nacl/broker/nacl_broker_listener.h',
                '../components/nacl/common/nacl_debug_exception_handler_win.cc',
                '../components/nacl/loader/nacl_helper_win_64.cc',
                '../components/nacl/loader/nacl_helper_win_64.h',
              ],
              'include_dirs': [
                '..',
              ],
              'defines': [
                '<@(nacl_win64_defines)',
                'COMPILE_CONTENT_STATICALLY',
              ],
              'configurations': {
                'Common_Base': {
                  'msvs_target_platform': 'x64',
                },
              },
              'direct_dependent_settings': {
                'defines': [
                  '<@(nacl_defines)',
                ],
              },
            },
            {
              'target_name': 'nacl_switches_win64',
              'type': 'static_library',
              'sources': [
                'nacl/common/nacl_switches.cc',
                'nacl/common/nacl_switches.h',
              ],
              'include_dirs': [
                '..',
              ],
              'configurations': {
                'Common_Base': {
                  'msvs_target_platform': 'x64',
                },
              },
            },
            {
              'target_name': 'nacl_common_win64',
              'type': 'static_library',
              'defines': [
                'COMPILE_CONTENT_STATICALLY',
              ],
              'sources': [
                'nacl/common/nacl_cmd_line.cc',
                'nacl/common/nacl_cmd_line.h',
                'nacl/common/nacl_messages.cc',
                'nacl/common/nacl_messages.h',
                'nacl/common/nacl_types.cc',
                'nacl/common/nacl_types.h',
              ],
              'include_dirs': [
                '..',
              ],
              'configurations': {
                'Common_Base': {
                  'msvs_target_platform': 'x64',
                },
              },
            },
          ],
        }],
      ],
    }, {  # else (disable_nacl==1)
      'targets': [
        {
          'target_name': 'nacl',
          'type': 'none',
          'sources': [],
        },
      ],
      'conditions': [
        ['OS=="win"', {
          'targets': [
            {
              'target_name': 'nacl_win64',
              'type': 'none',
              'sources': [],
            },
            {
              'target_name': 'nacl_switches_win64',
              'type': 'none',
              'sources': [],
            },
          ],
        }],
      ],
    }],
  ],
  'targets': [
    {
      'target_name': 'nacl_switches',
      'type': 'static_library',
      'sources': [
        'nacl/common/nacl_switches.cc',
        'nacl/common/nacl_switches.h',
    ],
      'include_dirs': [
        '..',
      ],
    },
    {
      'target_name': 'nacl_common',
      'type': 'static_library',
      'sources': [
        'nacl/common/nacl_cmd_line.cc',
        'nacl/common/nacl_cmd_line.h',
        'nacl/common/nacl_host_messages.h',
        'nacl/common/nacl_host_messages.cc',
        'nacl/common/nacl_messages.cc',
        'nacl/common/nacl_messages.h',
        'nacl/common/nacl_process_type.h',
        'nacl/common/nacl_sandbox_type_mac.h',
        'nacl/common/nacl_types.cc',
        'nacl/common/nacl_types.h',
        'nacl/common/pnacl_types.cc',
        'nacl/common/pnacl_types.h',
      ],
      'include_dirs': [
        '..',
      ],
    },
  ]
}
