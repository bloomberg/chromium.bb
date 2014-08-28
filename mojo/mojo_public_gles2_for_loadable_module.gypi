# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# In the non component build, the thunks need to be linked directly into the
# loadable module since they define symbols that should be exported from that
# library. So, this variable expands out to either list the sources directly (in
# the component build where no symbols need to be exported) a dependency.
{
  'conditions': [
    ['component=="shared_library"', {
      'dependencies': [
        'mojo_base.gyp:mojo_gles2_impl',
      ],
    }, {  # component!="shared_library"
      'defines': [
        'MOJO_GLES2_IMPLEMENTATION',
        'GLES2_USE_MOJO',
      ],
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        '../third_party/khronos/khronos.gyp:khronos_headers'
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '..',
        ],
        'defines': [
          'GLES2_USE_MOJO',
        ],
      },
      'all_dependent_settings': {
        'conditions': [
          # We need to be able to call the MojoSetGLES2Thunks() function in
          # gles2_thunks.cc
          ['OS=="android"', {
            'ldflags!': [
              '-Wl,--exclude-libs=ALL',
            ],
          }],
        ],
      },
      'sources': [
        'public/c/gles2/gles2.h',
        'public/c/gles2/gles2_export.h',
        'public/platform/native/gles2_thunks.cc',
        'public/platform/native/gles2_thunks.h',
        'public/platform/native/gles2_impl_thunks.cc',
        'public/platform/native/gles2_impl_thunks.h',
        'public/platform/native/gles2_impl_chromium_texture_mailbox_thunks.cc',
        'public/platform/native/gles2_impl_chromium_texture_mailbox_thunks.h',
        'public/platform/native/gles2_impl_chromium_sync_point_thunks.cc',
        'public/platform/native/gles2_impl_chromium_sync_point_thunks.h',
      ],
    }]
  ]
}
