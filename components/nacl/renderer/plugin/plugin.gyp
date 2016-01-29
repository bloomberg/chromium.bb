# -*- python -*-
# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    'plugin.gypi',
  ],
  'targets': [
    {
      'target_name': 'nacl_trusted_plugin',
      'type': 'static_library',
      'sources': [
        '<@(common_sources)',
      ],
      'dependencies': [
        '<(DEPTH)/media/media.gyp:shared_memory_support',
        '<(DEPTH)/ppapi/ppapi.gyp:ppapi_cpp_objects',
        '<(DEPTH)/ppapi/ppapi.gyp:ppapi_internal_module',
      ],
      # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
      'msvs_disabled_warnings': [4267, ],
    },
  ],
}
