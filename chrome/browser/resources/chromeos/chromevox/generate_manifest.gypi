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
# The following variables are optional:
#
# guest_manifest: 1 or 0; generates a manifest usable while in guest
# mode.
# use_chromevox_next: 1 or 0; generates a manifest for ChromeVox next.

{
  'variables': {
    'generate_manifest_script_path': 'tools/generate_manifest.py',
    'is_guest_manifest%': 0,
    'key': 'MIGfMA0GCSqGSIb3DQEBAQUAA4GNADCBiQKBgQDltVl1k15pjRzuZfMc3B69inxwm2bZeZ2O8/zFO+NluHnBm3GJ3fzdOoFGJd+M16I8p7zxxQyHeDMfWYASyCeB8XnUEDKjqNLQfCnncsANzHsYoEbYj2nEUML2P13b9q+AAvpCBpAJ4cZp81e9n1y/vbSXHE4385cgkKueItzikQIDAQAB',
    'use_chromevox_next%': 0,
  },
  'includes': [
    '../../../../../build/util/version.gypi',
  ],
  'actions': [
    {
      'action_name': 'generate_manifest',
      'message': 'Generate manifest for <(_target_name)',
      'inputs': [
        '<(generate_manifest_script_path)',
        '<(template_manifest_path)',
      ],
      'outputs': [
        '<(output_manifest_path)'
      ],
      'action': [
        'python',
        '<(generate_manifest_script_path)',
        '--is_guest_manifest=<(is_guest_manifest)',
        '--key=<(key)',
        '--use_chromevox_next=<(use_chromevox_next)',
        '--set_version=<(version_full)',
        '-o', '<(output_manifest_path)',
        '<(template_manifest_path)',
      ],
    },
  ],
}
