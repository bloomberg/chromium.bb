# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'translate_common',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../url/url.gyp:url_lib',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'translate/common/translate_constants.cc',
        'translate/common/translate_constants.h',
        'translate/common/translate_metrics.cc',
        'translate/common/translate_metrics.h',
        'translate/common/translate_switches.cc',
        'translate/common/translate_switches.h',
        'translate/common/translate_util.cc',
        'translate/common/translate_util.h',
      ],
    },
    {
      'target_name': 'translate_language_detection',
      'type': 'static_library',
      'dependencies': [
        'translate_common',
        '../base/base.gyp:base',
        '../url/url.gyp:url_lib',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'translate/language_detection/language_detection_util.cc',
        'translate/language_detection/language_detection_util.h',
      ],
      'conditions': [
        ['cld_version==0 or cld_version==1', {
          'dependencies': [
            '<(DEPTH)/third_party/cld/cld.gyp:cld',
          ],
        }],
        ['cld_version==0 or cld_version==2', {
          'dependencies': [
            '<(DEPTH)/third_party/cld_2/cld_2.gyp:cld_2',
          ],
        }],
      ],
    },
  ],
}
