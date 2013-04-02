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
#  native_libs_paths - The path to any native library to be included in this
#    target. This should be a path in <(SHARED_LIB_DIR). A stripped copy of
#    the library will be included in the apk.
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
    'proguard_flags_path%': '<(DEPTH)/build/android/empty_proguard.flags',
    'native_libs_paths': [],
    'jar_name': 'chromium_apk_<(_target_name).jar',
    'resource_dir%':'<(DEPTH)/build/android/ant/empty/res',
    'R_package%':'',
    'additional_res_dirs': [],
    'additional_res_packages': [],
    'is_test_apk%': 0,
    'java_strings_grd%': '',
    'library_manifest_paths' : [],
    'resource_input_paths': [],
    'intermediate_dir': '<(PRODUCT_DIR)/<(_target_name)',
    'asset_location%': '<(intermediate_dir)/assets',
    'codegen_stamp': '<(intermediate_dir)/codegen.stamp',
    'compile_input_paths': [ ],
    'package_input_paths': [ ],
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
    'android_manifest': '<(java_in_dir)/AndroidManifest.xml',
    'codegen_input_paths': [],
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
    ['native_libs_paths != []', {
      'variables': {
        'compile_input_paths': [ '<(native_libraries_java_stamp)' ],
        'generated_src_dirs': [ '<(native_libraries_java_dir)' ],
        'package_input_paths': [ '<(strip_stamp)' ],
      },
      'actions': [
        {
          'action_name': 'ordered_libraries_<(_target_name)',
          'message': 'Writing dependency ordered libraries for <(_target_name).',
          'inputs': [
            '<(DEPTH)/build/android/pylib/build_utils.py',
            '<(DEPTH)/build/android/write_ordered_libraries.py',
            '<@(native_libs_paths)',
          ],
          'outputs': [
            '<(ordered_libraries_file)',
          ],
          'action': [
            'python', '<(DEPTH)/build/android/write_ordered_libraries.py',
            '--input-libraries=<(native_libs_paths)',
            '--output=<(ordered_libraries_file)',
          ],
        },
        {
          'action_name': 'native_libraries_template_data_<(_target_name)',
          'message': 'Creating native_libraries_list.h for <(_target_name).',
          'inputs': [
            '<(DEPTH)/build/android/pylib/build_utils.py',
            '<(DEPTH)/build/android/create_native_libraries_header.py',
            '<(ordered_libraries_file)',
          ],
          'outputs': [
            '<(native_libraries_template_data_stamp)',
          ],
          'action': [
            'python', '<(DEPTH)/build/android/create_native_libraries_header.py',
            '--ordered-libraries=<(ordered_libraries_file)',
            '--output=<(native_libraries_template_data_file)',
            '--stamp=<(native_libraries_template_data_stamp)',
          ],
        },
        {
          'action_name': 'native_libraries_<(_target_name)',
          'message': 'Creating NativeLibraries.java for <(_target_name).',
          'inputs': [
            '<(DEPTH)/build/android/pylib/build_utils.py',
            '<(DEPTH)/build/android/gcc_preprocess.py',
            '<(native_libraries_template_data_stamp)',
            '<(native_libraries_template)',
          ],
          'outputs': [
            '<(native_libraries_java_stamp)',
          ],
          'action': [
            'python', '<(DEPTH)/build/android/gcc_preprocess.py',
            '--include-path=<(native_libraries_template_data_dir)',
            '--output=<(native_libraries_java_file)',
            '--template=<(native_libraries_template)',
            '--stamp=<(native_libraries_java_stamp)',
          ],
        },
        {
          'action_name': 'strip_native_libraries',
          'message': 'Stripping libraries for <(_target_name)',
          'variables': {
            'apk_libraries_dir': '<(intermediate_dir)/libs/<(android_app_abi)',
          },
          'inputs': [
            '<(DEPTH)/build/android/pylib/build_utils.py',
            '<(DEPTH)/build/android/strip_library_for_apk.py',
            '<(ordered_libraries_file)'
          ],
          'outputs': [
            '<(strip_stamp)',
          ],
          'action': [
            'python', '<(DEPTH)/build/android/strip_library_for_apk.py',
            '--android-strip=<(android_strip)',
            '--android-strip-arg=--strip-unneeded',
            '--stripped-libraries-dir=<(apk_libraries_dir)',
            '--libraries-dir=<(SHARED_LIB_DIR)',
            '--libraries-file=<(ordered_libraries_file)',
            '--stamp=<(strip_stamp)',
          ],
        },
      ],
    }], # native_libs_paths != []
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
        '<(android_manifest)',
        '>@(library_manifest_paths)'
        '>@(codegen_input_paths)',
        '>@(additional_input_paths)',
      ],
      'outputs': [
        '<(codegen_stamp)',
      ],
      'action': [
        'ant', '-quiet',
        '-DADDITIONAL_RES_DIRS=>(additional_res_dirs)',
        '-DADDITIONAL_RES_PACKAGES=>(additional_res_packages)',
        '-DADDITIONAL_R_TEXT_FILES=>(additional_R_text_files)',
        '-DANDROID_MANIFEST=<(android_manifest)',
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
        '<(DEPTH)/build/android/pylib/build_utils.py',
        '<(DEPTH)/build/android/javac.py',
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
        'python', '<(DEPTH)/build/android/javac.py',
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
        '<(DEPTH)/build/android/pylib/build_utils.py',
        '<(DEPTH)/build/android/jar.py',
        '<(compile_stamp)',
      ],
      'outputs': [
        '<(jar_stamp)',
      ],
      'action': [
        'python', '<(DEPTH)/build/android/jar.py',
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
        '<(compile_stamp)',
        '<(proguard_flags_path)',
      ],
      'outputs': [
        '<(obfuscate_stamp)',
      ],
      'action': [
        'ant', '-quiet',
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
        '-DPROGUARD_FLAGS=<(proguard_flags_path)',
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
        '<(DEPTH)/build/android/pylib/build_utils.py',
        '<(DEPTH)/build/android/dex.py',
        '<(compile_stamp)',
        '>@(dex_inputs)',
      ],
      'outputs': [
        '<(dex_path)',
      ],
      'action': [
        'python', '<(DEPTH)/build/android/dex.py',
        '--dex-path=<(dex_path)',
        '--android-sdk-root=<(android_sdk_root)',

        # TODO(newt): remove this once http://crbug.com/177552 is fixed in ninja.
        '--ignore=>!(echo \'>(_inputs)\' | md5sum)',

        '>@(dex_inputs)',
        '>@(dex_generated_inputs)',
      ]
    },
    {
      'action_name': 'ant_package_<(_target_name)',
      'message': 'Packaging <(_target_name).',
      'inputs': [
        '<(DEPTH)/build/android/ant/apk-package.xml',
        '<(dex_path)',
        '<(codegen_stamp)',
        '<(obfuscate_stamp)',
        '>@(package_input_paths)',
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
        '<(PRODUCT_DIR)/apks/<(apk_name).apk',
      ],
      'action': [
        'ant', '-quiet',
        '-DADDITIONAL_RES_DIRS=>(additional_res_dirs)',
        '-DADDITIONAL_RES_PACKAGES=>(additional_res_packages)',
        '-DADDITIONAL_R_TEXT_FILES=>(additional_R_text_files)',
        '-DANDROID_SDK_JAR=<(android_sdk_jar)',
        '-DANDROID_SDK_ROOT=<(android_sdk_root)',
        '-DANDROID_SDK_VERSION=<(android_sdk_version)',
        '-DAPKS_DIR=<(PRODUCT_DIR)/apks',
        '-DAPK_NAME=<(apk_name)',
        '-DAPP_MANIFEST_VERSION_CODE=<(app_manifest_version_code)',
        '-DAPP_MANIFEST_VERSION_NAME=<(app_manifest_version_name)',
        '-DASSET_DIR=<(asset_location)',
        '-DCONFIGURATION_NAME=<(CONFIGURATION_NAME)',
        '-DKEYSTORE_PATH=<(DEPTH)/build/android/ant/chromium-debug.keystore',
        '-DOUT_DIR=<(intermediate_dir)',
        '-DRESOURCE_DIR=<(resource_dir)',
        '-DSOURCE_DIR=<(java_in_dir)/src',

        '-Dbasedir=.',
        '-buildfile',
        '<(DEPTH)/build/android/ant/apk-package.xml',

        # Add list of inputs to the command line, so if inputs change
        # (e.g. if a Java file is removed), the command will be re-run.
        # TODO(newt): remove this once crbug.com/177552 is fixed in ninja.
        '-DTHIS_IS_IGNORED=>!(echo \'>(_inputs)\' | md5sum)',

      ]
    },
  ],
}
