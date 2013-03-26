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
#     'resource_dir': 'res',
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
#  asset_location - The directory where assets are located (default:
#    <(ant_build_out)/<(_target_name)/assets).
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
    'asset_location%': '',
    'additional_input_paths': [],
    'input_jars_paths': [],
    'additional_src_dirs': [],
    'generated_src_dirs': [],
    'app_manifest_version_name%': '<(android_app_version_name)',
    'app_manifest_version_code%': '<(android_app_version_code)',
    'proguard_enabled%': 'false',
    'proguard_flags%': '',
    'native_libs_paths': [],
    'jar_name%': 'chromium_apk_<(_target_name).jar',
    'resource_dir%':'',
    'R_package%':'',
    'additional_res_dirs': [],
    'additional_res_packages': [],
    'is_test_apk%': 0,
    'java_strings_grd%': '',
    'res_grit_files': [],
    'library_manifest_paths%' : [],
  },
  'sources': [
      '<@(native_libs_paths)'
  ],
  # Pass the jar path to the apk's "fake" jar target.  This would be better as
  # direct_dependent_settings, but a variable set by a direct_dependent_settings
  # cannot be lifted in a dependent to all_dependent_settings.
  'all_dependent_settings': {
    'variables': {
      'apk_output_jar_path': '<(PRODUCT_DIR)/lib.java/<(jar_name)',
    },
  },
  'rules': [
    {
      'rule_name': 'copy_and_strip_native_libraries',
      'extension': 'so',
      'variables': {
        'apk_libraries_dir': '<(PRODUCT_DIR)/<(_target_name)/libs/<(android_app_abi)',
        'stripped_library_path': '<(apk_libraries_dir)/<(RULE_INPUT_ROOT).so',
      },
      'inputs': [
        '<(DEPTH)/build/android/strip_library_for_apk.py',
      ],
      'outputs': [
        '<(stripped_library_path)',
      ],
      'action': [
        'python', '<(DEPTH)/build/android/strip_library_for_apk.py',
        '--android-strip=<(android_strip)',
        '--android-strip-arg=--strip-unneeded',
        '--stripped-libraries-dir=<(apk_libraries_dir)',
        '<(RULE_INPUT_PATH)',
      ],
    },
  ],
  'conditions': [
    ['R_package != ""', {
      'variables': {
        # We generate R.java in package R_package (in addition to the package
        # listed in the AndroidManifest.xml, which is unavoidable).
        'additional_res_dirs': ['<(DEPTH)/build/android/ant/empty/res'],
        'additional_res_packages': ['<(R_package)'],
        'additional_R_text_files': ['<(PRODUCT_DIR)/<(package_name)/R.txt'],
      },
    }],
    ['java_strings_grd != ""', {
      'variables': {
        'res_grit_dir': '<(SHARED_INTERMEDIATE_DIR)/<(package_name)_apk/res_grit',
        'additional_res_dirs': ['<(res_grit_dir)'],
        # grit_grd_file is used by grit_action.gypi, included below.
        'grit_grd_file': '<(java_in_dir)/strings/<(java_strings_grd)',
        'res_grit_files': ['<!@pymod_do_main(grit_info <@(grit_defines) --outputs "<(res_grit_dir)" <(grit_grd_file))'],
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
      'action_name': 'ant_<(_target_name)',
      'message': 'Building <(_target_name).',
      'inputs': [
        '<(java_in_dir)/AndroidManifest.xml',
        '<(DEPTH)/build/android/ant/chromium-apk.xml',
        '<(DEPTH)/build/android/ant/common.xml',
        '<(DEPTH)/build/android/ant/apk-build.xml',
        # If there is a separate find for additional_src_dirs, it will find the
        # wrong .java files when additional_src_dirs is empty.
        '>!@(find >(java_in_dir) >(additional_src_dirs) -name "*.java")',
        '>@(input_jars_paths)',
        '>@(native_libs_paths)',
        '>@(additional_input_paths)',
        '>@(library_manifest_paths)',
        '<@(res_grit_files)',
      ],
      'conditions': [
        ['resource_dir!=""', {
          'inputs': ['<!@(find <(java_in_dir)/<(resource_dir) -name "*")']
        }],
        ['is_test_apk == 1', {
          'variables': {
            'additional_res_dirs=': [],
            'additional_res_packages=': [],
          }
        }],
        ['proguard_enabled == "true" and proguard_flags != ""', {
          'inputs': ['<(java_in_dir)/<(proguard_flags)']
        }],
      ],
      'outputs': [
        '<(PRODUCT_DIR)/apks/<(apk_name).apk',
      ],
      'action': [
        'ant', '-quiet',
        '-DAPP_ABI=<(android_app_abi)',
        '-DANDROID_GDBSERVER=<(android_gdbserver)',
        '-DANDROID_SDK=<(android_sdk)',
        '-DANDROID_SDK_ROOT=<(android_sdk_root)',
        '-DANDROID_SDK_TOOLS=<(android_sdk_tools)',
        '-DANDROID_SDK_VERSION=<(android_sdk_version)',
        '-DANDROID_TOOLCHAIN=<(android_toolchain)',
        '-DCHROMIUM_SRC=<(ant_build_out)/../..',
        '-DCONFIGURATION_NAME=<(CONFIGURATION_NAME)',
        '-DPRODUCT_DIR=<(ant_build_out)',

        '-DAPK_NAME=<(apk_name)',
        '-DASSET_DIR=<(asset_location)',
        '-DADDITIONAL_SRC_DIRS=>(additional_src_dirs)',
        '-DGENERATED_SRC_DIRS=>(generated_src_dirs)',
        '-DINPUT_JARS_PATHS=>(input_jars_paths)',
        '-DJAR_NAME=<(jar_name)',
        '-DOUT_DIR=<(ant_build_out)/<(_target_name)',
        '-DRESOURCE_DIR=<(resource_dir)',
        '-DADDITIONAL_R_TEXT_FILES=>(additional_R_text_files)',
        '-DADDITIONAL_RES_DIRS=>(additional_res_dirs)',
        '-DADDITIONAL_RES_PACKAGES=>(additional_res_packages)',
        '-DAPP_MANIFEST_VERSION_NAME=<(app_manifest_version_name)',
        '-DAPP_MANIFEST_VERSION_CODE=<(app_manifest_version_code)',
        '-DPROGUARD_FLAGS=>(proguard_flags)',
        '-DPROGUARD_ENABLED=>(proguard_enabled)',
        '-DLIBRARY_MANIFEST_PATHS=>(library_manifest_paths)',

        # Add list of inputs to the command line, so if inputs change
        # (e.g. if a Java file is removed), the command will be re-run.
        # TODO(newt): remove this once crbug.com/177552 is fixed in ninja.
        '-DTHIS_IS_IGNORED=>(_inputs)',

        '-Dbasedir=<(java_in_dir)',
        '-buildfile',
        '<(DEPTH)/build/android/ant/chromium-apk.xml',

        # Specify CONFIGURATION_NAME as the target for ant to build. The
        # buildfile will then build the appropriate SDK tools target.
        '<(CONFIGURATION_NAME)',
      ]
    },
  ],
}
