# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,  # Use higher warning level.
    'directxsdk_exists': '<!(python <(DEPTH)/build/dir_exists.py ../third_party/directxsdk)',
    'conditions': [
      ['inside_chromium_build==0', {
        'webkit_src_dir': '../../../..',
      },{
        'webkit_src_dir': '../third_party/WebKit',
      }],
    ],
  },
  'target_defaults': {
    'defines': ['CONTENT_IMPLEMENTATION'],
    'conditions': [
      ['inside_chromium_build==0', {
        'dependencies': [
          '../webkit/support/setup_third_party.gyp:third_party_headers',
        ],
      }],
    ],
  },
  'conditions': [
    ['inside_chromium_build==1', {
      'includes': [
        'content_tests.gypi',
      ],
    }],
    ['OS != "ios"', {
      'includes': [
        '../build/win_precompile.gypi',
        'content_components_navigation_interception.gypi',
        'content_shell.gypi',
      ],
    }],
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
        {
          'target_name': 'content',
          'type': 'none',
          'dependencies': [
            'content_app',
            'content_browser',
            'content_common',
          ],
          'conditions': [
            ['OS != "ios"', {
              'dependencies': [
                'content_gpu',
                'content_plugin',
                'content_ppapi_plugin',
                'content_renderer',
                'content_utility',
                'content_worker',
              ],
            }],
          ],
        },
        {
          'target_name': 'content_app',
          'type': 'static_library',
          'variables': { 'enable_wexit_time_destructors': 1, },
          'includes': [
            'content_app.gypi',
          ],
          'dependencies': [
            'content_common',
          ],
        },
        {
          'target_name': 'content_browser',
          'type': 'static_library',
          'variables': { 'enable_wexit_time_destructors': 1, },
          'includes': [
            'content_browser.gypi',
          ],
          'dependencies': [
            'content_common',
            'content_resources.gyp:content_resources',
          ],
          'conditions': [
            ['OS != "ios"', {
              'dependencies': [
                'content_gpu',
                'content_renderer',
              ],
            }],
          ],
        },
        {
          'target_name': 'content_common',
          'type': 'static_library',
          'variables': { 'enable_wexit_time_destructors': 1, },
          'includes': [
            'content_common.gypi',
          ],
          'conditions': [
            ['OS != "ios"', {
              'dependencies': [
                'content_resources.gyp:content_resources',
              ],
            }],
          ],
        },
      ],
      'conditions': [
        ['OS != "ios"', {
          'targets': [
            {
              'target_name': 'content_gpu',
              'type': 'static_library',
              'variables': { 'enable_wexit_time_destructors': 1, },
              'includes': [
                'content_gpu.gypi',
              ],
              'dependencies': [
                'content_common',
              ],
            },
            {
              'target_name': 'content_plugin',
              'type': 'static_library',
              'variables': { 'enable_wexit_time_destructors': 1, },
              'includes': [
                'content_plugin.gypi',
              ],
              'dependencies': [
                'content_common',
              ],
            },
            {
              'target_name': 'content_ppapi_plugin',
              'type': 'static_library',
              'variables': { 'enable_wexit_time_destructors': 1, },
              'includes': [
                'content_ppapi_plugin.gypi',
              ],
            },
            {
              'target_name': 'content_renderer',
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
            {
              'target_name': 'content_utility',
              'type': 'static_library',
              'variables': { 'enable_wexit_time_destructors': 1, },
              'includes': [
                'content_utility.gypi',
              ],
              'dependencies': [
                'content_common',
              ],
            },
            {
              'target_name': 'content_worker',
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
        }],
      ],
    },
    {  # component != static_library
      'targets': [
        {
          'target_name': 'content',
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
          'msvs_settings': {
            'VCLinkerTool': {
              'conditions': [
                ['incremental_chrome_dll==1', {
                  'UseLibraryDependencyInputs': "true",
                }],
              ],
            },
          },
        },
        {
          'target_name': 'content_app',
          'type': 'none',
          'dependencies': ['content', 'content_browser'],
        },
        {
          'target_name': 'content_browser',
          'type': 'none',
          'dependencies': ['content'],
        },
        {
          'target_name': 'content_common',
          'type': 'none',
          'dependencies': ['content', 'content_resources.gyp:content_resources'],
        },
        {
          'target_name': 'content_gpu',
          'type': 'none',
          'dependencies': ['content'],
        },
        {
          'target_name': 'content_plugin',
          'type': 'none',
          'dependencies': ['content'],
        },
        {
          'target_name': 'content_ppapi_plugin',
          'type': 'none',
          'dependencies': ['content'],
        },
        {
          'target_name': 'content_renderer',
          'type': 'none',
          'dependencies': ['content'],
        },
        {
          'target_name': 'content_utility',
          'type': 'none',
          'dependencies': ['content'],
        },
        {
          'target_name': 'content_worker',
          'type': 'none',
          'dependencies': ['content'],
        },
      ],
    }],
    ['OS == "android"', {
      'targets': [
        {
          'target_name': 'common_aidl',
          'type': 'none',
          'variables': {
            'package_name': 'content',
            'aidl_interface_file': 'public/android/java/src/org/chromium/content/common/common.aidl',
          },
          'sources': [
            'public/android/java/src/org/chromium/content/common/ISandboxedProcessCallback.aidl',
            'public/android/java/src/org/chromium/content/common/ISandboxedProcessService.aidl',
          ],
          'includes': [ '../build/java_aidl.gypi' ],
        },
        {
          'target_name': 'content_java',
          'type': 'none',
          'dependencies': [
            '../base/base.gyp:base',
            '../net/net.gyp:net',
            '../ui/ui.gyp:ui_java',
            'common_aidl',
            'content_common',
            'page_transition_types_java',
          ],
          'variables': {
            'package_name': 'content',
            'java_in_dir': '../content/public/android/java',
            'has_java_resources': 1,
            'R_package': 'org.chromium.content',
            'R_package_relpath': 'org/chromium/content',
            'java_strings_grd': 'android_content_strings.grd',
          },
          'conditions': [
            ['android_build_type == 0', {
              'dependencies': [
                '../third_party/eyesfree/eyesfree.gyp:eyesfree_java',
                '../third_party/guava/guava.gyp:guava_javalib',
              ],
            }],
          ],
          'includes': [ '../build/java.gypi' ],
        },
        {
          'target_name': 'page_transition_types_java',
          'type': 'none',
          'sources': [
            'public/android/java/src/org/chromium/content/browser/PageTransitionTypes.template',
          ],
          'variables': {
            'package_name': 'org.chromium.content.browser',
            'template_deps': ['public/common/page_transition_types_list.h'],
          },
          'includes': [ '../build/android/java_cpp_template.gypi' ],
        },
        {
          'target_name': 'surface_texture_jni_headers',
          'type': 'none',
          'variables': {
            'jni_gen_dir': 'content',
            'input_java_class': 'android/graphics/SurfaceTexture.class',
            'input_jar_file': '<(android_sdk)/android.jar',
          },
          'includes': [ '../build/jar_file_jni_generator.gypi' ],
        },
        {
          'target_name': 'surface_jni_headers',
          'type': 'none',
          'variables': {
            'jni_gen_dir': 'content',
            'input_java_class': 'android/view/Surface.class',
            'input_jar_file': '<(android_sdk)/android.jar',
          },
          'includes': [ '../build/jar_file_jni_generator.gypi' ],
        },
        {
          'target_name': 'java_set_jni_headers',
          'type': 'none',
          'variables': {
            'jni_gen_dir': 'content',
            'input_java_class': 'java/util/HashSet.class',
            'input_jar_file': '<(android_sdk)/android.jar',
          },
          'includes': [ '../build/jar_file_jni_generator.gypi' ],
        },

        {
          'target_name': 'content_jni_headers',
          'type': 'none',
          'dependencies': [
            'java_set_jni_headers',
            'surface_texture_jni_headers',
            'surface_jni_headers',
          ],
          'direct_dependent_settings': {
            'include_dirs': [
              '<(SHARED_INTERMEDIATE_DIR)/content',
            ],
          },
          'includes': [ 'content_jni.gypi' ],
        },
      ],
    }],  # OS == "android"
  ],
}
