# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'conditions': [
    ['OS=="win" and asan==1', {
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
    # Note, not else.
    ['OS=="win" and fastbuild==0 and chrome_multiple_dll==1 and '
        '(asan!=1 or buildtype!="Official")', {
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
    }, {
      'conditions': [
        ['OS=="win" and fastbuild==0 and chrome_multiple_dll==1 and '
            'asan==1 and buildtype=="Official"', {
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
}
