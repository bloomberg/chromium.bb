# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'conditions': [
    ['android_must_copy_system_libraries == 1', {
      'targets': [
        {
          # These libraries from the Android ndk are required to be packaged with
          # any APK that is built with them. build/java_apk.gypi expects any
          # libraries that should be packaged with the apk to be in
          # <(SHARED_LIB_DIR)
          'target_name': 'copy_system_libraries',
          'type': 'none',
          'copies': [
            {
              'destination': '<(SHARED_LIB_DIR)/',
              'files': [
                '<(android_libcpp_libs_dir)/libc++_shared.so',
              ],
            },
          ],
        },
      ],
    }],
  ],
  'targets': [
    {
      'target_name': 'get_build_device_configurations',
      'type': 'none',
      'actions': [
        {
          'action_name': 'get configurations',
          'inputs': [
            'gyp/util/build_device.py',
            'gyp/get_device_configuration.py',
          ],
          'outputs': [
            '<(build_device_config_path)',
            '<(build_device_config_path).fake',
          ],
          'action': [
            'python', 'gyp/get_device_configuration.py',
            '--output=<(build_device_config_path)',
            '--output-directory=<(PRODUCT_DIR)',
          ],
        }
      ],
    },
    {
      # We cannot use the jinja gypi file because we need to do operations with
      # the PRODUCT_DIR variable before passing it to the jinja template.
      # This variable is not known at GYP time so we have to pass it to a
      # wrapper script to do the operations.
      'target_name': 'devil_chromium_config',
      'type': 'none',
      'actions': [
        {
          'action_name': 'devil_chromium_config__jinja_template',
          'message': 'processing jinja template',
          'variables': {
            'output_file_name': 'devil_chromium.json',
          },
          'action': [
            'python', '<(DEPTH)/build/android/gyp_devil_jinja_processor.py',
            '--android-abi', '<(android_app_abi)',
            '--android-sdk-root', '<(android_sdk_root)',
            '--build-tools-version', '<(android_sdk_build_tools_version)',
            '--input-file-path', '<(DEPTH)/build/android/devil_chromium.jinja',
            '--output-file-name', '<(output_file_name)',
            '--product-dir', '<(PRODUCT_DIR)',
          ],
          'inputs': [
            '<(DEPTH)/build/android/gyp_devil_jinja_processor.py',
            '<(DEPTH)/build/android/gyp/jinja_template.py',
          ],
          'outputs': [
            '<(PRODUCT_DIR)/gen/<(output_file_name)',
          ]
        },
      ],
    }, # devil_chromium_config
    {
      # Target for creating common output build directories. Creating output
      # dirs beforehand ensures that build scripts can assume these folders to
      # exist and there are no race conditions resulting from build scripts
      # trying to create these directories.
      # The build/java.gypi target depends on this target.
      'target_name': 'build_output_dirs',
      'type': 'none',
      'actions': [
        {
          'action_name': 'create_java_output_dirs',
          'variables' : {
            'output_dirs' : [
              '<(PRODUCT_DIR)/apks',
              '<(PRODUCT_DIR)/lib.java',
              '<(PRODUCT_DIR)/test.lib.java',
            ]
          },
          'inputs' : [],
          # By not specifying any outputs, we ensure that this command isn't
          # re-run when the output directories are touched (i.e. apks are
          # written to them).
          'outputs': [''],
          'action': [
            'mkdir',
            '-p',
            '<@(output_dirs)',
          ],
        },
      ],
    }, # build_output_dirs
    {
      'target_name': 'sun_tools_java',
      'type': 'none',
      'variables': {
        'found_jar_path': '<(PRODUCT_DIR)/sun_tools_java/tools.jar',
        'jar_path': '<(found_jar_path)',
      },
      'includes': [
        '../../build/host_prebuilt_jar.gypi',
      ],
      'actions': [
        {
          'action_name': 'find_sun_tools_jar',
          'variables' : {
          },
          'inputs' : [
            'gyp/find_sun_tools_jar.py',
            'gyp/util/build_utils.py',
          ],
          'outputs': [
            '<(found_jar_path)',
          ],
          'action': [
            'python', 'gyp/find_sun_tools_jar.py',
            '--output', '<(found_jar_path)',
          ],
        },
      ],
    }, # sun_tools_java
  ]
}

