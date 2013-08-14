# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'policy_component',
      'type': '<(component)',
      'dependencies': [
        '../base/base.gyp:base',
        'json_schema',
      ],
      'defines': [
        'POLICY_COMPONENT_IMPLEMENTATION',
      ],
      'include_dirs': [
        '..',
      ],
      'conditions': [
        ['configuration_policy==1', {
          'sources': [
            'policy/core/common/policy_schema.cc',
            'policy/core/common/policy_schema.h',
            'policy/policy_export.h',
          ],
        }, {  # configuration_policy==0
          # The target 'policy_component' always exists. Later it will include
          # some stubs when configuration_policy==0. For now this stub file is
          # compiled so that an output is produced, otherwise the shared build
          # breaks on iOS.
          # TODO(joaodasilva): remove this comment and the temporary stub after
          # moving one of the real stubs. http://crbug.com/271392
          'sources': [
            'policy/stub_to_remove.cc',
          ],
        }],
      ],
    },
  ],
}
