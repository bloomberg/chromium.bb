# Copyright (c) 2011 The Chromium Authors. All rights reserved.
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
  'conditions': [
   # In component mode, we build all of content as a single DLL.
   # However, in the static mode, we need to build content as multiple
   # targets in order to prevent dependencies from getting introduced
   # upstream unnecessarily (e.g., content_renderer depends on allocator
   # and chrome_exe depends on content_common but we don't want
   # chrome_exe to have to depend on allocator).
   # TODO(dpranke): Uncomment: ['component == "static_library"', {
   ['1 == 1', {
     'target_defaults': {
       'defines': [
         'CONTENT_IMPLEMENTATION',
       ],
     },
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
       'includes': [
          'content_app.gypi',
        ],
       'dependencies': [
          'content_common',
        ],
      },
      {'target_name': 'content_browser',
       'type': 'static_library',
       'includes': [
          'content_browser.gypi',
        ],
       'dependencies': [
          'content_common',
        ],
      },
      {'target_name': 'content_common',
       'type': 'static_library',
       'includes': [
          'content_common.gypi',
        ],
      },
      {'target_name': 'content_gpu',
       'type': 'static_library',
       'includes': [
          'content_gpu.gypi',
        ],
       'dependencies': [
          'content_common',
        ],
      },
      {'target_name': 'content_plugin',
       'type': 'static_library',
       'includes': [
          'content_plugin.gypi',
        ],
       'dependencies': [
          'content_common',
        ],
      },
      {'target_name': 'content_ppapi_plugin',
       'type': 'static_library',
       'includes': [
          'content_ppapi_plugin.gypi',
        ],
      },
      {'target_name': 'content_renderer',
       'type': 'static_library',
       'includes': [
          'content_renderer.gypi',
        ],
       'dependencies': [
          'content_common',
        ],
      },
      {'target_name': 'content_utility',
       'type': 'static_library',
       'includes': [
          'content_utility.gypi',
        ],
       'dependencies': [
          'content_common',
        ],
      },
      {'target_name': 'content_worker',
       'type': 'static_library',
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
     'target_defaults': {
       'defines': [
         'CONTENT_IMPLEMENTATION',
       ],
     },
     'targets': [
      {'target_name': 'content',
       'type': 'shared_library',
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
       'dependencies': ['content'],
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
  ],
}
