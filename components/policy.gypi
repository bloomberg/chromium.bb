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
            'policy/core/common/policy_details.h',
            'policy/core/common/policy_namespace.cc',
            'policy/core/common/policy_namespace.h',
            'policy/core/common/policy_pref_names.cc',
            'policy/core/common/policy_pref_names.h',
            'policy/core/common/policy_switches.cc',
            'policy/core/common/policy_switches.h',
            'policy/core/common/schema.cc',
            'policy/core/common/schema.h',
            'policy/core/common/schema_internal.h',
            'policy/policy_export.h',
          ],
        }, {  # configuration_policy==0
          # Some of the policy code is always enabled, so that other parts of
          # Chrome can always interface with the PolicyService without having
          # to #ifdef on ENABLE_CONFIGURATION_POLICY.
          'sources': [
            'policy/core/common/policy_namespace.cc',
            'policy/core/common/policy_namespace.h',
          ],
        }],
      ],
    },
  ],
}
