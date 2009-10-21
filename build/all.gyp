{
  'includes': [
    'common.gypi',
  ],
  'targets': [
    {
      'target_name': 'pull_in_all',
      'type': 'none',
      # NOTE: Chrome-specific targets should not be part of this project
      'dependencies': [
        '../src/shared/npruntime/npruntime.gyp:*',
        '../src/shared/imc/imc.gyp:libgoogle_nacl_imc_c',
        '../src/shared/platform/platform.gyp:platform',
        '../src/shared/srpc/srpc.gyp:*',
        '../src/shared/utils/utils.gyp:*',
        '../src/trusted/desc/desc.gyp:*',
        '../src/trusted/nonnacl_util/nonnacl_util.gyp:*',
        '../src/trusted/platform_qualify/platform_qualify.gyp:*',
        '../src/trusted/plugin/plugin.gyp:*',
        '../src/trusted/service_runtime/service_runtime.gyp:*',
        '../src/trusted/validator_x86/validator_x86.gyp:*',
        # Tests
        '../src/trusted/service_runtime/service_runtime_tests.gyp:*',
      ],
      'conditions': [
        ['OS=="linux"', {
          'dependencies': [
# TODO: fix sandbox.gyp
#            '../src/trusted/sandbox/sandbox.gyp:*',
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
