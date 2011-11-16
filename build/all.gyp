{
  'includes': [
    'common.gypi',
  ],
  'targets': [
    {
      'target_name': 'pull_in_all',
      'type': 'none',
      'dependencies': [
        '../src/shared/gio/gio.gyp:*',
        '../src/shared/imc/imc.gyp:*',
        '../src/shared/platform/platform.gyp:*',
        '../src/shared/srpc/srpc.gyp:*',
        '../src/shared/utils/utils.gyp:*',
        '../src/trusted/debug_stub/debug_stub.gyp:*',
        '../src/trusted/desc/desc.gyp:*',
        '../src/trusted/gdb_rsp/gdb_rsp.gyp:*',
        '../src/trusted/nacl_base/nacl_base.gyp:*',
        '../src/trusted/nonnacl_util/nonnacl_util.gyp:*',
        '../src/trusted/perf_counter/perf_counter.gyp:*',
        '../src/trusted/platform_qualify/platform_qualify.gyp:*',
# TODO(robertm): Make sel_universal work without relying on the old input
# events. See http://code.google.com/p/nativeclient/issues/detail?id=2066
#        '../src/trusted/sel_universal/sel_universal.gyp:*',
        '../src/trusted/service_runtime/service_runtime.gyp:*',
      ],
      'conditions': [
        ['target_arch!="arm"', {
          'dependencies': [
            '../src/untrusted/irt/irt.gyp:irt_core_nexe',
            '../src/untrusted/irt_stub/irt_stub.gyp:*',
            '../src/untrusted/nacl/nacl.gyp:*',
            '../src/untrusted/nosys/nosys.gyp:*',
            '../src/untrusted/pthread/pthread.gyp:*',
            '../tests.gyp:*',
          ],
        }],
        ['target_arch=="arm"', {
          'dependencies': [
            '../src/trusted/validator_arm/validator_arm.gyp:*',
          ],
        }, {
          'dependencies': [
            '../src/trusted/validator_x86/validator_x86.gyp:*',
          ],
        }],
        ['OS=="win" and nacl_standalone==0', {
          'dependencies': [
            '../src/trusted/handle_pass/handle_pass.gyp:*',
          ],
        }],
      ],
    },
  ],
}
