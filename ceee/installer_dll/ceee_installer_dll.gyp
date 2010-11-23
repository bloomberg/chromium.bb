# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'includes': [
    '../../build/common.gypi',
    '../../ceee/common.gypi',
  ],
  'targets': [
    {
      'target_name': 'ceee_installer_helper',
      'type': 'shared_library',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../chrome/chrome.gyp:common',
        '../../ceee/common/common.gyp:ceee_common',
        '../../ceee/common/common.gyp:initializing_coclass',
        '../../ceee/ie/common/common.gyp:ie_common',
        '../firefox/firefox.gyp:xpi',
        '<(DEPTH)/chrome/chrome.gyp:chrome_version_header',
        '<(DEPTH)/chrome/chrome.gyp:installer_util',
      ],
      'include_dirs': [
        '<(INTERMEDIATE_DIR)',
        # For version.h
        '<(SHARED_INTERMEDIATE_DIR)',
      ],
      'sources': [
        'installer_helper.cc',
        'installer_helper.def',
        'installer_helper.h',
        'installer_helper.rc',
        'resource.h',
        '$(OutDir)/ceee_ff.xpi',
        # TODO(joi@chromium.org) Load this in a nicer way.
        '../../chrome/installer/mini_installer/pe_resource.cc',
        '../../chrome/installer/mini_installer/pe_resource.h',
      ],
      'msvs_settings': {
        'VCLinkerTool': {
          'OutputFile': '$(OutDir)/servers/$(ProjectName).dll',
         },
      },
      'actions': [
      {
          'action_name': 'generate_packing_list',
          'msvs_cygwin_shell': 0,
          'inputs': [
            'generate_packing_list.py',
            '$(OutDir)/ceee_ff.xpi',
          ],
          'outputs': [
            '<(INTERMEDIATE_DIR)/ceee_installer_packing_list.txt',
          ],
          'action': [
            '<@(python)',
            'generate_packing_list.py',
            '>(_outputs)',  # output file
            '$(OutDir)/ceee_ff.xpi',  # one or more packed files
          ],
        },
      ],
    },
  ],
}
