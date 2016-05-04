# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //components/exo
      'target_name': 'exo',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        '../ash/ash.gyp:ash',
        '../ash/wm/common/ash_wm_common.gyp:ash_wm_common',
        '../base/base.gyp:base',
        '../cc/cc.gyp:cc',
        '../gpu/gpu.gyp:gpu',
        '../skia/skia.gyp:skia',
        '../ui/aura/aura.gyp:aura',
        '../ui/compositor/compositor.gyp:compositor',
        '../ui/gfx/gfx.gyp:gfx',
        '../ui/gfx/gfx.gyp:gfx_geometry',
        '../ui/gl/gl.gyp:gl',
        '../ui/views/views.gyp:views',
        '../ui/wm/wm.gyp:wm',
      ],
      'export_dependent_settings': [
        '../ui/views/views.gyp:views',
      ],
      'sources': [
        # Note: sources list duplicated in GN build.
        'exo/buffer.cc',
        'exo/buffer.h',
        'exo/display.cc',
        'exo/display.h',
        'exo/keyboard.cc',
        'exo/keyboard.h',
        'exo/keyboard_delegate.h',
        'exo/pointer.cc',
        'exo/pointer.h',
        'exo/pointer_delegate.h',
        'exo/shared_memory.cc',
        'exo/shared_memory.h',
        'exo/shell_surface.cc',
        'exo/shell_surface.h',
        'exo/sub_surface.cc',
        'exo/sub_surface.h',
        'exo/surface.cc',
        'exo/surface.h',
        'exo/surface_delegate.h',
        'exo/surface_observer.h',
        'exo/touch.cc',
        'exo/touch.h',
        'exo/touch_delegate.h',
      ],
    },
  ],
  'conditions': [
    [ 'OS=="linux"', {
      'targets': [
        {
          # GN version: //components/exo:wayland
          'target_name': 'exo_wayland',
          'type': 'static_library',
          'include_dirs': [
            '..',
          ],
          'dependencies': [
            '../base/base.gyp:base',
            '../ipc/ipc.gyp:ipc',
            '../skia/skia.gyp:skia',
            '../third_party/wayland-protocols/wayland-protocols.gyp:scaler_protocol',
            '../third_party/wayland-protocols/wayland-protocols.gyp:secure_output_protocol',
            '../third_party/wayland-protocols/wayland-protocols.gyp:xdg_shell_protocol',
            '../third_party/wayland/wayland.gyp:wayland_server',
            '../ui/events/events.gyp:dom_keycode_converter',
            '../ui/display/display.gyp:display',
            '../ui/events/events.gyp:events_base',
            '../ui/views/views.gyp:views',
            'exo',
          ],
          'sources': [
            # Note: sources list duplicated in GN build.
            'exo/wayland/scoped_wl.cc',
            'exo/wayland/scoped_wl.h',
            'exo/wayland/server.cc',
            'exo/wayland/server.h',
          ],
          'conditions': [
            ['use_ozone==1', {
              'dependencies': [
                '../build/linux/system.gyp:libdrm',
                '../third_party/mesa/mesa.gyp:wayland_drm_protocol',
                '../third_party/wayland-protocols/wayland-protocols.gyp:linux_dmabuf_protocol',
              ],
            }],
            ['use_xkbcommon==1', {
              'dependencies': [
                '../build/linux/system.gyp:xkbcommon',
              ],
              'defines': [
                'USE_XKBCOMMON',
              ],
            }],
          ],
        },
      ],
    }],
  ],
}
