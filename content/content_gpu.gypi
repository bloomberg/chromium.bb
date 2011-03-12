# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'directxsdk_exists': '<!(python <(DEPTH)/build/dir_exists.py ../third_party/directxsdk)',
  },  # variables
  'targets': [
    {
      'target_name': 'content_gpu',
      'type': '<(library)',
      'msvs_guid': 'F10F1ECD-D84D-4C33-8468-9DDFE19F4D8A',
      'dependencies': [
        'content_common',
        '../app/app.gyp:app_base',
        '../base/base.gyp:base',
        '../media/media.gyp:media',
        '../skia/skia.gyp:skia',
      ],
      'sources': [
        'gpu/gpu_channel.cc',
        'gpu/gpu_channel.h',
        'gpu/gpu_command_buffer_stub.cc',
        'gpu/gpu_command_buffer_stub.h',
        'gpu/gpu_config.h',
        'gpu/gpu_dx_diagnostics_win.cc',
        'gpu/gpu_info_collector_linux.cc',
        'gpu/gpu_info_collector_mac.mm',
        'gpu/gpu_info_collector_win.cc',
        'gpu/gpu_info_collector.cc',
        'gpu/gpu_info_collector.h',
        'gpu/gpu_main.cc',
        'gpu/gpu_process.cc',
        'gpu/gpu_process.h',
        'gpu/gpu_thread.cc',
        'gpu/gpu_thread.h',
        'gpu/gpu_video_decoder.cc',
        'gpu/gpu_video_decoder.h',
        'gpu/gpu_video_service.cc',
        'gpu/gpu_video_service.h',
        'gpu/gpu_watchdog_thread.cc',
        'gpu/gpu_watchdog_thread.h',
        'gpu/media/gpu_video_device.h',
        'gpu/media/fake_gl_video_decode_engine.cc',
        'gpu/media/fake_gl_video_decode_engine.h',
        'gpu/media/fake_gl_video_device.cc',
        'gpu/media/fake_gl_video_device.h',
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
        }],
        ['OS=="win" and directxsdk_exists=="True"', {
          'actions': [
            {
              'action_name': 'extract_d3dx9',
              'variables': {
                'input': 'Aug2009_d3dx9_42_x86.cab',
                'output': 'd3dx9_42.dll',
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
                'input': 'Aug2009_D3DCompiler_42_x86.cab',
                'output': 'D3DCompiler_42.dll',
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
          'sources': [
            'gpu/media/mft_angle_video_device.cc',
            'gpu/media/mft_angle_video_device.h',
          ],
        }],
        ['OS=="linux" and target_arch!="arm"', {
          'sources': [
            'gpu/x_util.cc',
            'gpu/x_util.h',
          ],
        }],
        ['enable_gpu==1', {
          'dependencies': [
            '../gpu/gpu.gyp:command_buffer_service',
          ],
        }],
      ],
    },
  ],
}
