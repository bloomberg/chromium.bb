# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'conditions': [
    ['chromeos==1', {
      'variables': {
        # Whether to compress the 4 main ChromeVox scripts.
        'chromevox_compress_js%': '1',
        'chromevox1_background_script_loader_file': 'chromevox/background/loader.js',
        'chromevox1_content_script_loader_file': 'chromevox/injected/loader.js',
        'chromevox1_kbexplorer_loader_file': 'chromevox/background/kbexplorer_loader.js',
        'chromevox1_options_script_loader_file': 'chromevox/background/options_loader.js',
        'chromevox2_background_script_loader_file': 'cvox2/background/loader.js',
      },
      'includes': [
        'chromevox_tests.gypi',
        'common.gypi',
      ],
      'targets': [
        {
          'target_name': 'chromevox',
          'type': 'none',
          'dependencies': [
            'chromevox_resources',
            'chromevox_manifest',
            'chromevox_guest_manifest',
          ],
        },
        {
          'target_name': 'chromevox_resources',
          'type': 'none',
          'dependencies': [
            'chromevox_assets',
            'chromevox_static_files',
            'chromevox_strings',
            'chromevox_uncompiled_js_files',
            '<(chromevox_third_party_dir)/chromevox.gyp:chromevox_third_party_resources',
            '../braille_ime/braille_ime.gyp:braille_ime_manifest',
          ],
          'conditions': [
            ['disable_nacl==0 and disable_nacl_untrusted==0', {
              'dependencies': [
                '<(DEPTH)/third_party/liblouis/liblouis_nacl.gyp:liblouis_nacl_wrapper_nacl',
              ],
            }],
            ['chromevox_compress_js==1', {
              'dependencies': [
                'chromevox1_background_script',
                'chromevox1_content_script',
                'chromevox1_kbexplorer_script',
                'chromevox1_options_script',
                'chromevox2_background_script',
              ],
            }, {  # chromevox_compress_js==0
              'dependencies': [
                'chromevox_copied_scripts',
                'chromevox_deps',
              ],
            }],
          ],
        },
        {
          'target_name': 'chromevox_assets',
          'type': 'none',
          'includes': [
            'chromevox_assets.gypi',
          ],
        },
        {
          'target_name': 'chromevox_static_files',
          'type': 'none',
          'copies': [
            {
              'destination': '<(chromevox_dest_dir)/chromevox/background',
              'files': [
                'chromevox/background/background.html',
                'chromevox/background/kbexplorer.html',
                'chromevox/background/options.html',
              ],
            },
            {
              'destination': '<(PRODUCT_DIR)/resources/chromeos/chromevox/cvox2/background',
              'files': [
                'cvox2/background/background.html',
              ],
            },
          ],
        },
        {
          # JavaScript files that are always directly included into the
          # destination directory.
          'target_name': 'chromevox_uncompiled_js_files',
          'type': 'none',
          'copies': [
            {
              'destination': '<(chromevox_dest_dir)/chromevox/injected',
              'files': [
                'chromevox/injected/api.js',
              ],
              'conditions': [
                [ 'chromevox_compress_js==1', {
                  'files': [
                    # api_util.js is copied by the chromevox_copied_scripts
                    # target in the non-compressed case.
                    'chromevox/injected/api_util.js',
                  ],
                }],
              ],
            },
          ],
          'conditions': [
            [ 'chromevox_compress_js==0', {
              'copies': [
                {
                  'destination': '<(chromevox_dest_dir)/closure',
                  'files': [
                    'closure/closure_preinit.js',
                  ],
                },
              ],
            }],
          ],
        },
        {
          'target_name': 'chromevox_strings',
          'type': 'none',
          'actions': [
            {
              'action_name': 'chromevox_strings',
              'variables': {
                'grit_grd_file': 'strings/chromevox_strings.grd',
                'grit_out_dir': '<(chromevox_dest_dir)',
                # We don't generate any RC files, so no resource_ds file is needed.
                'grit_resource_ids': '',
              },
              'includes': [ '../../../../../build/grit_action.gypi' ],
            },
          ],
        },
        {
          'target_name': 'chromevox_deps',
          'type': 'none',
          'variables': {
            'deps_js_output_file': '<(chromevox_dest_dir)/deps.js',
          },
          'sources': [
            '<(chromevox1_background_script_loader_file)',
            '<(chromevox1_content_script_loader_file)',
            '<(chromevox1_kbexplorer_loader_file)',
            '<(chromevox1_options_script_loader_file)',
            '<(chromevox2_background_script_loader_file)',
          ],
          'includes': ['generate_deps.gypi'],
        },
        {
          'target_name': 'chromevox_manifest',
          'type': 'none',
          'variables': {
            'output_manifest_path': '<(chromevox_dest_dir)/manifest.json',
          },
          'includes': [ 'generate_manifest.gypi', ],
        },
        {
          'target_name': 'chromevox_guest_manifest',
          'type': 'none',
          'variables': {
            'output_manifest_path': '<(chromevox_dest_dir)/manifest_guest.json',
            'is_guest_manifest': 1,
          },
          'includes': [ 'generate_manifest.gypi', ],
        },
      ],
      'conditions': [
        ['chromevox_compress_js==1', {
          'targets': [
            {
              'target_name': 'chromevox1_content_script',
              'type': 'none',
              'variables': {
                'output_file': '<(chromevox_dest_dir)/chromeVoxChromePageScript.js',
              },
              'sources': [ '<(chromevox1_content_script_loader_file)' ],
              'includes': [ 'compress_js.gypi', ],
            },
            {
              'target_name': 'chromevox1_background_script',
              'type': 'none',
              'variables': {
                'output_file': '<(chromevox_dest_dir)/chromeVoxChromeBackgroundScript.js',
              },
              'sources': [ '<(chromevox1_background_script_loader_file)' ],
              'includes': [ 'compress_js.gypi', ],
            },
            {
              'target_name': 'chromevox1_options_script',
              'type': 'none',
              'variables': {
                'output_file': '<(chromevox_dest_dir)/chromeVoxChromeOptionsScript.js',
              },
              'sources': [ '<(chromevox1_options_script_loader_file)' ],
              'includes': [ 'compress_js.gypi', ],
            },
            {
              'target_name': 'chromevox1_kbexplorer_script',
              'type': 'none',
              'variables': {
                'output_file': '<(chromevox_dest_dir)/chromeVoxKbExplorerScript.js',
              },
              'sources': [ '<(chromevox1_kbexplorer_loader_file)' ],
              'includes': [ 'compress_js.gypi', ],
            },
            {
              'target_name': 'chromevox2_background_script',
              'type': 'none',
              'variables': {
                'output_file': '<(chromevox_dest_dir)/chromeVox2ChromeBackgroundScript.js',
              },
              'sources': [
                '<(chromevox1_background_script_loader_file)',
                '<(chromevox2_background_script_loader_file)',
              ],
              'includes': [ 'compress_js.gypi', ],
            },
          ],
        }, {  # chromevox_compress_js==0
          'targets': [
            {
              'target_name': 'chromevox_copied_scripts',
              'type': 'none',
              'variables': {
                'dest_dir': '<(chromevox_dest_dir)',
              },
              'sources': [
                '<(chromevox1_background_script_loader_file)',
                '<(chromevox1_content_script_loader_file)',
                '<(chromevox1_kbexplorer_loader_file)',
                '<(chromevox1_options_script_loader_file)',
                '<(chromevox2_background_script_loader_file)',
              ],
              'includes': [ 'copy_js.gypi', ],
            },
          ],
        }],
      ],
    }],
  ],
}
