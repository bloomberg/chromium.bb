# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../../build/common.gypi',
    '../../ceee/common.gypi',
    'xpi_file_list.gypi',
  ],
  'variables': {
    'version_path': '<(DEPTH)/chrome/VERSION',
  },
  'conditions': [
    [ 'branding == "Chrome"', {
      'variables': {
        'branding_path':
            '<(DEPTH)/chrome/app/theme/google_chrome/BRANDING',
      },
    }, { # else branding!="Chrome"
      'variables': {
        'branding_path':
            '<(DEPTH)/chrome/app/theme/chromium/BRANDING',
      },
    }],
  ],
  'targets': [
    {
      'target_name': 'xpi',
      'type': 'none',
      'msvs_guid': 'C697F5CD-04EB-4260-95D4-E051258809B2',
      'dependencies': [
        'create_xpi',
        '../ie/plugin/scripting/scripting.gyp:javascript_bindings',
      ],
      'sources': [
        'zipfiles.py',
        '<(SHARED_INTERMEDIATE_DIR)/ceee_ff.xpi',
        '<(SHARED_INTERMEDIATE_DIR)/event_bindings.js',
        '<(SHARED_INTERMEDIATE_DIR)/renderer_extension_bindings.js',
        '<(version_path)',
        '<(branding_path)',
      ],
      'actions': [
        {
          'action_name': 'append_xpi',
          'msvs_cygwin_shell': 0,
          'inputs': [
            '<@(_sources)',
          ],
          'outputs': [
            '$(OutDir)/ceee_ff.xpi',
          ],
          'action': [
            '<@(python)',
            '$(ProjectDir)\zipfiles.py',
            '--config',
            '$(ConfigurationName)',
            '--input',
            '<(SHARED_INTERMEDIATE_DIR)/ceee_ff.xpi',
            '--version',
            '<(version_path)',
            '--branding',
            '<(branding_path)',
            '--ignore_extensions',
            '<(xpi_no_expand_extensions)',
            '--output',
            '<(_outputs)',
            '--path',
            'content/us',
            '<(SHARED_INTERMEDIATE_DIR)/event_bindings.js',
            '<(SHARED_INTERMEDIATE_DIR)/renderer_extension_bindings.js',
          ],
        },
      ],
    },
    {
      'target_name': 'create_xpi',
      'type': 'none',
      'sources': [
        'zipfiles.py',
        '<@(_xpi_files)',
        '<@(_xpi_test_files)',
        '<(version_path)',
        '<(branding_path)',
      ],
      'actions': [
        {
          'action_name': 'create_xpi',
          'msvs_cygwin_shell': 0,
          'inputs': [
            '<@(_sources)',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/ceee_ff.xpi',
          ],
          'action': [
            # Because gyp automatically wraps all strings below in quotes,
            # we need to keep command line arguments separate so that they
            # don't get merged together in one quoted string.  Below, we want
            # -o and >(_outputs) to be seen as two command line arguments, not
            # one of the form "-o >(_outputs)".
            '<@(python)',
            '$(ProjectDir)\zipfiles.py',
            '--config',
            '$(ConfigurationName)',
            '--version',
            '<(version_path)',
            '--branding',
            '<(branding_path)',
            '--ignore_extensions',
            '<(xpi_no_expand_extensions)',
            '--output',
            '<(_outputs)',
            '<@(_xpi_files)',
            '##',
            '<@(_xpi_test_files)',  # Included only in Debug build config.
          ],
        },
      ],
    },
  ],
}
