# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'conditions': [
    ['OS=="win" and fastbuild==0', {
      # Reorder the initial chrome DLL executable, placing the optimized
      # output and corresponding PDB file into the "syzygy" subdirectory.
      # If there's a matching chrome.dll-ordering.json file present in
      # the output directory, chrome.dll will be ordered according to that,
      # otherwise it will be randomized.
      # This target won't build in fastbuild, since there are no PDBs.
      'targets': [
        {
          'target_name': 'chrome_dll_syzygy',
          'type': 'none',
          'sources' : [],
          'dependencies': [
            '<(DEPTH)/chrome/chrome.gyp:chrome_dll',
          ],
          'variables': {
            'dest_dir': '<(PRODUCT_DIR)\\syzygy',
          },
          'actions': [
            {
              'action_name': 'Reorder Chrome with Syzygy',
              'msvs_cygwin_shell': 0,
              'inputs': [
                '<(PRODUCT_DIR)\\chrome.dll',
                '<(PRODUCT_DIR)\\chrome_dll.pdb',
              ],
              'outputs': [
                '<(dest_dir)\\chrome.dll',
                '<(dest_dir)\\chrome_dll.pdb',
              ],
              'action': [
                'python',
                '<(DEPTH)/chrome/tools/build/win/syzygy_reorder.py',
                '--input_executable', '<(PRODUCT_DIR)\\chrome.dll',
                '--input_symbol', '<(PRODUCT_DIR)\\chrome_dll.pdb',
                '--destination_dir', '<(dest_dir)',
              ],
            },
          ],
        },
      ],
    }],
  ],
}
