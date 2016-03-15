# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'variables': {
      'application_name%': '<(application_name)',
      'application_type%': '<(application_type)',
    },
    'application_type%': '<(application_type)',
    'application_name%': '<(application_name)',
    'manifest_collator_script%':
        '<(DEPTH)/mojo/public/tools/manifest/manifest_collator.py',
    'source_manifest%': '<(source_manifest)',
    'conditions': [
      ['application_type=="mojo"', {
        'output_manifest%': '<(PRODUCT_DIR)/<(application_name)/manifest.json',
      }, {
        'output_manifest%': '<(PRODUCT_DIR)/<(application_name)_manifest.json',
      }],
    ],
  },
  'actions': [{
    'action_name': '<(_target_name)_collation',
    'inputs': [
      '<(manifest_collator_script)',
      '<(source_manifest)',
    ],
    'outputs': [
      '<(output_manifest)',
    ],
    'action': [
      'python',
      '<(manifest_collator_script)',
      '--application-name', '<(application_name)',
      '--parent=<(source_manifest)',
      '--output=<(output_manifest)',
    ],
  }],
}
