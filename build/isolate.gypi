# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This file is meant to be included into a target to provide a rule
# to "build" .isolate files into a .isolated file.
#
# To use this, create a gyp target with the following form:
# 'conditions': [
#   ['test_isolation_mode != "noop"', {
#     'targets': [
#       {
#         'target_name': 'foo_test_run',
#         'type': 'none',
#         'dependencies': [
#           'foo_test',
#         ],
#         'includes': [
#           '../build/isolate.gypi',
#           'foo_test.isolate',
#         ],
#         'sources': [
#           'foo_test.isolate',
#         ],
#       },
#     ],
#   }],
# ],
#
# Note: foo_test.isolate is included and a source file. It is an inherent
# property of the .isolate format. This permits to define GYP variables but is
# a stricter format than GYP so isolate.py can read it.
#
# The generated .isolated file will be:
#   <(PRODUCT_DIR)/foo_test.isolated
#
# See http://dev.chromium.org/developers/testing/isolated-testing/for-swes
# for more information.

{
  'rules': [
    {
      'rule_name': 'isolate',
      'extension': 'isolate',
      'inputs': [
        # Files that are known to be involved in this step.
        '<(DEPTH)/tools/swarming_client/isolate.py',
        '<(DEPTH)/tools/swarming_client/run_isolated.py',

        # Disable file tracking by the build driver for now. This means the
        # project must have the proper build-time dependency for their runtime
        # dependency. This improves the runtime of the build driver since it
        # doesn't have to stat() all these files.
        #
        # More importantly, it means that even if a isolate_dependency_tracked
        # file is missing, for example if a file was deleted and the .isolate
        # file was not updated, that won't break the build, especially in the
        # case where foo_tests_run is not built! This should be reenabled once
        # the switch-over to running tests on Swarm is completed.
        #'<@(isolate_dependency_tracked)',
      ],
      'outputs': [
        '<(PRODUCT_DIR)/<(RULE_INPUT_ROOT).isolated',
      ],
      'action': [
        'python',
        '<(DEPTH)/tools/swarming_client/isolate.py',
        '<(test_isolation_mode)',
        # Variables should use the -V FOO=<(FOO) form so frequent values,
        # like '0' or '1', aren't stripped out by GYP.
        '--path-variable', 'PRODUCT_DIR', '<(PRODUCT_DIR) ',
        '--config-variable', 'OS=<(OS)',
        '--result', '<@(_outputs)',
        '--isolate', '<(RULE_INPUT_PATH)',
      ],
      'conditions': [
        # Note: When gyp merges lists, it appends them to the old value.
        ['OS=="mac"', {
          # <(mac_product_name) can contain a space, so don't use FOO=<(FOO)
          # form.
          'action': [
            '--extra-variable', 'mac_product_name', '<(mac_product_name)',
          ],
        }],
        ["test_isolation_outdir==''", {
          # GYP will eliminate duplicate arguments so '<(PRODUCT_DIR)' cannot
          # be provided twice. To work around this behavior, append '/'.
          #
          # Also have a space after <(PRODUCT_DIR) or visual studio will
          # escape the argument wrappping " with the \ and merge it into
          # the following arguments.
          'action': [ '--outdir', '<(PRODUCT_DIR)/ ' ],
        }, {
          'action': [ '--outdir', '<(test_isolation_outdir)' ],
        }],
        ['test_isolation_fail_on_missing == 0', {
            'action': ['--ignore_broken_items'],
          },
        ],
      ],

      'msvs_cygwin_shell': 0,
    },
  ],
}
