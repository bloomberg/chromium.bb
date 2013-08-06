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
        '<(DEPTH)/third_party/khronos',
        '<(DEPTH)/third_party/angle_dx11/src',
        '<(DEPTH)/third_party/wtl/include',
      ],
      'dependencies': [
        '../third_party/angle_dx11/src/build_angle.gyp:libEGL',
        '../third_party/angle_dx11/src/build_angle.gyp:libGLESv2',
      ],
      'link_settings': {
        'libraries': [
          '-lsetupapi.lib',
        ],
      },
      'copies': [
        {
          'destination': '<(PRODUCT_DIR)',
          'files': [
            '<(windows_sdk_path)/Redist/D3D/<(winsdk_arch)/d3dcompiler_46.dll',
          ],
        },
      ],
    }],
    ['OS=="win" and target_arch=="ia32" and directxsdk_exists=="True"', {
      # We don't support x64 prior to Win7 and D3DCompiler_43.dll is
      # not needed on Vista+.
      'actions': [
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
          'msvs_cygwin_shell': 1,
        },
      ],
    }],
    ['target_arch!="arm" and chromeos == 1', {
      'include_dirs': [
        '<(DEPTH)/third_party/libva',
      ],
    }],
  ],
}
