# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,  # Use higher warning level.
    'directxsdk_exists': '<!(python <(DEPTH)/build/dir_exists.py ../third_party/directxsdk)',
  },
  'includes': [
    '../build/win_precompile.gypi',
    'content_shell.gypi',
    'content_tests.gypi',
  ],
  'target_defaults': {
    'defines': ['CONTENT_IMPLEMENTATION'],
  },
  'conditions': [
   # In component mode, we build all of content as a single DLL.
   # However, in the static mode, we need to build content as multiple
   # targets in order to prevent dependencies from getting introduced
   # upstream unnecessarily (e.g., content_renderer depends on allocator
   # and chrome_exe depends on content_common but we don't want
   # chrome_exe to have to depend on allocator).
   ['component=="static_library"', {
     'target_defines': [
       'COMPILE_CONTENT_STATICALLY',
     ],
     'targets': [
      {'target_name': 'content',
       'type': 'none',
       'dependencies': [
        'content_app',
        'content_browser',
        'content_common',
        'content_gpu',
        'content_plugin',
        'content_ppapi_plugin',
        'content_renderer',
        'content_utility',
        'content_worker',
       ],
      },
      {'target_name': 'content_app',
       'type': 'static_library',
       'variables': { 'enable_wexit_time_destructors': 1, },
       'includes': [
          'content_app.gypi',
        ],
       'dependencies': [
          'content_common',
        ],
      },
      {'target_name': 'content_browser',
       'type': 'static_library',
       'variables': { 'enable_wexit_time_destructors': 1, },
       'includes': [
          'content_browser.gypi',
        ],
       'dependencies': [
          'content_common',
        ],
      },
      {'target_name': 'content_common',
       'type': 'static_library',
       'variables': { 'enable_wexit_time_destructors': 1, },
       'includes': [
          'content_common.gypi',
        ],
       'dependencies': [
          'content_resources.gyp:content_resources',
        ],
      },
      {'target_name': 'content_gpu',
       'type': 'static_library',
       'variables': { 'enable_wexit_time_destructors': 1, },
       'includes': [
          'content_gpu.gypi',
        ],
       'dependencies': [
          'content_common',
        ],
      },
      {'target_name': 'content_plugin',
       'type': 'static_library',
       'variables': { 'enable_wexit_time_destructors': 1, },
       'includes': [
          'content_plugin.gypi',
        ],
       'dependencies': [
          'content_common',
        ],
      },
      {'target_name': 'content_ppapi_plugin',
       'type': 'static_library',
       'variables': { 'enable_wexit_time_destructors': 1, },
       'includes': [
          'content_ppapi_plugin.gypi',
        ],
      },
      {'target_name': 'content_renderer',
       'type': 'static_library',
       'variables': { 'enable_wexit_time_destructors': 1, },
       'includes': [
          'content_renderer.gypi',
        ],
       'dependencies': [
          'content_common',
          'content_resources.gyp:content_resources',
        ],
      },
      {'target_name': 'content_utility',
       'type': 'static_library',
       'variables': { 'enable_wexit_time_destructors': 1, },
       'includes': [
          'content_utility.gypi',
        ],
       'dependencies': [
          'content_common',
        ],
      },
      {'target_name': 'content_worker',
       'type': 'static_library',
       'variables': { 'enable_wexit_time_destructors': 1, },
       'includes': [
          'content_worker.gypi',
       ],
       'dependencies': [
          'content_common',
        ],
      },
     ],
    },
    { # component != static_library
     'targets': [
      {'target_name': 'content',
       'type': 'shared_library',
       'variables': { 'enable_wexit_time_destructors': 1, },
       'dependencies': [
         'content_resources.gyp:content_resources',
       ],
       'conditions': [
         ['OS=="mac"', {
           'dependencies': [
             '<(DEPTH)/third_party/mach_override/mach_override.gyp:mach_override',
           ],
         }],
       ],
       'includes': [
        'content_app.gypi',
        'content_browser.gypi',
        'content_common.gypi',
        'content_gpu.gypi',
        'content_plugin.gypi',
        'content_ppapi_plugin.gypi',
        'content_renderer.gypi',
        'content_utility.gypi',
        'content_worker.gypi',
       ],
      },
      {'target_name': 'content_app',
       'type': 'none',
       'dependencies': ['content', 'content_browser'],
      },
      {'target_name': 'content_browser',
       'type': 'none',
       'dependencies': ['content'],
      },
      {'target_name': 'content_common',
       'type': 'none',
       'dependencies': ['content', 'content_resources.gyp:content_resources'],
      },
      {'target_name': 'content_gpu',
       'type': 'none',
       'dependencies': ['content'],
      },
      {'target_name': 'content_plugin',
       'type': 'none',
       'dependencies': ['content'],
      },
      {'target_name': 'content_ppapi_plugin',
       'type': 'none',
       'dependencies': ['content'],
      },
      {'target_name': 'content_renderer',
       'type': 'none',
       'dependencies': ['content'],
      },
      {'target_name': 'content_utility',
       'type': 'none',
       'dependencies': ['content'],
      },
      {'target_name': 'content_worker',
       'type': 'none',
       'dependencies': ['content'],
      },
     ],
    },
   ],
   ['OS == "android"', {
     'targets': [
       {
         'target_name': 'content_java',
         'type': 'none',
         'dependencies': ['../base/base.gyp:base_java'],
         'variables': {
           'package_name': 'content',
           'java_in_dir': '../content/public/android/java',
         },
         'includes': [ '../build/java.gypi' ],
       },
       {
         'target_name': 'content_jni_headers',
         'type': 'none',
         'includes': [ 'content_jni.gypi' ],
       },
     ],
   }],  # OS == "android"
  ],
}
