# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //extensions/shell/common/api
      'target_name': 'shell_api',
      'type': 'static_library',
      # TODO(jschuh): http://crbug.com/167187 size_t -> int
      'msvs_disabled_warnings': [ 4267 ],
      'includes': [
        '../../../../build/json_schema_bundle_compile.gypi',
        '../../../../build/json_schema_compile.gypi',
        'schemas.gypi',
      ],
    },
    {
      # GN version: //extensions/shell/common/api:extensions_features:shell_api_features
      'target_name': 'shell_api_features',
      'type': 'static_library',
      'variables': {
        'feature_class': 'APIFeature',
        'provider_class': 'ShellAPIFeatureProvider',
        'out_dir': 'extensions/shell/common/api',
        'out_base_filename': 'shell_api_features',
        'in_files': [
          'extensions/common/api/_api_features.json',
          'extensions/shell/common/api/_api_features.json',
        ],
      },
      'inputs': ['<@(in_files)'],
      'sources': ['<@(in_files)'],
      'includes': ['../../../../tools/json_schema_compiler/json_features.gypi'],
    },
    {
      # GN version: //extensions/shell/common/api:extensions_features:shell_behavior_features
      'target_name': 'shell_behavior_features',
      'type': 'static_library',
      'variables': {
        'feature_class': 'BehaviorFeature',
        'provider_class': 'ShellBehaviorFeatureProvider',
        'out_dir': 'extensions/shell/common/api',
        'out_base_filename': 'shell_behavior_features',
        'in_files': [
          'extensions/common/api/_behavior_features.json',
        ],
      },
      'inputs': ['<@(in_files)'],
      'sources': ['<@(in_files)'],
      'includes': ['../../../../tools/json_schema_compiler/json_features.gypi'],
    },
    {
      # GN version: //extensions/shell/common/api:extensions_features:shell_manifest_features
      'target_name': 'shell_manifest_features',
      'type': 'static_library',
      'variables': {
        'feature_class': 'ManifestFeature',
        'provider_class': 'ShellManifestFeatureProvider',
        'out_dir': 'extensions/shell/common/api',
        'out_base_filename': 'shell_manifest_features',
        'in_files': [
          'extensions/common/api/_manifest_features.json',
        ],
      },
      'inputs': ['<@(in_files)'],
      'sources': ['<@(in_files)'],
      'includes': ['../../../../tools/json_schema_compiler/json_features.gypi'],
    },
    {
      # GN version: //extensions/shell/common/api:extensions_features:shell_permission_features
      'target_name': 'shell_permission_features',
      'type': 'static_library',
      'variables': {
        'feature_class': 'PermissionFeature',
        'provider_class': 'ShellPermissionFeatureProvider',
        'out_dir': 'extensions/shell/common/api',
        'out_base_filename': 'shell_permission_features',
        'in_files': [
          'extensions/common/api/_permission_features.json',
        ],
      },
      'inputs': ['<@(in_files)'],
      'sources': ['<@(in_files)'],
      'includes': ['../../../../tools/json_schema_compiler/json_features.gypi'],
    },
    {
      # GN version: //extensions/shell/common/api:extensions_features
      'target_name': 'extensions_features',
      'type': 'none',
      'dependencies': [
        'shell_api_features',
        'shell_behavior_features',
        'shell_manifest_features',
        'shell_permission_features',
      ],
    },
  ],
}
