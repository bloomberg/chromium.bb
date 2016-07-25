# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //extensions/test:extensions_features:test_api_features
      'target_name': 'test_api_features',
      'type': 'static_library',
      'variables': {
        'feature_class': 'APIFeature',
        'provider_class': 'TestAPIFeatureProvider',
        'out_dir': 'extensions/test',
        'out_base_filename': 'test_api_features',
        'in_files': [
          'extensions/common/api/_api_features.json',
        ],
      },
      'inputs': ['<@(in_files)'],
      'sources': ['<@(in_files)'],
      'includes': ['../../tools/json_schema_compiler/json_features.gypi'],
    },
    {
      # GN version: //extensions/test:extensions_features:test_behavior_features
      'target_name': 'test_behavior_features',
      'type': 'static_library',
      'variables': {
        'feature_class': 'BehaviorFeature',
        'provider_class': 'TestBehaviorFeatureProvider',
        'out_dir': 'extensions/test',
        'out_base_filename': 'test_behavior_features',
        'in_files': [
          'extensions/common/api/_behavior_features.json',
        ],
      },
      'inputs': ['<@(in_files)'],
      'sources': ['<@(in_files)'],
      'includes': ['../../tools/json_schema_compiler/json_features.gypi'],
    },
    {
      # GN version: //extensions/test:extensions_features:test_manifest_features
      'target_name': 'test_manifest_features',
      'type': 'static_library',
      'variables': {
        'feature_class': 'ManifestFeature',
        'provider_class': 'TestManifestFeatureProvider',
        'out_dir': 'extensions/test',
        'out_base_filename': 'test_manifest_features',
        'in_files': [
          'extensions/common/api/_manifest_features.json',
        ],
      },
      'inputs': ['<@(in_files)'],
      'sources': ['<@(in_files)'],
      'includes': ['../../tools/json_schema_compiler/json_features.gypi'],
    },
    {
      # GN version: //extensions/test:extensions_features:test_permission_features
      'target_name': 'test_permission_features',
      'type': 'static_library',
      'variables': {
        'feature_class': 'PermissionFeature',
        'provider_class': 'TestPermissionFeatureProvider',
        'out_dir': 'extensions/test',
        'out_base_filename': 'test_permission_features',
        'in_files': [
          'extensions/common/api/_permission_features.json',
        ],
      },
      'inputs': ['<@(in_files)'],
      'sources': ['<@(in_files)'],
      'includes': ['../../tools/json_schema_compiler/json_features.gypi'],
    },
    {
      # GN version: //extensions/test:extensions_features
      'target_name': 'extensions_features',
      'type': 'none',
      'dependencies': [
        'test_api_features',
        'test_behavior_features',
        'test_manifest_features',
        'test_permission_features',
      ],
    },
  ],
}
