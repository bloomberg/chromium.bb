# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    # This is the app shim on Windows. It is a small binary that simply runs
    # Chrome, passing along any command-line arguments.
    {
      'target_name': 'app_shim',
      'type': 'executable',
      'dependencies': [
        'launcher_support',
        '../base/base.gyp:base',
      ],
      'sources': [
        'win/app_shim.rc',
        'win/app_shim_main.cc',
      ],
      'msvs_settings': {
        'VCLinkerTool': {
          'SubSystem': '2',     # Set /SUBSYSTEM:WINDOWS
        },
        'VCManifestTool': {
          'AdditionalManifestFiles': [
            'app_shim/win/app_shim.exe.manifest',
          ],
        },
      },
    },
  ],  # targets
}
