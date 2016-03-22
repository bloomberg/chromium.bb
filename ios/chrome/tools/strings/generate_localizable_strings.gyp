# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Builds an OS X command line tool to generate Localizable.strings and
# InfoPlist.strings from compiled locales.pak.

{
  'conditions': [
    ['OS!="ios" or "<(GENERATOR)"!="xcode" or "<(GENERATOR_FLAVOR)"=="ninja"', {
      'targets': [
        {
          'target_name': 'generate_localizable_strings',
          'type': 'executable',
          'toolsets': ['host'],
          'dependencies': [
            '../../../../base/base.gyp:base',
            '../../../../ui/base/ui_base.gyp:ui_data_pack',
          ],
          'sources': [
            'generate_localizable_strings.mm',
          ],
          'libraries': [
            '$(SDKROOT)/System/Library/Frameworks/Foundation.framework',
          ],
        },
      ],
    }, {  # else, OS=="ios" and "<(GENERATOR)"=="xcode" and "<(GENERATOR_FLAVOR)"!="ninja"
      # Generation is done via two actions: (1) compiling the executable with
      # ninja, and (2) copying the executable into a location that is shared
      # with other projects. These actions are separated into two targets in
      # order to be able to specify that the second action should not run until
      # the first action finishes (since the ordering of multiple actions in
      # one target is defined only by inputs and outputs, and it's impossible
      # to set correct inputs for the ninja build, so setting all the inputs
      # and outputs isn't an option).
      'variables': {
        # Hardcode the ninja_product_dir variable to avoid having to include
        # mac_build.gypi, as otherwise the re-generation of the project will
        # happen twice while running gyp. It's a bit fragile, but if it gets
        # out of sync we'll know as soon as anyone does a clobber build
        # because it won't find the input.
        'ninja_output_dir': 'ninja-localizable_string_tool',
        'ninja_product_dir':
          '../../../../xcodebuild/<(ninja_output_dir)/<(CONFIGURATION_NAME)',
        # Gyp to rerun
        're_run_targets': [
          'ios/chrome/tools/strings/generate_localizable_strings.gyp',
        ],
      },
      'targets': [
        {
          'target_name': 'compile_generate_localizable_strings',
          'type': 'none',
          'includes': ['../../../../build/ios/mac_build.gypi'],
          'actions': [
            {
              'action_name': 'compile generate_localizable_strings',
              'inputs': [],
              'outputs': [],
              'action': [
                '<@(ninja_cmd)',
                'generate_localizable_strings',
              ],
              'message':
                'Generating the generate_localizable_strings executable',
            },
          ],
        },
        {
          'target_name': 'generate_localizable_strings',
          'type': 'none',
          'dependencies': [
            'compile_generate_localizable_strings',
          ],
          'actions': [
            {
              'action_name': 'copy generate_localizable_strings',
              'inputs': [
                '<(ninja_product_dir)/generate_localizable_strings',
              ],
              'outputs': [
                '<(PRODUCT_DIR)/generate_localizable_strings',
              ],
              'action': [
                'cp',
                '<(ninja_product_dir)/generate_localizable_strings',
                '<(PRODUCT_DIR)/generate_localizable_strings',
              ],
            },
          ],
        },
      ],
    }],
  ],
}
