# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Generates an output manifest based on a Jinja2 templated manifest.
# Include this file inside of your target to generate a manifest.
# The following variables must be set before including this file:
#
# template_manifest_path: a valid Jinja2 file path.
# output_manifest_path: file path for the resulting manifest.
#
# The following variable is optional:
#
# is_guest_manifest: 1 or 0; generates a manifest usable while in guest mode.

{
  'actions': [
    {
      'action_name': 'generate_manifest',
      'message': 'Generate manifest for <(_target_name)',
      'variables': {
        'is_guest_manifest%': 0,
      },
      'inputs': [
        'tools/generate_manifest.py',
        '<(template_manifest_path)',
      ],
      'outputs': [
        '<(output_manifest_path)'
      ],
      'action': [
        'python',
        'tools/generate_manifest.py',
        '-o', '<(output_manifest_path)',
        '-g', '<(is_guest_manifest)',
        '<(template_manifest_path)'
      ],
    },
  ],
}
