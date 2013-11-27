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
        'component_strings.gyp:component_strings',
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
            'policy/core/common/policy_bundle.cc',
            'policy/core/common/policy_bundle.h',
            'policy/core/common/external_data_fetcher.cc',
            'policy/core/common/external_data_fetcher.h',
            'policy/core/common/external_data_manager.h',
            'policy/core/common/policy_details.h',
            'policy/core/common/policy_namespace.cc',
            'policy/core/common/policy_namespace.h',
            'policy/core/common/policy_map.cc',
            'policy/core/common/policy_map.h',
            'policy/core/common/policy_pref_names.cc',
            'policy/core/common/policy_pref_names.h',
            'policy/core/common/policy_switches.cc',
            'policy/core/common/policy_switches.h',
            'policy/core/common/policy_types.h',
            'policy/core/common/preferences_mac.cc',
            'policy/core/common/preferences_mac.h',
            'policy/core/common/registry_dict_win.cc',
            'policy/core/common/registry_dict_win.h',
            'policy/core/common/schema.cc',
            'policy/core/common/schema.h',
            'policy/core/common/schema_internal.h',
            'policy/core/common/schema_map.cc',
            'policy/core/common/schema_map.h',
            'policy/core/common/schema_registry.cc',
            'policy/core/common/schema_registry.h',
            'policy/policy_export.h',
          ],
        }, {  # configuration_policy==0
          # Some of the policy code is always enabled, so that other parts of
          # Chrome can always interface with the PolicyService without having
          # to #ifdef on ENABLE_CONFIGURATION_POLICY.
          'sources': [
            'policy/core/common/external_data_fetcher.h',
            'policy/core/common/external_data_fetcher.cc',
            'policy/core/common/external_data_manager.h',
            'policy/core/common/policy_map.cc',
            'policy/core/common/policy_map.h',
            'policy/core/common/policy_namespace.cc',
            'policy/core/common/policy_namespace.h',
          ],
        }],
      ],
    },
    {
      'target_name': 'policy_test_support',
      'type': 'static_library',
      'defines!': ['POLICY_COMPONENT_IMPLEMENTATION'],
      'dependencies': [],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'policy/core/common/preferences_mock_mac.cc',
        'policy/core/common/preferences_mock_mac.h',
        # TODO: Remove this once there are more test support files.
        # Prevents the Windows build from breaking.
        'policy/stub_to_remove.cc',
      ],
      'conditions': [],
    },
  ],
}
