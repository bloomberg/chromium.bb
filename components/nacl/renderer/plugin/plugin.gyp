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
        '<(DEPTH)/native_client/src/shared/gio/gio.gyp:gio',
        '<(DEPTH)/native_client/src/shared/imc/imc.gyp:imc',
        '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform',
        '<(DEPTH)/native_client/src/trusted/desc/desc.gyp:nrd_xfer',
        '<(DEPTH)/native_client/src/trusted/platform_qualify/platform_qualify.gyp:platform_qual_lib',
        '<(DEPTH)/native_client/src/trusted/simple_service/simple_service.gyp:simple_service',
        '<(DEPTH)/ppapi/ppapi.gyp:ppapi_cpp_objects',
        '<(DEPTH)/ppapi/ppapi.gyp:ppapi_internal_module',
      ],
      # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
      'msvs_disabled_warnings': [4267, ],
    },
  ],
}
