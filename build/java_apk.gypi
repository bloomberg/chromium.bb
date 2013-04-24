# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This file is meant to be included into a target to provide a rule
# to build Android APKs in a consistent manner.
#
# To use this, create a gyp target with the following form:
# {
#   'target_name': 'my_package_apk',
#   'type': 'none',
#   'variables': {
#     'apk_name': 'MyPackage',
#     'java_in_dir': 'path/to/package/root',
#     'resource_dir': 'path/to/package/root/res',
#   },
#   'includes': ['path/to/this/gypi/file'],
# }
#
# Required variables:
#  apk_name - The final apk will be named <apk_name>.apk
#  java_in_dir - The top-level java directory. The src should be in
#    <(java_in_dir)/src.
# Optional/automatic variables:
#  additional_input_paths - These paths will be included in the 'inputs' list to
#    ensure that this target is rebuilt when one of these paths changes.
#  additional_res_dirs - Additional directories containing Android resources.
#  additional_res_packages - Package names of the R.java files corresponding to
#    each directory in additional_res_dirs.
#  additional_src_dirs - Additional directories with .java files to be compiled
#    and included in the output of this target.
#  asset_location - The directory where assets are located.
#  generated_src_dirs - Same as additional_src_dirs except used for .java files
#    that are generated at build time. This should be set automatically by a
#    target's dependencies. The .java files in these directories are not
#    included in the 'inputs' list (unlike additional_src_dirs).
#  input_jars_paths - The path to jars to be included in the classpath. This
#    should be filled automatically by depending on the appropriate targets.
#  is_test_apk - Set to 1 if building a test apk.  This prevents resources from
#    dependencies from being re-included.
#  native_lib_target - The target_name of the target which generates the final
#    shared library to be included in this apk. A stripped copy of the
#    library will be included in the apk.
#  resource_dir - The directory for resources.
#  R_package - A custom Java package to generate the resource file R.java in.
#    By default, the package given in AndroidManifest.xml will be used.
#  java_strings_grd - The name of the grd file from which to generate localized
#    strings.xml files, if any.
#  library_manifest_paths'- Paths to additional AndroidManifest.xml files from
#    libraries.

{
  'variables': {
    'additional_input_paths': [],
    'input_jars_paths': [],
    'library_dexed_jars_paths': [],
    'additional_src_dirs': [],
    'generated_src_dirs': [],
    'app_manifest_version_name%': '<(android_app_version_name)',
    'app_manifest_version_code%': '<(android_app_version_code)',
    'proguard_enabled%': 'false',
    'proguard_flags_paths%': ['<(DEPTH)/build/android/empty_proguard.flags'],
    'native_lib_target%': '',
    'jar_name': 'chromium_apk_<(_target_name).jar',
    'resource_dir%':'<(DEPTH)/build/android/ant/empty/res',
    'R_package%':'',
    'additional_R_text_files': [],
    'additional_res_dirs': [],
    'additional_res_packages': [],
    'is_test_apk%': 0,
    'java_strings_grd%': '',
    'library_manifest_paths' : [],
    'resource_input_paths': [],
    'intermediate_dir': '<(PRODUCT_DIR)/<(_target_name)',
    'asset_location%': '<(intermediate_dir)/assets',
    'codegen_stamp': '<(intermediate_dir)/codegen.stamp',
    'compile_input_paths': [],
    'package_input_paths': [],
    'ordered_libraries_file': '<(intermediate_dir)/native_libraries.json',
    # TODO(cjhopman): build/ shouldn't refer to content/. The libraryloader and
    # nativelibraries template should be moved out of content/ (to base/?).
    # http://crbug.com/225101
    'native_libraries_template': '<(DEPTH)/content/public/android/java/templates/NativeLibraries.template',
    'native_libraries_java_dir': '<(intermediate_dir)/native_libraries_java/',
    'native_libraries_java_file': '<(native_libraries_java_dir)/NativeLibraries.java',
    'native_libraries_java_stamp': '<(intermediate_dir)/native_libraries_java.stamp',
    'native_libraries_template_data_dir': '<(intermediate_dir)/native_libraries/',
    'native_libraries_template_data_file': '<(native_libraries_template_data_dir)/native_libraries_array.h',
    'native_libraries_template_data_stamp': '<(intermediate_dir)/native_libraries_template_data.stamp',
    'compile_stamp': '<(intermediate_dir)/compile.stamp',
    'jar_stamp': '<(intermediate_dir)/jar.stamp',
    'obfuscate_stamp': '<(intermediate_dir)/obfuscate.stamp',
    'strip_stamp': '<(intermediate_dir)/strip.stamp',
    'classes_dir': '<(intermediate_dir)/classes',
    'javac_includes': [],
    'jar_excluded_classes': [],
    'jar_path': '<(PRODUCT_DIR)/lib.java/<(jar_name)',
    'obfuscated_jar_path': '<(intermediate_dir)/obfuscated.jar',
    'dex_path': '<(intermediate_dir)/classes.dex',
    'android_manifest_path%': '<(java_in_dir)/AndroidManifest.xml',
    'push_stamp': '<(intermediate_dir)/push.stamp',
    'link_stamp': '<(intermediate_dir)/link.stamp',
    'package_resources_stamp': '<(intermediate_dir)/package_resources.stamp',
    'codegen_input_paths': [],
    'keystore_path': '<(DEPTH)/build/android/ant/chromium-debug.keystore',
    'unsigned_apk_path': '<(intermediate_dir)/<(apk_name)-unsigned.apk',
    'final_apk_path%': '<(PRODUCT_DIR)/apks/<(apk_name).apk',
    'source_dir': '<(java_in_dir)/src',
    'apk_install_record': '<(intermediate_dir)/apk_install.record.stamp',
    'device_intermediate_dir': '/data/local/tmp/chromium/<(_target_name)/<(CONFIGURATION_NAME)',
    'symlink_script_host_path': '<(intermediate_dir)/create_symlinks.sh',
    'symlink_script_device_path': '<(device_intermediate_dir)/create_symlinks.sh',
    'variables': {
      'conditions': [
        ['gyp_managed_install == 1', {
          'apk_package_native_libs_dir': '<(intermediate_dir)/libs.managed',
        }, {
          'apk_package_native_libs_dir': '<(intermediate_dir)/libs',
        }],
      ],
    },
    'apk_package_native_libs_dir': '<(apk_package_native_libs_dir)',
  },
  # Pass the jar path to the apk's "fake" jar target.  This would be better as
  # direct_dependent_settings, but a variable set by a direct_dependent_settings
  # cannot be lifted in a dependent to all_dependent_settings.
  'all_dependent_settings': {
    'variables': {
      'apk_output_jar_path': '<(PRODUCT_DIR)/lib.java/<(jar_name)',
    },
  },
  'conditions': [
    ['resource_dir!=""', {
      'variables': {
        'resource_input_paths': [ '<!@(find <(resource_dir) -name "*")' ]
      },
    }],
    ['R_package != ""', {
      'variables': {
        # We generate R.java in package R_package (in addition to the package
        # listed in the AndroidManifest.xml, which is unavoidable).
        'additional_res_dirs': ['<(DEPTH)/build/android/ant/empty/res'],
        'additional_res_packages': ['<(R_package)'],
        'additional_R_text_files': ['<(PRODUCT_DIR)/<(package_name)/R.txt'],
      },
    }],
    ['native_lib_target != "" and component == "shared_library"', {
      'dependencies': [
        '<(DEPTH)/build/android/setup.gyp:copy_system_libraries',
      ],
    }],
    ['native_lib_target != ""', {
      'variables': {
        'compile_input_paths': [ '<(native_libraries_java_stamp)' ],
        'generated_src_dirs': [ '<(native_libraries_java_dir)' ],
        'native_libs_paths': [
          '<(SHARED_LIB_DIR)/<(native_lib_target).>(android_product_extension)'
        ],
        'package_input_paths': [
          '<(apk_package_native_libs_dir)/<(android_app_abi)/gdbserver',
        ],
      },
      'copies': [
        {
          # gdbserver is always copied into the APK's native libs dir. The ant
          # build scripts (apkbuilder task) will only include it in a debug
          # build.
          'destination': '<(apk_package_native_libs_dir)/<(android_app_abi)',
          'files': [
            '<(android_gdbserver)',
          ],
        },
      ],
      'actions': [
        {
          'variables': {
            'input_libraries': ['<@(native_libs_paths)'],
          },
          'includes': ['../build/android/write_ordered_libraries.gypi'],
        },
        {
          'action_name': 'native_libraries_template_data_<(_target_name)',
          'message': 'Creating native_libraries_list.h for <(_target_name).',
          'inputs': [
            '<(DEPTH)/build/android/gyp/util/build_utils.py',
            '<(DEPTH)/build/android/gyp/create_native_libraries_header.py',
            '<(ordered_libraries_file)',
          ],
          'outputs': [
            '<(native_libraries_template_data_stamp)',
          ],
          'action': [
            'python', '<(DEPTH)/build/android/gyp/create_native_libraries_header.py',
            '--ordered-libraries=<(ordered_libraries_file)',
            '--output=<(native_libraries_template_data_file)',
            '--stamp=<(native_libraries_template_data_stamp)',
          ],
        },
        {
          'action_name': 'native_libraries_<(_target_name)',
          'message': 'Creating NativeLibraries.java for <(_target_name).',
          'inputs': [
            '<(DEPTH)/build/android/gyp/util/build_utils.py',
            '<(DEPTH)/build/android/gyp/gcc_preprocess.py',
            '<(native_libraries_template_data_stamp)',
            '<(native_libraries_template)',
          ],
          'outputs': [
            '<(native_libraries_java_stamp)',
          ],
          'action': [
            'python', '<(DEPTH)/build/android/gyp/gcc_preprocess.py',
            '--include-path=<(native_libraries_template_data_dir)',
            '--output=<(native_libraries_java_file)',
            '--template=<(native_libraries_template)',
            '--stamp=<(native_libraries_java_stamp)',
          ],
        },
        {
          'variables': {
            'stripped_libraries_dir': '<(libraries_source_dir)',
            'input_paths': ['<@(native_libs_paths)'],
          },
          'includes': ['../build/android/strip_native_libraries.gypi'],
        },
      ],
      'conditions': [
        ['gyp_managed_install == 1', {
          'variables': {
            'libraries_source_dir': '<(intermediate_dir)/lib.stripped/<(android_app_abi)',
            'device_library_dir': '<(device_intermediate_dir)/lib.stripped',
          },
          'dependencies': [
            '<(DEPTH)/tools/android/md5sum/md5sum.gyp:md5sum',
          ],
          'actions': [
            {
              'includes': ['../build/android/push_libraries.gypi'],
            },
            {
              'action_name': 'create device library symlinks',
              'message': 'Creating links on device for <(_target_name).',
              'inputs': [
                '<(DEPTH)/build/android/gyp/util/build_utils.py',
                '<(DEPTH)/build/android/gyp/create_device_library_links.py',
                '<(apk_install_record)',
                '<(ordered_libraries_file)',
              ],
              'outputs': [
                '<(link_stamp)'
              ],
              'action': [
                'python', '<(DEPTH)/build/android/gyp/create_device_library_links.py',
                '--libraries-json=<(ordered_libraries_file)',
                '--script-host-path=<(symlink_script_host_path)',
                '--script-device-path=<(symlink_script_device_path)',
                '--target-dir=<(device_library_dir)',
                '--apk=<(final_apk_path)',
                '--stamp=<(link_stamp)',
              ],
            },
          ],
        }, {
          # gyp_managed_install != 1
          'variables': {
            'libraries_source_dir': '<(apk_package_native_libs_dir)/<(android_app_abi)',
            'package_input_paths': [ '<(strip_stamp)' ],
          },
        }],
      ],
    }], # native_lib_target != ''
    ['java_strings_grd != ""', {
      'variables': {
        'res_grit_dir': '<(SHARED_INTERMEDIATE_DIR)/<(package_name)_apk/res_grit',
        'additional_res_dirs': ['<(res_grit_dir)'],
        # grit_grd_file is used by grit_action.gypi, included below.
        'grit_grd_file': '<(java_in_dir)/strings/<(java_strings_grd)',
        'resource_input_paths': [
          '<!@pymod_do_main(grit_info <@(grit_defines) --outputs "<(res_grit_dir)" <(grit_grd_file))'
        ],
      },
      'actions': [
        {
          'action_name': 'generate_localized_strings_xml',
          'variables': {
            'grit_additional_defines': ['-E', 'ANDROID_JAVA_TAGGED_ONLY=false'],
            'grit_out_dir': '<(res_grit_dir)',
            # resource_ids is unneeded since we don't generate .h headers.
            'grit_resource_ids': '',
          },
          'includes': ['../build/grit_action.gypi'],
        },
      ],
    }],
    ['gyp_managed_install == 1', {
      'actions': [
        {
          'action_name': 'apk_install_<(_target_name)',
          'message': 'Installing <(apk_name).apk',
          'inputs': [
            '<(DEPTH)/build/android/gyp/util/build_utils.py',
            '<(DEPTH)/build/android/gyp/apk_install.py',
            '<(final_apk_path)',
          ],
          'outputs': [
            '<(apk_install_record)',
            # If a user switches the connected device, the APK may need to be
            # installed even if there have been no changes. To ensure that the
            # APK on the device is always up-to-date, this step should always
            # be triggered.
            '<(apk_install_record).fake',
          ],
          'action': [
            'python', '<(DEPTH)/build/android/gyp/apk_install.py',
            '--android-sdk-tools=<(android_sdk_tools)',
            '--apk-path=<(final_apk_path)',
            '--install-record=<(apk_install_record)'
          ],
        },
      ],
    }],
  ],
  'actions': [
    {
      'action_name': 'ant_codegen_<(_target_name)',
      'message': 'Generating R.java for <(_target_name)',
      'conditions': [
        ['is_test_apk == 1', {
          'variables': {
            'additional_res_dirs=': [],
            'additional_res_packages=': [],
          }
        }],
      ],
      'inputs': [
        '<(DEPTH)/build/android/ant/apk-codegen.xml',
        '<(DEPTH)/build/android/gyp/util/build_utils.py',
        '<(DEPTH)/build/android/gyp/ant.py',
        '<(android_manifest_path)',
        '>@(additional_input_paths)',
        '>@(codegen_input_paths)',
        '>@(library_manifest_paths)',
        '>@(resource_input_paths)',
      ],
      'outputs': [
        '<(codegen_stamp)',
      ],
      'action': [
        'python', '<(DEPTH)/build/android/gyp/ant.py',
        '-quiet',
        '-DADDITIONAL_RES_DIRS=>(additional_res_dirs)',
        '-DADDITIONAL_RES_PACKAGES=>(additional_res_packages)',
        '-DADDITIONAL_R_TEXT_FILES=>(additional_R_text_files)',
        '-DANDROID_MANIFEST=<(android_manifest_path)',
        '-DANDROID_SDK_JAR=<(android_sdk_jar)',
        '-DANDROID_SDK_ROOT=<(android_sdk_root)',
        '-DANDROID_SDK_VERSION=<(android_sdk_version)',
        '-DLIBRARY_MANIFEST_PATHS=>(library_manifest_paths)',
        '-DOUT_DIR=<(intermediate_dir)',
        '-DRESOURCE_DIR=<(resource_dir)',

        '-DSTAMP=<(codegen_stamp)',
        '-Dbasedir=.',
        '-buildfile',
        '<(DEPTH)/build/android/ant/apk-codegen.xml',

        # Add list of inputs to the command line, so if inputs change
        # (e.g. if a Java file is removed), the command will be re-run.
        # TODO(newt): remove this once crbug.com/177552 is fixed in ninja.
        '-DTHIS_IS_IGNORED=>!(echo \'>(_inputs)\' | md5sum)',
      ],
    },
    {
      'action_name': 'javac_<(_target_name)',
      'message': 'Compiling java for <(_target_name)',
      'variables': {
        'all_src_dirs': [
          '<(java_in_dir)/src',
          '<(intermediate_dir)/gen',
          '>@(additional_src_dirs)',
          '>@(generated_src_dirs)',
        ],
      },
      'inputs': [
        '<(DEPTH)/build/android/gyp/util/build_utils.py',
        '<(DEPTH)/build/android/gyp/javac.py',
        # If there is a separate find for additional_src_dirs, it will find the
        # wrong .java files when additional_src_dirs is empty.
        '>!@(find >(java_in_dir) >(additional_src_dirs) -name "*.java")',
        '>@(input_jars_paths)',
        '<(codegen_stamp)',
        '>@(compile_input_paths)',
      ],
      'outputs': [
        '<(compile_stamp)',
      ],
      'action': [
        'python', '<(DEPTH)/build/android/gyp/javac.py',
        '--output-dir=<(classes_dir)',
        '--classpath=>(input_jars_paths) <(android_sdk_jar)',
        '--src-dirs=>(all_src_dirs)',
        '--javac-includes=<(javac_includes)',
        '--chromium-code=<(chromium_code)',
        '--stamp=<(compile_stamp)',

        # TODO(newt): remove this once http://crbug.com/177552 is fixed in ninja.
        '--ignore=>!(echo \'>(_inputs)\' | md5sum)',
      ],
    },
    {
      'action_name': 'jar_<(_target_name)',
      'message': 'Creating <(_target_name) jar',
      'inputs': [
        '<(DEPTH)/build/android/gyp/util/build_utils.py',
        '<(DEPTH)/build/android/gyp/util/md5_check.py',
        '<(DEPTH)/build/android/gyp/jar.py',
        '<(compile_stamp)',
      ],
      'outputs': [
        '<(jar_stamp)',
      ],
      'action': [
        'python', '<(DEPTH)/build/android/gyp/jar.py',
        '--classes-dir=<(classes_dir)',
        '--jar-path=<(jar_path)',
        '--excluded-classes=<(jar_excluded_classes)',
        '--stamp=<(jar_stamp)',

        # TODO(newt): remove this once http://crbug.com/177552 is fixed in ninja.
        '--ignore=>!(echo \'>(_inputs)\' | md5sum)',
      ]
    },
    {
      'action_name': 'ant_obfuscate_<(_target_name)',
      'message': 'Obfuscating <(_target_name)',
      'inputs': [
        '<(DEPTH)/build/android/ant/apk-obfuscate.xml',
        '<(DEPTH)/build/android/ant/create-test-jar.js',
        '<(DEPTH)/build/android/gyp/util/build_utils.py',
        '<(DEPTH)/build/android/gyp/ant.py',
        '<(compile_stamp)',
        '>@(proguard_flags_paths)',
      ],
      'outputs': [
        '<(obfuscate_stamp)',
      ],
      'action': [
        'python', '<(DEPTH)/build/android/gyp/ant.py',
        '-quiet',
        '-DADDITIONAL_SRC_DIRS=>(additional_src_dirs)',
        '-DANDROID_SDK_JAR=<(android_sdk_jar)',
        '-DANDROID_SDK_ROOT=<(android_sdk_root)',
        '-DANDROID_SDK_VERSION=<(android_sdk_version)',
        '-DAPK_NAME=<(apk_name)',
        '-DCREATE_TEST_JAR_PATH=<(DEPTH)/build/android/ant/create-test-jar.js',
        '-DCONFIGURATION_NAME=<(CONFIGURATION_NAME)',
        '-DGENERATED_SRC_DIRS=>(generated_src_dirs)',
        '-DINPUT_JARS_PATHS=>(input_jars_paths)',
        '-DIS_TEST_APK=<(is_test_apk)',
        '-DJAR_PATH=<(PRODUCT_DIR)/lib.java/<(jar_name)',
        '-DOBFUSCATED_JAR_PATH=<(obfuscated_jar_path)',
        '-DOUT_DIR=<(intermediate_dir)',
        '-DPROGUARD_ENABLED=<(proguard_enabled)',
        '-DPROGUARD_FLAGS=<(proguard_flags_paths)',
        '-DTEST_JAR_PATH=<(PRODUCT_DIR)/test.lib.java/<(apk_name).jar',

        '-DSTAMP=<(obfuscate_stamp)',
        '-Dbasedir=.',
        '-buildfile',
        '<(DEPTH)/build/android/ant/apk-obfuscate.xml',

        # Add list of inputs to the command line, so if inputs change
        # (e.g. if a Java file is removed), the command will be re-run.
        # TODO(newt): remove this once crbug.com/177552 is fixed in ninja.
        '-DTHIS_IS_IGNORED=>!(echo \'>(_inputs)\' | md5sum)',
      ],
    },
    {
      'action_name': 'dex_<(_target_name)',
      'message': 'Dexing <(_target_name) jar',
      'variables': {
        'conditions': [
          ['proguard_enabled=="true" and CONFIGURATION_NAME=="Release"', {
            'dex_inputs': [ '<(obfuscated_jar_path)' ],
            'dex_generated_inputs': [],
          }, {
            'dex_inputs': [
              '>@(library_dexed_jars_paths)',
            ],
            'dex_generated_inputs': [
              '<(classes_dir)',
            ],
          }],
        ],
      },
      'inputs': [
        '<(DEPTH)/build/android/gyp/util/build_utils.py',
        '<(DEPTH)/build/android/gyp/util/md5_check.py',
        '<(DEPTH)/build/android/gyp/dex.py',
        '<(compile_stamp)',
        '>@(dex_inputs)',
      ],
      'outputs': [
        '<(dex_path)',
      ],
      'action': [
        'python', '<(DEPTH)/build/android/gyp/dex.py',
        '--dex-path=<(dex_path)',
        '--android-sdk-root=<(android_sdk_root)',

        # TODO(newt): remove this once http://crbug.com/177552 is fixed in ninja.
        '--ignore=>!(echo \'>(_inputs)\' | md5sum)',

        '>@(dex_inputs)',
        '>@(dex_generated_inputs)',
      ]
    },
    {
      'action_name': 'ant package resources',
      'message': 'Packaging resources for <(_target_name) APK.',
      'inputs': [
        '<(DEPTH)/build/android/ant/apk-package-resources.xml',
        '<(DEPTH)/build/android/gyp/util/build_utils.py',
        '<(DEPTH)/build/android/gyp/ant.py',
        '<(android_manifest_path)',
        '<(codegen_stamp)',

        '>@(library_manifest_paths)',
        '>@(additional_input_paths)',
      ],
      'conditions': [
        ['is_test_apk == 1', {
          'variables': {
            'additional_res_dirs=': [],
            'additional_res_packages=': [],
          }
        }],
      ],
      'outputs': [
        '<(package_resources_stamp)',
      ],
      'action': [
        'python', '<(DEPTH)/build/android/gyp/ant.py',
        '-quiet',
        '-DADDITIONAL_RES_DIRS=>(additional_res_dirs)',
        '-DADDITIONAL_RES_PACKAGES=>(additional_res_packages)',
        '-DADDITIONAL_R_TEXT_FILES=>(additional_R_text_files)',
        '-DANDROID_SDK_JAR=<(android_sdk_jar)',
        '-DANDROID_SDK_ROOT=<(android_sdk_root)',
        '-DAPK_NAME=<(apk_name)',
        '-DAPP_MANIFEST_VERSION_CODE=<(app_manifest_version_code)',
        '-DAPP_MANIFEST_VERSION_NAME=<(app_manifest_version_name)',
        '-DASSET_DIR=<(asset_location)',
        '-DCONFIGURATION_NAME=<(CONFIGURATION_NAME)',
        '-DOUT_DIR=<(intermediate_dir)',
        '-DRESOURCE_DIR=<(resource_dir)',

        '-DSTAMP=<(package_resources_stamp)',

        '-Dbasedir=.',
        '-buildfile',
        '<(DEPTH)/build/android/ant/apk-package-resources.xml',

        # Add list of inputs to the command line, so if inputs change
        # (e.g. if a Java file is removed), the command will be re-run.
        # TODO(newt): remove this once crbug.com/177552 is fixed in ninja.
        '-DTHIS_IS_IGNORED=>!(echo \'>(_inputs)\' | md5sum)',
      ]
    },
    {
      'action_name': 'ant_package_<(_target_name)',
      'message': 'Packaging <(_target_name).',
      'inputs': [
        '<(DEPTH)/build/android/ant/apk-package.xml',
        '<(DEPTH)/build/android/gyp/util/build_utils.py',
        '<(DEPTH)/build/android/gyp/ant.py',
        '<(dex_path)',
        '<(codegen_stamp)',
        '<(obfuscate_stamp)',
        '<(package_resources_stamp)',
        '>@(package_input_paths)',
      ],
      'outputs': [
        '<(unsigned_apk_path)',
      ],
      'action': [
        'python', '<(DEPTH)/build/android/gyp/ant.py',
        '-quiet',
        '-DANDROID_SDK_ROOT=<(android_sdk_root)',
        '-DAPK_NAME=<(apk_name)',
        '-DCONFIGURATION_NAME=<(CONFIGURATION_NAME)',
        '-DNATIVE_LIBS_DIR=<(apk_package_native_libs_dir)',
        '-DOUT_DIR=<(intermediate_dir)',
        '-DSOURCE_DIR=<(source_dir)',
        '-DUNSIGNED_APK_PATH=<(unsigned_apk_path)',

        '-Dbasedir=.',
        '-buildfile',
        '<(DEPTH)/build/android/ant/apk-package.xml',

        # Add list of inputs to the command line, so if inputs change
        # (e.g. if a Java file is removed), the command will be re-run.
        # TODO(newt): remove this once crbug.com/177552 is fixed in ninja.
        '-DTHIS_IS_IGNORED=>!(echo \'>(_inputs)\' | md5sum)',
      ]
    },
    {
      'action_name': 'finalize_apk',
      'message': 'Signing/aligning <(_target_name) APK.',
      'inputs': [
        '<(DEPTH)/build/android/gyp/util/build_utils.py',
        '<(DEPTH)/build/android/gyp/finalize_apk.py',
        '<(unsigned_apk_path)',
      ],
      'outputs': [
        '<(final_apk_path)',
      ],
      'action': [
        'python', '<(DEPTH)/build/android/gyp/finalize_apk.py',
        '--android-sdk-root=<(android_sdk_root)',
        '--unsigned-apk-path=<(unsigned_apk_path)',
        '--final-apk-path=<(final_apk_path)',
        '--keystore-path=<(keystore_path)',

        # TODO(newt): remove this once crbug.com/177552 is fixed in ninja.
        '--ignore=>!(echo \'>(_inputs)\' | md5sum)',
      ],
    },
  ],
}
