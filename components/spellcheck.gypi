# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //components/spellcheck/common
      'target_name': 'spellcheck_common',
      'type': 'static_library',
      'dependencies': [
        '../third_party/icu/icu.gyp:icui18n',
        '../third_party/icu/icu.gyp:icuuc',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'spellcheck/common/spellcheck_bdict_language.h',
        'spellcheck/common/spellcheck_common.cc',
        'spellcheck/common/spellcheck_common.h',
        'spellcheck/common/spellcheck_marker.h',
        'spellcheck/common/spellcheck_message_generator.cc',
        'spellcheck/common/spellcheck_message_generator.h',
        'spellcheck/common/spellcheck_messages.h',
        'spellcheck/common/spellcheck_result.h',
        'spellcheck/common/spellcheck_switches.cc',
        'spellcheck/common/spellcheck_switches.h',
      ],
    },
    {
      # GN version: //components/spellcheck/renderer
      'target_name': 'spellcheck_renderer',
      'type': 'static_library',
      'dependencies': [
        'spellcheck_common',
        '../third_party/icu/icu.gyp:icui18n',
        '../third_party/icu/icu.gyp:icuuc',
        '../third_party/WebKit/public/blink.gyp:blink',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'spellcheck/renderer/custom_dictionary_engine.cc',
        'spellcheck/renderer/custom_dictionary_engine.h',
        'spellcheck/renderer/hunspell_engine.cc',
        'spellcheck/renderer/hunspell_engine.h',
        'spellcheck/renderer/platform_spelling_engine.cc',
        'spellcheck/renderer/platform_spelling_engine.h',
        'spellcheck/renderer/spellcheck.cc',
        'spellcheck/renderer/spellcheck.h',
        'spellcheck/renderer/spellcheck_language.cc',
        'spellcheck/renderer/spellcheck_language.h',
        'spellcheck/renderer/spellcheck_provider.cc',
        'spellcheck/renderer/spellcheck_provider.h',
        'spellcheck/renderer/spellcheck_worditerator.cc',
        'spellcheck/renderer/spellcheck_worditerator.h',
        'spellcheck/renderer/spelling_engine.h',
      ],
      'conditions': [
        ['OS=="android"', {
          'sources!': [
            'spellcheck/hunspell_engine.cc',
            'spellcheck/hunspell_engine.h',
          ]
        }],
        ['OS!="android"', {
          'dependencies': [
            '../third_party/hunspell/hunspell.gyp:hunspell',
          ],
        }],
        ['use_browser_spellchecker==0', {
          'sources!': [
            'spellcheck/platform_spelling_engine.cc',
            'spellcheck/platform_spelling_engine.h',
          ]
        }],
      ],
    },
  ],
}
