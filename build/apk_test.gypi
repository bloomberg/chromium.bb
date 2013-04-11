# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This file is meant to be included into a target to provide a rule
# to build APK based test suites.
#
# To use this, create a gyp target with the following form:
# {
#   'target_name': 'test_suite_name_apk',
#   'type': 'none',
#   'variables': {
#     'test_suite_name': 'test_suite_name',  # string
#     'input_shlib_path' : '/path/to/test_suite.so',  # string
#     'input_jars_paths': ['/path/to/test_suite.jar', ... ],  # list
#   },
#   'includes': ['path/to/this/gypi/file'],
# }
#

{
  'dependencies': [
    '<(DEPTH)/base/base.gyp:base_java',
    '<(DEPTH)/tools/android/android_tools.gyp:android_tools',
  ],
  'variables': {
    'generator_intermediate_dir': '<(PRODUCT_DIR)/<(test_suite_name)_apk/generated/',
    'generate_native_test_stamp': '<(generator_intermediate_dir)/generate_native_test.stamp',
  },
  'conditions': [
     ['OS == "android" and gtest_target_type == "shared_library"', {
       'variables': {
         # These are used to configure java_apk.gypi included below.
         'apk_name': '<(test_suite_name)',
         'intermediate_dir': '<(PRODUCT_DIR)/<(test_suite_name)_apk',
         'final_apk_path': '<(intermediate_dir)/<(test_suite_name)-debug.apk',
         'java_in_dir': '<(DEPTH)/build/android/empty',
         'android_manifest_path': '<(generator_intermediate_dir)/AndroidManifest.xml',
         'native_lib_target': 'lib<(test_suite_name)',
         'generated_src_dirs': [
           '<(generator_intermediate_dir)/java',
         ],
         'additional_input_paths': [
           '<(generate_native_test_stamp)',
         ],
         'additional_res_dirs': [
           '<(generator_intermediate_dir)/res',
         ],
       },
       'actions': [
         {
           'action_name': 'apk_<(test_suite_name)',
           'message': 'Building <(test_suite_name) test apk.',
           'inputs': [
             '<(DEPTH)/testing/android/AndroidManifest.xml',
             '<(DEPTH)/testing/android/generate_native_test.py',
             '<(input_shlib_path)',
             '>@(input_jars_paths)',
             '<!@(find <(DEPTH)/testing/android/java)',
           ],
           'outputs': [
             '<(generate_native_test_stamp)',
             '<(android_manifest_path)',
           ],
           'action': [
             '<(DEPTH)/testing/android/generate_native_test.py',
             '--native_library',
             '<(input_shlib_path)',
             '--output',
             '<(generator_intermediate_dir)',
             '--strip-binary=<(android_strip)',
             '--app_abi',
             '<(android_app_abi)',
             '--stamp-file',
             '<(generate_native_test_stamp)',
             '--no-compile',
           ],
         },
       ],
       'includes': [ 'java_apk.gypi' ],
     }],  # 'OS == "android" and gtest_target_type == "shared_library"
  ],  # conditions
}
