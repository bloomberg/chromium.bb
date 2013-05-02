# Copyright 2013 the V8 project authors. All rights reserved.
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
#       copyright notice, this list of conditions and the following
#       disclaimer in the documentation and/or other materials provided
#       with the distribution.
#     * Neither the name of Google Inc. nor the names of its
#       contributors may be used to endorse or promote products derived
#       from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

{
  'includes': ['../../build/common.gypi'],
  'targets': [
    {
      'target_name': 'v8',
      'type': 'none',
      'conditions': [
        ['want_separate_host_toolset==1', {
          'toolsets': ['host', 'target'],
        }, {
          'toolsets': ['target'],
        }],
      ],
      'variables': {
        'shim_headers_path': '<(SHARED_INTERMEDIATE_DIR)/shim_headers/<(_target_name)/<(_toolset)',
      },
      'include_dirs++': [
        '<(shim_headers_path)',
      ],
      'all_dependent_settings': {
        'include_dirs+++': [
          '<(shim_headers_path)',
        ],
      },
      'actions': [
        {
          'variables': {
            'generator_path': '../../../tools/generate_shim_headers/generate_shim_headers.py',
            'generator_args': [
              '--headers-root', '../../include',
              '--output-directory', '<(shim_headers_path)',
              'v8-debug.h',
              'v8-preparser.h',
              'v8-profiler.h',
              'v8-testing.h',
              'v8.h',
              'v8stdint.h',
            ],
          },
          'action_name': 'generate_<(_target_name)_shim_headers',
          'inputs': [
            '<(generator_path)',
          ],
          'outputs': [
            '<!@pymod_do_main(generate_shim_headers <@(generator_args) --outputs)',
          ],
          'action': ['python',
                     '<(generator_path)',
                     '<@(generator_args)',
                     '--generate',
          ],
          'message': 'Generating <(_target_name) shim headers.',
        },
      ],
      'link_settings': {
        'libraries': [
          '-lv8',
        ],
      },
    },
    {
      'target_name': 'v8_shell',
      'type': 'none',
      'conditions': [
        ['want_separate_host_toolset==1', {
          'toolsets': ['host'],
        }, {
          'toolsets': ['target'],
        }],
      ],
      'dependencies': [
        'v8'
      ],
    },
  ],
}
