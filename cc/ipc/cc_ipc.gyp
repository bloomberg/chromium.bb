# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
     {
      # GN version: //cc/ipc
      'target_name': 'cc_ipc',
      'type': '<(component)',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../cc/cc.gyp:cc',
        '../../gpu/gpu.gyp:gpu_ipc_common',
        '../../ipc/ipc.gyp:ipc',
        '../../skia/skia.gyp:skia',
        '../../ui/events/events.gyp:events_base',
        '../../ui/events/events.gyp:events_ipc',
        '../../ui/gfx/gfx.gyp:gfx',
        '../../ui/gfx/gfx.gyp:gfx_geometry',
        '../../ui/gfx/ipc/gfx_ipc.gyp:gfx_ipc',
        '../../ui/gfx/ipc/geometry/gfx_ipc_geometry.gyp:gfx_ipc_geometry',
        '../../ui/gfx/ipc/skia/gfx_ipc_skia.gyp:gfx_ipc_skia',
      ],
      'defines': [
        'CC_IPC_IMPLEMENTATION',
      ],
      'sources': [
        'cc_param_traits.cc',
        'cc_param_traits.h',
        'cc_param_traits_macros.h',
      ],
    },
  ],
}
