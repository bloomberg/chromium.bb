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
   #
   # TODO(dpranke): Remove the mac conditional once the circular
   # dependencies in WebKit.gyp are fixed.
   # See https://bugs.webkit.org/show_bug.cgi?id=68463
   ['OS=="mac" or component=="static_library" or incremental_chrome_dll==1', {
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
     'conditions': [
       ['OS=="mac"', {
         'targets': [
           {
             'target_name': 'closure_blocks_leopard_compat',
             'defines!': ['CONTENT_IMPLEMENTATION'],
             'conditions': [
               ['mac_sdk == "10.5"', {
                 'type': 'shared_library',
                 'product_name': 'closure_blocks_leopard_compat_stub',
                 'variables': {
                   # This target controls stripping directly. See below.
                   'mac_strip': 0,
                 },
                 'sources': [
                   'browser/mac/closure_blocks_leopard_compat.S',
                 ],
                 'xcode_settings': {
                   # These values are taken from libSystem.dylib in the 10.5
                   # SDK. Setting LD_DYLIB_INSTALL_NAME causes anything linked
                   # against this stub library to look for the symbols it
                   # provides in the real libSystem at runtime. When using ld
                   # from Xcode 4 or later (ld64-123.2 and up), giving two
                   # libraries with the same "install name" to the linker will
                   # cause it to print "ld: warning: dylibs with same install
                   # name". This is harmless, and ld will behave as intended
                   # here.
                   #
                   # The real library's compatibility version is used, and the
                   # value of the current version from the SDK is used to make
                   # it appear as though anything linked against this stub was
                   # linked against the real thing.
                   'LD_DYLIB_INSTALL_NAME': '/usr/lib/libSystem.B.dylib',
                   'DYLIB_COMPATIBILITY_VERSION': '1.0.0',
                   'DYLIB_CURRENT_VERSION': '111.1.4',

                   # Turn on stripping (yes, even in debug mode), and add the -c
                   # flag. This is what produces a stub library (MH_DYLIB_STUB)
                   # as opposed to a dylib (MH_DYLIB). MH_DYLIB_STUB files
                   # contain symbol tables and everything else needed for
                   # linking, but are stripped of section contents. This is the
                   # same way that the stub libraries in Mac OS X SDKs are
                   # created. dyld will refuse to load a stub library, so this
                   # provides some insurance in case anyone tries to load the
                   # stub at runtime.
                   'DEPLOYMENT_POSTPROCESSING': 'YES',
                   'STRIP_STYLE': 'non-global',
                   'STRIPFLAGS': '-c',
                 },
               }, {  # else: mac_sdk != "10.5"
                 # When using the 10.6 SDK or newer, the necessary definitions
                 # are already present in libSystem.dylib. There is no need to
                 # build a stub dylib to provide these symbols at link time.
                 # This target is still useful to cause those symbols to be
                 # treated as weak imports in dependents, who still must
                 # #include closure_blocks_leopard_compat.h to get weak imports.
                 'type': 'none',
               }],
             ],
           },
         ],
       }],
      ],
    },
    { # component != static_library
     'targets': [
      {'target_name': 'content',
       'type': 'shared_library',
       'variables': { 'enable_wexit_time_destructors': 1, },
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
  ],
}
