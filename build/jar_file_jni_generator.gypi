# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This file is meant to be included into a target to provide a rule
# to generate jni bindings for system Java-files in a consistent manner.
#
# To use this, create a gyp target with the following form:
# {
#   'target_name': 'android_jar_jni_headers',
#   'type': 'none',
#   'variables': {
#     'jni_gen_package': 'chrome',
#     'input_java_class': 'java/io/InputStream.class',
#   },
#   'includes': [ '../build/jar_file_jni_generator.gypi' ],
# },

{
  'variables': {
    'jni_generator': '<(DEPTH)/base/android/jni_generator/jni_generator.py',
  },
  'actions': [
    {
      'action_name': 'generate_jni_headers_from_jar_file',
      'inputs': [
        '<(jni_generator)',
        '<(android_sdk_jar)',
      ],
      'variables': {
        'java_class_name': '<!(basename <(input_java_class)|sed "s/\.class//")'
      },
      'outputs': [
        '<(SHARED_INTERMEDIATE_DIR)/<(jni_gen_package)/jni/<(java_class_name)_jni.h',
      ],
      'action': [
        '<(jni_generator)',
        '-j',
        '<(android_sdk_jar)',
        '--input_file',
        '<(input_java_class)',
        '--output_dir',
        '<(SHARED_INTERMEDIATE_DIR)/<(jni_gen_package)/jni',
        '--optimize_generation',
        '<(optimize_jni_generation)',
      ],
      'message': 'Generating JNI bindings from  <(android_sdk_jar)/<(input_java_class)',
      'process_outputs_as_sources': 1,
    },
  ],
  # This target exports a hard dependency because it generates header
  # files.
  'hard_dependency': 1,
}
