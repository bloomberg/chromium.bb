# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //components/syncable_prefs
      'target_name': 'syncable_prefs',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../base/base.gyp:base_prefs',
        '../sync/sync.gyp:sync',
        'pref_registry',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'syncable_prefs/pref_model_associator.cc',
        'syncable_prefs/pref_model_associator.h',
        'syncable_prefs/pref_model_associator_client.h',
        'syncable_prefs/pref_service_syncable.cc',
        'syncable_prefs/pref_service_syncable.h',
        'syncable_prefs/pref_service_syncable_factory.cc',
        'syncable_prefs/pref_service_syncable_factory.h',
        'syncable_prefs/pref_service_syncable_observer.h',
        'syncable_prefs/synced_pref_change_registrar.cc',
        'syncable_prefs/synced_pref_change_registrar.h',
        'syncable_prefs/synced_pref_observer.h',
      ],
      'conditions': [
        ['configuration_policy==1', {
          'dependencies': [
            'policy_component_browser',
            'policy_component_common',
          ],
        }],
      ],
    },
    {
      # GN version: //components/syncable_prefs:test_support
      'target_name': 'syncable_prefs_test_support',
      'type': 'static_library',
      'dependencies': [
        '../testing/gtest.gyp:gtest',
        'syncable_prefs',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'syncable_prefs/pref_service_mock_factory.cc',
        'syncable_prefs/pref_service_mock_factory.h',
        'syncable_prefs/testing_pref_service_syncable.cc',
        'syncable_prefs/testing_pref_service_syncable.h',
      ],
    },
  ],
}
