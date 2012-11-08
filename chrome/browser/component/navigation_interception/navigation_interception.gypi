# Copyright (c) 2012 The Chromium Authors. All rights reserved.
#
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'navigation_interception',
      'type': 'static_library',
      'dependencies': [
        '../../../base/base.gyp:base',
        '../../../content/content.gyp:content_browser',
        '../../../content/content.gyp:content_common',
        '../../../net/net.gyp:net',
      ],
      'include_dirs': [
        '../../../..',
        '../../../../skia/config',
        '<(SHARED_INTERMEDIATE_DIR)/chrome/browser_component',

      ],
      'sources': [
        'intercept_navigation_resource_throttle.cc',
        'intercept_navigation_resource_throttle.h',
      ],
      'conditions': [
        ['OS=="android"', {
          'dependencies': [
            'browser_component_jni_headers',
          ],
          'sources': [
            'component_jni_registrar.cc',
            'component_jni_registrar.h',
            'intercept_navigation_delegate.cc',
            'intercept_navigation_delegate.h',
          ],
        }],
      ],
    },
  ],
  'conditions': [
    ['OS=="android"', {
      'targets': [
        {
          'target_name': 'navigation_interception_java',
          'type': 'none',
          'dependencies': [
            '../../../base/base.gyp:base',
          ],
          'variables': {
            'package_name': 'navigation_interception',
            'java_in_dir': 'java',
          },
          'includes': [ '../../../../build/java.gypi' ],
        },
      ],
    }],
  ],
}
