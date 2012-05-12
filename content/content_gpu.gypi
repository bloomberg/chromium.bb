# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'dependencies': [
    '../base/base.gyp:base',
    '../skia/skia.gyp:skia',
    '../ui/gl/gl.gyp:gl',
  ],
  'sources': [
    'gpu/gpu_dx_diagnostics_win.cc',
    'gpu/gpu_info_collector_android.cc',
    'gpu/gpu_info_collector_linux.cc',
    'gpu/gpu_info_collector_mac.mm',
    'gpu/gpu_info_collector_win.cc',
    'gpu/gpu_info_collector.cc',
    'gpu/gpu_info_collector.h',
    'gpu/gpu_main.cc',
    'gpu/gpu_process.cc',
    'gpu/gpu_process.h',
    'gpu/gpu_child_thread.cc',
    'gpu/gpu_child_thread.h',
    'gpu/gpu_watchdog_thread.cc',
    'gpu/gpu_watchdog_thread.h',
  ],
  'include_dirs': [
    '..',
  ],
  'conditions': [
    ['OS=="win"', {
      'include_dirs': [
        '<(DEPTH)/third_party/angle/include',
        '<(DEPTH)/third_party/angle/src',
        '<(DEPTH)/third_party/wtl/include',
        '$(DXSDK_DIR)/include',
      ],
      'dependencies': [
        '../third_party/angle/src/build_angle.gyp:libEGL',
        '../third_party/angle/src/build_angle.gyp:libGLESv2',
      ],
      'link_settings': {
        'libraries': [
          '-lsetupapi.lib',
        ],
      },
    }],
    ['OS=="win" and directxsdk_exists=="True"', {
      'actions': [
        {
          'action_name': 'extract_d3dx9',
          'variables': {
            'input': 'Jun2010_d3dx9_43_x86.cab',
            'output': 'd3dx9_43.dll',
          },
          'inputs': [
            '../third_party/directxsdk/files/Redist/<(input)',
          ],
          'outputs': [
            '<(PRODUCT_DIR)/<(output)',
          ],
          'action': [
            'python',
            '../build/extract_from_cab.py',
            '..\\third_party\\directxsdk\\files\\Redist\\<(input)',
            '<(output)',
            '<(PRODUCT_DIR)',
          ],
        },
        {
          'action_name': 'extract_d3dcompiler',
          'variables': {
            'input': 'Jun2010_D3DCompiler_43_x86.cab',
            'output': 'D3DCompiler_43.dll',
          },
          'inputs': [
            '../third_party/directxsdk/files/Redist/<(input)',
          ],
          'outputs': [
            '<(PRODUCT_DIR)/<(output)',
          ],
          'action': [
            'python',
            '../build/extract_from_cab.py',
            '..\\third_party\\directxsdk\\files\\Redist\\<(input)',
            '<(output)',
            '<(PRODUCT_DIR)',
          ],
        },
      ],
    }],
  ],
}
