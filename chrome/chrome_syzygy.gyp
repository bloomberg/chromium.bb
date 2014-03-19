# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'conditions': [
    ['syzyasan==1', {
      'variables': {
        'dest_dir': '<(PRODUCT_DIR)/syzygy',
        'syzygy_exe_dir': '<(DEPTH)/third_party/syzygy/binaries/exe',
      },
      # Copy the SyzyASan runtime and logger to the syzygy directory.
      'targets': [
        {
          'target_name': 'copy_syzyasan_binaries',
          'type': 'none',
          'outputs': [
            '<(dest_dir)/agent_logger.exe',
            '<(dest_dir)/syzyasan_rtl.dll',
            '<(dest_dir)/syzyasan_rtl.dll.pdb',
          ],
          'copies': [
            {
              'destination': '<(dest_dir)',
              'files': [
                '<(syzygy_exe_dir)/agent_logger.exe',
                '<(syzygy_exe_dir)/syzyasan_rtl.dll',
                '<(syzygy_exe_dir)/syzyasan_rtl.dll.pdb',
              ],
            },
          ],
        },
      ],
    }],
    ['OS=="win" and fastbuild==0', {
      'conditions': [
        ['syzygy_optimize==1 or syzyasan==1', {
          'variables': {
            'dll_name': 'chrome',
          },
          'targets': [
            {
              'target_name': 'chrome_dll_syzygy',
              'type': 'none',
              'sources' : [],
              'includes': [
                'chrome_syzygy.gypi',
              ],
            },
          ],
        }],
        ['chrome_multiple_dll==1', {
          'conditions': [
            # Prevent chrome_child.dll from being syzyasan-instrumented for the
            # official builds.
            #
            # Here's the truth table for this logic:
            # +----------+-----------------+-----------+--------------+
            # | syzyasan | syzygy_optimize | buildtype | instrument ? |
            # +----------+-----------------+-----------+--------------+
            # |    0     |        0        |    any    |      N       |
            # +----------+-----------------+-----------+--------------+
            # |    0     |        1        |    any    |      Y       |
            # +----------+-----------------+-----------+--------------+
            # |    1     |        0        |     -     |      Y       |
            # +----------+-----------------+-----------+--------------+
            # |    1     |        0        |  Official |      N       |
            # +----------+-----------------+-----------+--------------+
            # |    1     |        1        |     ----Invalid----      |
            # +----------+-----------------+-----------+--------------+
            ['(syzyasan==1 and buildtype!="Official") or syzygy_optimize==1', {
              'variables': {
                'dll_name': 'chrome_child',
              },
              'targets': [
                {
                  'target_name': 'chrome_child_dll_syzygy',
                  'type': 'none',
                  'sources' : [],
                  'includes': [
                    'chrome_syzygy.gypi',
                  ],
                },
              ],
            }],
            # For the official SyzyASan builds just copy chrome_child.dll to the
            # Syzygy directory.
            ['syzyasan==1 and buildtype=="Official"', {
              'targets': [
              {
                'target_name': 'chrome_child_dll_syzygy',
                'type': 'none',
                'inputs': [
                  '<(PRODUCT_DIR)/chrome_child.dll',
                  '<(PRODUCT_DIR)/chrome_child.dll.pdb',
                ],
                'outputs': [
                  '<(PRODUCT_DIR)/syzygy/chrome_child.dll',
                  '<(PRODUCT_DIR)/syzygy/chrome_child.dll.pdb',
                ],
                'copies': [
                  {
                    'destination': '<(PRODUCT_DIR)/syzygy',
                    'files': [
                      '<(PRODUCT_DIR)/chrome_child.dll',
                      '<(PRODUCT_DIR)/chrome_child.dll.pdb',
                    ],
                  },
                ],
              }],
            }],
          ],
        }],
      ],
    }],
  ],
}
