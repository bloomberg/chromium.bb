# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //components/url_formatter
      'target_name': 'url_formatter',
      'type': 'static_library',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../net/net.gyp:net',
        '../../third_party/icu/icu.gyp:icui18n',
        '../../third_party/icu/icu.gyp:icuuc',
        '../../url/url.gyp:url_lib',
      ],
      'sources': [
        # Note: sources list duplicated in GN build.
        'android/component_jni_registrar.cc',
        'android/component_jni_registrar.h',
        'elide_url.cc',
        'elide_url.h',
        'url_fixer.cc',
        'url_fixer.h',
        'url_formatter.cc',
        'url_formatter.h',
        'url_formatter_android.cc',
        'url_formatter_android.h',
      ],
      # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
      'msvs_disabled_warnings': [4267, ],

      'conditions': [
        ['OS != "android"', {
          'dependencies': [
            '../../ui/gfx/gfx.gyp:gfx',
          ],
          'sources!': [
            'android/component_jni_registrar.cc',
            'android/component_jni_registrar.h',
            'url_formatter_android.cc',
            'url_formatter_android.h',
          ]
        }],
        ['OS == "android"', {
          'dependencies': [
            'url_formatter_jni_headers',
          ]
        }],
      ],
    },
  ],
  'conditions': [
    ['OS == "android"', {
      'targets' : [
        {
          # GN: //components/url_formatter/android:jni_headers
          'target_name' : 'url_formatter_jni_headers',
          'type': 'none',
          'sources': [
            'android/java/src/org/chromium/components/url_formatter/UrlFormatter.java',
           ],
          'variables': {
            'jni_gen_package': 'url_formatter',
           },
          'includes': [ '../../build/jni_generator.gypi' ],
        },
        {
          # GN: //components/url_formatter/android:url_formatter_java
          'target_name': 'url_formatter_java',
          'type': 'none',
          'dependencies': [
            '../../base/base.gyp:base_java',
          ],
          'variables': {
            'java_in_dir': 'android/java',
          },
          'includes': [ '../../build/java.gypi' ],
        },
       ],
    }],
  ],
}
