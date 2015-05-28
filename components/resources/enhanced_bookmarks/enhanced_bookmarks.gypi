# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets' :[
    {
      'target_name': 'document_image_extractor',
      'type': 'none',
      'variables': { 'CLOSURE_DIR': '<(DEPTH)/third_party/closure_compiler', },
      'actions': [
        {
          # This action optionally takes these arguments:
          # - depends: scripts that the source file depends on being included already
          # - externs: files that describe globals used by |source|
          'action_name': 'Compile enhanced bookmarks image extractor JavaScript',
          'variables': {
            'source_file': 'get_salient_image_url.js',
            'out_file': '<(SHARED_INTERMEDIATE_DIR)/closure/<!(python <(CLOSURE_DIR)/build/outputs.py <@(source_file))',
          },
          'inputs': [
            '<(CLOSURE_DIR)/compile.py',
            '<(CLOSURE_DIR)/processor.py',
            '<(CLOSURE_DIR)/build/inputs.py',
            '<(CLOSURE_DIR)/build/outputs.py',
            '<(CLOSURE_DIR)/compiler/compiler.jar',
            '<(CLOSURE_DIR)/runner/runner.jar',
            '<!@(python <(CLOSURE_DIR)/build/inputs.py <(source_file))',
          ],
          'outputs': [
            '<(out_file)',
          ],
          'action': [
            'python',
            '<(CLOSURE_DIR)/compile.py',
            '<@(document_image_extractor_js_files)',
            '<(source_file)',
            '--out_file', '<(out_file)',
            '--no-single-file',
            '--output_wrapper', '(function(){%output% return GetSalientImageUrl();})();',
          ],
        }
      ],
      'includes': [
        '../../../third_party/document_image_extractor/document_image_extractor_files.gypi'
      ],
    },
    {
      'target_name': 'dom_initializer',
      'type': 'none',
      'variables': { 'CLOSURE_DIR': '<(DEPTH)/third_party/closure_compiler', },
      'actions': [
        {
          # This action optionally takes these arguments:
          # - depends: scripts that the source file depends on being included already
          # - externs: files that describe globals used by |source|
          'action_name': 'Compile enhanced bookmarks dom initializer JavaScript',
          'variables': {
            'source_file': 'dom_initializer.js',
            'out_file': '<(SHARED_INTERMEDIATE_DIR)/closure/<!(python <(CLOSURE_DIR)/build/outputs.py <@(source_file))',
          },
          'inputs': [
            '<(CLOSURE_DIR)/compile.py',
            '<(CLOSURE_DIR)/processor.py',
            '<(CLOSURE_DIR)/build/inputs.py',
            '<(CLOSURE_DIR)/build/outputs.py',
            '<(CLOSURE_DIR)/compiler/compiler.jar',
            '<(CLOSURE_DIR)/runner/runner.jar',
            '<!@(python <(CLOSURE_DIR)/build/inputs.py <(source_file))',
          ],
          'outputs': [
            '<(out_file)',
          ],
          'action': [
            'python',
            '<(CLOSURE_DIR)/compile.py',
            '<@(dom_controller_js_files)',
            '<(source_file)',
            '--out_file', '<(out_file)',
            '--no-single-file',
            '--output_wrapper', '(function(){%output%})();',
          ]
        }
      ],
      'includes': [
        '../../../third_party/document_image_extractor/dom_controller_files.gypi'
      ],
    },
  ],
}
