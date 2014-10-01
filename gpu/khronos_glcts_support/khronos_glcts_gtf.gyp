# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    'khronos_glcts.gypi',
  ],
  'targets': [
    {
      'target_name': 'gtf_es',
      'type': 'static_library',
      'dependencies': [
        '<(DEPTH)/gpu/khronos_glcts_support/khronos_glcts_framework.gyp:debase',
        '<(DEPTH)/gpu/khronos_glcts_support/khronos_glcts_cts.gyp:glcts_gtf_wrapper',
        '<(DEPTH)/third_party/expat/expat.gyp:expat',
      ],
      'include_dirs': [
        '<(DEPTH)/third_party/khronos_glcts/GTF_ES/glsl/GTF/Source',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(DEPTH)/third_party/khronos_glcts/GTF_ES/glsl/GTF/Source',
        ],
      },
      'sources': [
        '<@(gtf_core_srcs)',
        '<@(gtf_gl_core_srcs)',
        '<@(gtf_gles2_srcs)',
        '<@(gtf_gles2_es_only_srcs)',
      ],
    },
  ],
}
