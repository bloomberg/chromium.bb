# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # TODO(calamity): Remove once print_preview no longer depends on this.
      'target_name': 'pdf_scripting_api',
      'dependencies': [
        '<(DEPTH)/ui/webui/resources/js/compiled_resources2.gyp:assert',
      ],
      'includes': ['../../../../third_party/closure_compiler/compile_js2.gypi'],
    },
  ],
}
