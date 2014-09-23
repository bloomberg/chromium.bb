# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'conditions': [
    ['OS=="android" and use_icu_alternatives_on_android==1', {
      # TODO(mef): Figure out what needs to be done for gn script.
      'targets': [
        {
          'target_name': 'cronet_jni_headers',
          'type': 'none',
          'sources': [
            'cronet/android/java/src/org/chromium/net/ChromiumUrlRequest.java',
            'cronet/android/java/src/org/chromium/net/ChromiumUrlRequestContext.java',
          ],
          'variables': {
            'jni_gen_package': 'cronet',
          },
          'includes': [ '../build/jni_generator.gypi' ],
        },
        {
          'target_name': 'cronet_url_request_error_list',
          'type': 'none',
          'sources': [
            'cronet/android/java/src/org/chromium/net/ChromiumUrlRequestError.template',
          ],
          'variables': {
            'package_name': 'org/chromium/cronet',
            'template_deps': ['cronet/android/chromium_url_request_error_list.h'],
          },
          'includes': [ '../build/android/java_cpp_template.gypi' ],
        },
        {
          'target_name': 'cronet_url_request_priority_list',
          'type': 'none',
          'sources': [
            'cronet/android/java/src/org/chromium/net/ChromiumUrlRequestPriority.template',
          ],
          'variables': {
            'package_name': 'org/chromium/cronet',
            'template_deps': ['cronet/android/chromium_url_request_priority_list.h'],
          },
          'includes': [ '../build/android/java_cpp_template.gypi' ],
        },
        {
          'target_name': 'cronet_url_request_context_config_list',
          'type': 'none',
          'sources': [
            'cronet/android/java/src/org/chromium/net/UrlRequestContextConfig.template',
          ],
          'variables': {
            'package_name': 'org/chromium/cronet',
            'template_deps': ['cronet/url_request_context_config_list.h'],
          },
          'includes': [ '../build/android/java_cpp_template.gypi' ],
        },
        {
          'target_name': 'cronet_version',
          'type': 'none',
          # Because cronet_version generates a header, we must set the
          # hard_dependency flag.
          'hard_dependency': 1,
          'actions': [
            {
              'action_name': 'cronet_version',
              'variables': {
                'lastchange_path': '<(DEPTH)/build/util/LASTCHANGE',
                'version_py_path': '<(DEPTH)/build/util/version.py',
                'version_path': '<(DEPTH)/chrome/VERSION',
                'template_input_path': 'cronet/android/java/src/org/chromium/net/Version.template',
              },
              'inputs': [
                '<(template_input_path)',
                '<(version_path)',
                '<(lastchange_path)',
              ],
              'outputs': [
                '<(SHARED_INTERMEDIATE_DIR)/templates/org/chromium/cronet/Version.java',
              ],
              'action': [
                'python',
                '<(version_py_path)',
                '-f', '<(version_path)',
                '-f', '<(lastchange_path)',
                '<(template_input_path)',
                '<@(_outputs)',
              ],
              'message': 'Generating version information',
            },
          ],
        },
        {
          'target_name': 'cronet_static',
          'type': 'static_library',
          'dependencies': [
            '../base/base.gyp:base',
            '../base/base.gyp:base_i18n',
            '../third_party/icu/icu.gyp:icui18n',
            '../third_party/icu/icu.gyp:icuuc',
            'cronet_jni_headers',
            'cronet_url_request_context_config_list',
            'cronet_url_request_error_list',
            'cronet_url_request_priority_list',
            'cronet_version',
            '../net/net.gyp:net',
          ],
          'sources': [
            'cronet/url_request_context_config.cc',
            'cronet/url_request_context_config.h',
            'cronet/url_request_context_config_list.h',
            'cronet/android/chromium_url_request.cc',
            'cronet/android/chromium_url_request.h',
            'cronet/android/chromium_url_request_error_list.h',
            'cronet/android/chromium_url_request_priority_list.h',
            'cronet/android/chromium_url_request_context.cc',
            'cronet/android/chromium_url_request_context.h',
            'cronet/android/cronet_loader.cc',
            'cronet/android/cronet_loader.h',
            'cronet/android/url_request_adapter.cc',
            'cronet/android/url_request_adapter.h',
            'cronet/android/url_request_context_adapter.cc',
            'cronet/android/url_request_context_adapter.h',
            'cronet/android/wrapped_channel_upload_element_reader.cc',
            'cronet/android/wrapped_channel_upload_element_reader.h',
          ],
          'cflags': [
            '-DLOGGING=1',
            '-fdata-sections',
            '-ffunction-sections',
            '-fno-rtti',
            '-fvisibility=hidden',
            '-fvisibility-inlines-hidden',
            '-Wno-sign-promo',
            '-Wno-missing-field-initializers',
          ],
          'ldflags': [
            '-llog',
            '-landroid',
            '-Wl,--gc-sections',
            '-Wl,--exclude-libs,ALL'
          ],
          'conditions': [
            [ 'use_icu_alternatives_on_android == 1', {
                'dependencies!': [
                  '../base/base.gyp:base_i18n',
                  '../third_party/icu/icu.gyp:icui18n',
                  '../third_party/icu/icu.gyp:icuuc',
                ]
              },
            ],
          ],
        },
        {
          'target_name': 'libcronet',
          'type': 'shared_library',
          'sources': [
            'cronet/android/cronet_jni.cc',
          ],
          'dependencies': [
            'cronet_static',
            '../base/base.gyp:base',
            '../net/net.gyp:net',
            '../url/url.gyp:url_lib',
          ],
        },
        { # cronet_stub.jar defines HttpUrlRequest interface and provides its
          # its implementation using HttpUrlConnection (not the Chromium stack).
          'target_name': 'cronet_stub',
          'type': 'none',
          'dependencies': [
            'cronet_url_request_context_config_list',
            'cronet_version',
          ],
          'variables': {
            'java_in_dir': 'cronet/android/java',
            'javac_includes': [
              '**/AsyncUrlRequest.java',
              '**/AsyncUrlRequestException.java',
              '**/AsyncUrlRequestFactory.java',
              '**/AsyncUrlRequestListener.java',
              '**/ResponseInfo.java',
              '**/ChunkedWritableByteChannel.java',
              '**/HttpUrlConnection*.java',
              '**/HttpUrlRequest*.java',
              '**/ResponseTooLargeException.java',
              '**/UserAgent.java',
              # TODO(mef): Consider moving this into HttpUrlRequestConfig.
              '**/UrlRequestContextConfig.java',
              '**/Version.java',
            ],
          },
          'includes': [ '../build/java.gypi' ],
        },
        { # cronet.jar implements HttpUrlRequest interface using Chromium stack
          # in native libcronet.so library.
          'target_name': 'cronet',
          'type': 'none',
          'dependencies': [
            '../base/base.gyp:base',
            'cronet_stub',
            'cronet_url_request_error_list',
            'cronet_url_request_priority_list',
            'libcronet',
          ],
          'variables': {
            'java_in_dir': 'cronet/android/java',
            'javac_includes': [
              '**/ChromiumAsyncUrlRequest.java',
              '**/ChromiumUrlRequest.java',
              '**/ChromiumUrlRequestContext.java',
              '**/ChromiumUrlRequestError.java',
              '**/ChromiumUrlRequestFactory.java',
              '**/ChromiumUrlRequestPriority.java',
              '**/UsedByReflection.java',
            ],
          },
          'includes': [ '../build/java.gypi' ],
        },
        {
          'target_name': 'cronet_package',
          'type': 'none',
          'dependencies': [
            'libcronet',
            'cronet',
            'cronet_stub',
          ],
          'variables': {
              'native_lib': 'libcronet.>(android_product_extension)',
              'java_lib': 'cronet.jar',
              'java_stub_lib': 'cronet_stub.jar',
              'java_src_lib': 'cronet-src.jar',
              'java_sample_src_lib': 'cronet-sample-src.jar',
              'lib_java_dir': '<(PRODUCT_DIR)/lib.java',
              'package_dir': '<(PRODUCT_DIR)/cronet',
              'intermediate_dir': '<(SHARED_INTERMEDIATE_DIR)/cronet',
              'jar_extract_dir': '<(intermediate_dir)/cronet_jar_extract',
              'jar_excluded_classes': [
                '*/library_loader/*',
              ],
              'jar_extract_stamp': '<(intermediate_dir)/jar_extract.stamp',
              'cronet_jar_stamp': '<(intermediate_dir)/cronet_jar.stamp',
          },
          'actions': [
            {
              'action_name': 'strip libcronet',
              'inputs': ['<(SHARED_LIB_DIR)/<(native_lib)'],
              'outputs': ['<(package_dir)/libs/<(android_app_abi)/<(native_lib)'],
              'action': [
                '<(android_strip)',
                '--strip-unneeded',
                '<@(_inputs)',
                '-o',
                '<@(_outputs)',
              ],
            },
            {
              'action_name': 'extracting from jars',
              'inputs':  [
                '<(lib_java_dir)/<(java_lib)',
                '<(lib_java_dir)/base_java.jar',
                '<(lib_java_dir)/net_java.jar',
                '<(lib_java_dir)/url_java.jar',
              ],
              'outputs': ['<(jar_extract_stamp)', '<(jar_extract_dir)'],
              'action': [
                'python',
                'cronet/tools/extract_from_jars.py',
                '--classes-dir=<(jar_extract_dir)',
                '--jars=<@(_inputs)',
                '--stamp=<(jar_extract_stamp)',
              ],
            },
            {
              'action_name': 'jar_<(_target_name)',
              'message': 'Creating <(_target_name) jar',
              'inputs': [
                '<(DEPTH)/build/android/gyp/util/build_utils.py',
                '<(DEPTH)/build/android/gyp/util/md5_check.py',
                '<(DEPTH)/build/android/gyp/jar.py',
                '<(jar_extract_stamp)',
              ],
              'outputs': [
                '<(package_dir)/<(java_lib)',
                '<(cronet_jar_stamp)',
              ],
              'action': [
                'python', '<(DEPTH)/build/android/gyp/jar.py',
                '--classes-dir=<(jar_extract_dir)',
                '--jar-path=<(package_dir)/<(java_lib)',
                '--excluded-classes=<@(jar_excluded_classes)',
                '--stamp=<(cronet_jar_stamp)',
              ]
            },
            {
              'action_name': 'jar_src_<(_target_name)',
              'inputs': ['cronet/tools/jar_src.py'] ,
              'outputs': ['<(package_dir)/<(java_src_lib)'],
              'action': [
                'python',
                '<@(_inputs)',
                '--src-dir=cronet/android/java/src',
                '--jar-path=<(package_dir)/<(java_src_lib)',
              ],
            },
            {
              'action_name': 'jar_sample_src_<(_target_name)',
              'inputs': ['cronet/tools/jar_src.py'] ,
              'outputs': ['<(package_dir)/<(java_sample_src_lib)'],
              'action': [
                'python',
                '<@(_inputs)',
                '--src-dir=cronet/android/sample',
                '--jar-path=<(package_dir)/<(java_sample_src_lib)',
              ],
            },
            {
              'action_name': 'generate licenses',
              'inputs': ['cronet/tools/cronet_licenses.py'] ,
              'outputs': ['<(package_dir)/LICENSE'],
              'action': [
                'python',
                '<@(_inputs)',
                'license',
                '<@(_outputs)',
              ],
            },
          ],
          'copies': [
            {
              'destination': '<(package_dir)',
              'files': [
                '../AUTHORS',
                '../chrome/VERSION',
                'cronet/android/proguard.cfg',
                '<(lib_java_dir)/<(java_stub_lib)'
              ],
            },
            {
              'destination': '<(package_dir)/symbols/<(android_app_abi)',
              'files': [
                '<(SHARED_LIB_DIR)/<(native_lib)',
              ],
            },
          ],
        },
        {
          'target_name': 'cronet_sample_apk',
          'type': 'none',
          'dependencies': [
            'cronet',
            'cronet_stub',
          ],
          'variables': {
            'apk_name': 'CronetSample',
            'java_in_dir': 'cronet/android/sample',
            'resource_dir': 'cronet/android/sample/res',
            'native_lib_target': 'libcronet',
            'proguard_enabled': 'true',
            'proguard_flags_paths': [
              'cronet/android/proguard.cfg',
              'cronet/android/sample/javatests/proguard.cfg',
            ],
          },
          'includes': [ '../build/java_apk.gypi' ],
        },
        {
          # cronet_sample_apk creates a .jar as a side effect. Any java targets
          # that need that .jar in their classpath should depend on this target,
          # cronet_sample_apk_java. Dependents of cronet_sample_apk receive its
          # jar path in the variable 'apk_output_jar_path'. This target should
          # only be used by targets which instrument cronet_sample_apk.
          'target_name': 'cronet_sample_apk_java',
          'type': 'none',
          'dependencies': [
            'cronet_sample_apk',
          ],
          'includes': [ '../build/apk_fake_jar.gypi' ],
        },
        {
          'target_name': 'cronet_sample_test_apk',
          'type': 'none',
          'dependencies': [
            'cronet',
            'cronet_sample_apk_java',
            'cronet_stub',
            '../base/base.gyp:base_java',
            '../base/base.gyp:base_java_test_support',
          ],
          'variables': {
            'apk_name': 'CronetSampleTest',
            'java_in_dir': 'cronet/android/sample/javatests',
            'resource_dir': 'cronet/android/sample/res',
            'is_test_apk': 1,
          },
          'includes': [ '../build/java_apk.gypi' ],
        },
        {
          'target_name': 'cronet_tests_jni_headers',
          'type': 'none',
          'sources': [
            'cronet/android/test/src/org/chromium/cronet_test_apk/CronetTestUtil.java',
            'cronet/android/test/javatests/src/org/chromium/cronet_test_apk/MockUrlRequestJobTest.java',
          ],
          'variables': {
            'jni_gen_package': 'cronet_tests',
          },
          'includes': [ '../build/jni_generator.gypi' ],
        },
        {
          'target_name': 'libcronet_tests',
          'type': 'shared_library',
          'sources': [
            'cronet/android/test/cronet_test_jni.cc',
            'cronet/android/test/mock_url_request_job_test.cc',
            'cronet/android/test/mock_url_request_job_test.h',
            '../net/base/directory_lister.cc',
            '../net/base/directory_lister.h',
            '../net/url_request/url_request_file_job.cc',
            '../net/url_request/url_request_file_job.h',
          ],
          'dependencies': [
            'cronet_static',
            'cronet_tests_jni_headers',
            '../base/base.gyp:base',
            '../net/net.gyp:net',
            '../net/net.gyp:net_test_support',
            '../net/net.gyp:quic_tools',
            '../url/url.gyp:url_lib',
            '../base/base.gyp:base_i18n',
            '../third_party/icu/icu.gyp:icui18n',
            '../third_party/icu/icu.gyp:icuuc',
          ],
        },
        {
          'target_name': 'cronet_test_apk',
          'type': 'none',
          'dependencies': [
            'cronet',
          ],
          'variables': {
            'apk_name': 'CronetTest',
            'java_in_dir': 'cronet/android/test',
            'resource_dir': 'cronet/android/test/res',
            'asset_location': 'cronet/android/test/assets',
            'native_lib_target': 'libcronet_tests',
          },
          'includes': [ '../build/java_apk.gypi' ],
        },
        {
          # cronet_test_apk creates a .jar as a side effect. Any java targets
          # that need that .jar in their classpath should depend on this target,
          # cronet_sample_apk_java. Dependents of cronet_test_apk receive its
          # jar path in the variable 'apk_output_jar_path'. This target should
          # only be used by targets which instrument cronet_test_apk.
          'target_name': 'cronet_test_apk_java',
          'type': 'none',
          'dependencies': [
            'cronet_test_apk',
          ],
          'includes': [ '../build/apk_fake_jar.gypi' ],
        },
        {
          'target_name': 'cronet_test_instrumentation_apk',
          'type': 'none',
          'dependencies': [
            'cronet_test_apk_java',
            '../base/base.gyp:base_java',
            '../base/base.gyp:base_javatests',
            '../base/base.gyp:base_java_test_support',
          ],
          'variables': {
            'apk_name': 'CronetTestInstrumentation',
            'java_in_dir': 'cronet/android/test/javatests',
            'resource_dir': 'cronet/android/test/res',
            'is_test_apk': 1,
          },
          'includes': [ '../build/java_apk.gypi' ],
        },
      ],
    }],  # OS=="android"
  ],
}
