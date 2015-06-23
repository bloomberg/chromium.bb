# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'get_salient_image_url',
      'variables': {
        'source_files': [
          'get_salient_image_url.js',
          '<@(document_image_extractor_js_files)',
        ],
        'script_args': ['--no-single-file'],
        'closure_args': ['output_wrapper=\'(function(){%output% return GetSalientImageUrl();})();\''],
      },
      'includes': [
        '../../../third_party/document_image_extractor/document_image_extractor_files.gypi',
        '../../../third_party/closure_compiler/compile_js.gypi',
      ],
    },
    {
      'target_name': 'dom_initializer',
      'variables': {
        'source_files': [
          'dom_initializer.js',
          '<@(dom_controller_js_files)',
        ],
        'script_args': ['--no-single-file'],
        'closure_args': ['output_wrapper=\'(function(){%output%})();\''],
      },
      'includes': [
        '../../../third_party/document_image_extractor/dom_controller_files.gypi',
        '../../../third_party/closure_compiler/compile_js.gypi',
      ],
    },
  ],
}
