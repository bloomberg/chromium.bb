# Copyright (c) 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'dependencies': [
    '../base/base.gyp:base',
    '../ipc/ipc.gyp:ipc',
  ],
  'include_dirs': [
    '..',
  ],
  'sources': [
    'ipc/common/memory_stats.cc',
    'ipc/common/memory_stats.h',
  ],
  'conditions': [
    # This section applies to gpu_ipc_win64, used by the NaCl Win64 helper
    # (nacl64.exe).
    ['nacl_win64_target==1', {
      # gpu_ipc_win64 must only link against the 64-bit ipc target.
      'dependencies!': [
        '../ipc/ipc.gyp:ipc',
      ],
    }],
  ],
}
