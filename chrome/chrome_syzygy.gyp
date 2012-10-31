# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'conditions': [
    ['OS=="win" and fastbuild==0', {
      'variables': {
        'conditions': [
          ['incremental_chrome_dll==1', {
            'pdb_file': '<(PRODUCT_DIR)/initial/chrome.dll.pdb',
          }, {
            'pdb_file': '<(PRODUCT_DIR)/chrome.dll.pdb',
          }],
        ],
      },
      'targets': [
        # The PDB is written by a background service, which means writes can
        # happen after the build step which originated the request
        # terminates.  See BUG=126499
        #
        # We hack around this by adding a script which will poll for write
        # access to the PDB as a way to determine if the file is available.
        # On success, we create stamp which is now gauranteed to be newer
        # any inputs the PDB file depends on.
        #
        {
          'target_name': 'wait_chrome_pdb',
          'type': 'none',
          'sources' : [],
          'dependencies': [
            '<(DEPTH)/chrome/chrome.gyp:chrome_dll',
          ],
          'actions': [
            {
              'action_name': 'Wait for PDB access.',
              'msvs_cygwin_shell': 0,
              'inputs': [
                '<(PRODUCT_DIR)/chrome.dll',
              ],
              'outputs': [
                '<(PRODUCT_DIR)/chrome_dll_pdb.stamp',
              ],
              'action': [
                'python',
                '<(DEPTH)/chrome/tools/build/win/wait_for_pdb.py',
                '--input', '<(PRODUCT_DIR)/chrome.dll',
                '--stamp', '<(PRODUCT_DIR)/chrome_dll_pdb.stamp',
                '--pdb', '<(pdb_file)'
              ],
            },
          ],
        },
        # Reorder the initial chrome DLL executable, placing the optimized
        # output and corresponding PDB file into the "syzygy" subdirectory.
        # If there's a matching chrome.dll-ordering.json file present in
        # the output directory, chrome.dll will be ordered according to that,
        # otherwise it will be randomized.
        # This target won't build in fastbuild, since there are no PDBs.
        {
          'target_name': 'chrome_dll_syzygy',
          'type': 'none',
          'sources' : [],
          'dependencies': [
            '<(DEPTH)/chrome/chrome.gyp:chrome_dll',
          ],
          'variables': {
            'dest_dir': '<(PRODUCT_DIR)/syzygy',
          },
          'actions': [
            {
              'action_name': 'Reorder Chrome with Syzygy',
              'msvs_cygwin_shell': 0,
              'inputs': [
                '<(PRODUCT_DIR)/chrome_dll_pdb.stamp',
              ],
              'outputs': [
                '<(dest_dir)/chrome.dll',
                '<(dest_dir)/chrome_dll.pdb',
              ],
              'action': [
                'python',
                '<(DEPTH)/chrome/tools/build/win/syzygy_reorder.py',
                '--input_executable', '<(PRODUCT_DIR)/chrome.dll',
                '--input_symbol', '<(pdb_file)',
                '--destination_dir', '<(dest_dir)',
              ],
            },
          ],
        },
      ],
    }],
  ],
}
