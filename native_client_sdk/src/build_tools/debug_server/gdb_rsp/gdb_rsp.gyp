# -*- python -*-
# Copyright 2009, Google Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
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
  'includes': [
    '../../../build/common.gypi',
  ],
  'target_defaults': {
    'conditions': [
      ['target_arch=="ia32"', {
        'defines': [
          'GDB_RSP_ABI_X86',
        ],
      }],
      ['target_arch=="x64"', {
        'defines': [
          'GDB_RSP_ABI_X86',
          'GDB_RSP_ABI_X86_64',
        ],
      }],
      ['target_arch=="arm"', {
        'defines': [
          'GDB_RSP_ABI_ARM',
        ],
      }],
    ],
    'target_conditions': [
      ['OS=="linux" or OS=="mac"', {
        'cflags': [
          '-fexceptions',
        ],
        'cflags_cc' : [
          '-frtti',
        ]
      }],
      ['OS=="mac"', {
        'xcode_settings': {
          'GCC_ENABLE_CPP_EXCEPTIONS': 'YES',      # -fexceptions
          'GCC_ENABLE_CPP_RTTI': 'YES',            # -frtti
        }
      }],
      ['OS=="win"', {
        'msvs_settings': {
          'VCCLCompilerTool': {
            'ExceptionHandling': '1',
            'RuntimeTypeInfo': 'true',
          },
        }
      }],
    ],
  },
  'variables': {
    'gdb_rsp_sources': [
        'abi.cc',
        'abi.h',
        'host.cc',
        'host.h',
        'packet.cc',
        'packet.h',
        'session.cc',
        'session.h',
        'target.cc',
        'target.h',
        'util.cc',
        'util.h',
    ],
    'gdb_test_sources': [
        'abi_test.cc',
        'packet_test.cc',
        'host_test.cc',
        'session_mock.h',
        'session_mock.cc',
        'session_test.cc',
        'target_test.cc',
        'util_test.cc',
        'test.cc',
        'test.h',
    ],
  },
  'targets': [
    {
      'target_name': 'gdb_rsp',
      'type': 'static_library',
        'sources': [
          '<@(gdb_rsp_sources)',
        ],
    },
    {
      'target_name': 'gdb_rsp_test',
      'type': 'executable',
        'sources': [
          '<@(gdb_test_sources)',
        ],
      'dependencies': [
        'gdb_rsp',
      ]
    },
  ],
  'conditions': [
    ['OS=="win"', {
      'targets': [
        {
          'target_name': 'gdb_rsp64',
          'type': 'static_library',
            'sources': [
              '<@(gdb_rsp_sources)',
            ],
          'defines': [
            'GDB_RSP_ABI_X86_64',
          ],
          'configurations': {
            'Common_Base': {
              'msvs_target_platform': 'x64',
            },
          },
        },
        {
          'target_name': 'gdb_rsp_test64',
          'type': 'executable',
            'sources': [
              '<@(gdb_test_sources)',
            ],
          'configurations': {
            'Common_Base': {
              'msvs_target_platform': 'x64',
            },
          },
          'dependencies': [
            'gdb_rsp64',
          ]
        },
      ],
    }],
  ],
}
