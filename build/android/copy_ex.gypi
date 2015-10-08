# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Copy files to a directory with the option to clear directory first.
#
# Variables:
#   dest_path - directory to copy files to.
#   src_files - a list of files to copy.
#   clear - optional, if set, clear directory before copying files.
#   stamp - optional, path to touch on success.
#
# Exmaple
#  {
#    'target_name': 'copy_assets',
#    'type': 'none',
#    'variables': {
#      'dest_path': 'apk/assets/path',
#      'src_files': ['path1/fr.pak'],
#      'clear': 1,
#    },
#    'includes': [ '../build/android/copy_ex.gypi' ],
#  },
#
{
  'variables': {
    'clear%': 0,
    'stamp%': '',
  },
  'actions': [{
    'action_name': '<(_target_name)_copy_ex',
    'variables': {
      'additional_args':[],
      'conditions': [
        ['clear == 1', {
          'additional_args': ['--clear'],
        }],
        ['stamp != ""', {
          'additional_args': ['--stamp', '<(stamp)'],
        }],
      ],
    },
    'inputs': [
      '<(DEPTH)/build/android/gyp/copy_ex.py',
      '<(DEPTH)/build/android/gyp/generate_copy_ex_outputs.py',
      '<@(src_files)',
    ],
    'outputs': [
      '<(stamp)',
      '<!@pymod_do_main(generate_copy_ex_outputs --dest-path <(dest_path) --src-files <(src_files))'
    ],
    'action': [
      'python', '<(DEPTH)/build/android/gyp/copy_ex.py',
      '--dest', '<(dest_path)',
      '--files', '<(src_files)',
      '<(additional_args)',
    ],
  }],
}
