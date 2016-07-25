# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //chrome/common/extensions/api:api
      'target_name': 'chrome_api',
      'type': 'static_library',
      'sources': [
        '<@(schema_files)',
      ],
      # TODO(jschuh): http://crbug.com/167187 size_t -> int
      'msvs_disabled_warnings': [ 4267 ],
      'includes': [
        '../../../../build/json_schema_bundle_compile.gypi',
        '../../../../build/json_schema_compile.gypi',
        'schemas.gypi',
      ],
      'dependencies': [
        '<@(schema_dependencies)',
      ],
    },
    {
      # GN version: //chrome/common/extensions/api:extensions_features:api_features
      'target_name': 'api_features',
      'type': 'static_library',
      'variables': {
        'feature_class': 'APIFeature',
        'provider_class': 'APIFeatureProvider',
        'out_dir': 'chrome/common/extensions/api',
        'out_base_filename': 'api_features',
        'in_files': [
          'chrome/common/extensions/api/_api_features.json',
          'extensions/common/api/_api_features.json',
        ],
      },
      'inputs': ['<@(in_files)'],
      'sources': ['<@(in_files)'],
      'includes': ['../../../../tools/json_schema_compiler/json_features.gypi'],
    },
    {
      # GN version: //chrome/common/extensions/api:extensions_features:behavior_features
      'target_name': 'behavior_features',
      'type': 'static_library',
      'variables': {
        'feature_class': 'BehaviorFeature',
        'provider_class': 'BehaviorFeatureProvider',
        'out_dir': 'chrome/common/extensions/api',
        'out_base_filename': 'behavior_features',
        'in_files': [
          'extensions/common/api/_behavior_features.json',
        ],
      },
      'inputs': ['<@(in_files)'],
      'sources': ['<@(in_files)'],
      'includes': ['../../../../tools/json_schema_compiler/json_features.gypi'],
    },
    {
      # GN version: //chrome/common/extensions/api:extensions_features:manifest_features
      'target_name': 'manifest_features',
      'type': 'static_library',
      'variables': {
        'feature_class': 'ManifestFeature',
        'provider_class': 'ManifestFeatureProvider',
        'out_dir': 'chrome/common/extensions/api',
        'out_base_filename': 'manifest_features',
        'in_files': [
          'chrome/common/extensions/api/_manifest_features.json',
          'extensions/common/api/_manifest_features.json',
        ],
      },
      'inputs': ['<@(in_files)'],
      'sources': ['<@(in_files)'],
      'includes': ['../../../../tools/json_schema_compiler/json_features.gypi'],
    },
    {
      # GN version: //chrome/common/extensions/api:extensions_features:permission_features
      'target_name': 'permission_features',
      'type': 'static_library',
      'variables': {
        'feature_class': 'PermissionFeature',
        'provider_class': 'PermissionFeatureProvider',
        'out_dir': 'chrome/common/extensions/api',
        'out_base_filename': 'permission_features',
        'in_files': [
          'chrome/common/extensions/api/_permission_features.json',
          'extensions/common/api/_permission_features.json',
        ],
      },
      'inputs': ['<@(in_files)'],
      'sources': ['<@(in_files)'],
      'includes': ['../../../../tools/json_schema_compiler/json_features.gypi'],
    },
    {
      # GN version: //chrome/common/extensions/api:extensions_features
      'target_name': 'extensions_features',
      'type': 'none',
      'dependencies': [
        'api_features',
        'behavior_features',
        'manifest_features',
        'permission_features',
      ],
    },
  ],
}
