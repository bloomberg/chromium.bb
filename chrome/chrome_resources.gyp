# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'variables': {
    'grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)/chrome',
    'repack_locales_cmd': ['python', 'tools/build/repack_locales.py'],
  },
  'targets': [
    {
      'target_name': 'chrome_extra_resources',
      'type': 'none',
      'dependencies': [
        '../third_party/WebKit/Source/WebKit/chromium/WebKit.gyp:generate_devtools_grd',
      ],
      # These resources end up in resources.pak because they are resources
      # used by internal pages.  Putting them in a spearate pak file makes
      # it easier for us to reference them internally.
      'actions': [
        {
          'action_name': 'component_extension_resources',
          'variables': {
            'grit_grd_file': 'browser/resources/component_extension_resources.grd',
          },
          'includes': [ '../build/grit_action.gypi' ],
        },
        {
          'action_name': 'net_internals_resources',
          'variables': {
            'grit_grd_file': 'browser/resources/net_internals_resources.grd',
          },
          'includes': [ '../build/grit_action.gypi' ],
        },
        {
          'action_name': 'options_resources',
          'variables': {
            'grit_grd_file': 'browser/resources/options_resources.grd',
          },
          'includes': [ '../build/grit_action.gypi' ],
        },
        {
          'action_name': 'quota_internals_resources',
          'variables': {
            'grit_grd_file': 'browser/resources/quota_internals_resources.grd',
          },
          'includes': [ '../build/grit_action.gypi' ],
        },
        {
          'action_name': 'shared_resources',
          'variables': {
            'grit_grd_file': 'browser/resources/shared_resources.grd',
          },
          'includes': [ '../build/grit_action.gypi' ],
        },
        {
          'action_name': 'sync_internals_resources',
          'variables': {
            'grit_grd_file': 'browser/resources/sync_internals_resources.grd',
          },
          'includes': [ '../build/grit_action.gypi' ],
        },
        {
          'action_name': 'workers_resources',
          'variables': {
            'grit_grd_file': 'browser/resources/workers_resources.grd',
          },
          'includes': [ '../build/grit_action.gypi' ],
        },
        {
          'action_name': 'devtools_frontend_resources',
          'variables': {
            'grit_grd_file':
               'browser/debugger/frontend/devtools_frontend_resources.grd',
          },
          'includes': [ '../build/grit_action.gypi' ]
        },
        {
          'action_name': 'devtools_resources',
          # This can't use ../build/grit_action.gypi because the grd file
          # is generated a build time, so the trick of using grit_info to get
          # the real inputs/outputs at GYP time isn't possible.
          'variables': {
            'grit_cmd': ['python', '../tools/grit/grit.py'],
            'grit_grd_file': '<(SHARED_INTERMEDIATE_DIR)/devtools/devtools_resources.grd',
          },
          'inputs': [
            '<(grit_grd_file)',
            '<!@pymod_do_main(grit_info --inputs)',
          ],
          'outputs': [
            '<(grit_out_dir)/grit/devtools_resources.h',
            '<(grit_out_dir)/devtools_resources.pak',
            '<(grit_out_dir)/grit/devtools_resources_map.cc',
            '<(grit_out_dir)/grit/devtools_resources_map.h',
          ],
          'action': ['<@(grit_cmd)',
                     '-i', '<(grit_grd_file)', 'build',
                     '-o', '<(grit_out_dir)',
                     '-D', 'SHARED_INTERMEDIATE_DIR=<(SHARED_INTERMEDIATE_DIR)',
                     '<@(grit_defines)' ],
          'message': 'Generating resources from <(grit_grd_file)',
        },
      ],
      'includes': [ '../build/grit_target.gypi' ],
    },
    {
      # TODO(mark): It would be better if each static library that needed
      # to run grit would list its own .grd files, but unfortunately some
      # of the static libraries currently have circular dependencies among
      # generated headers.
      'target_name': 'chrome_resources',
      'type': 'none',
      'actions': [
        # Data resources.
        {
          'action_name': 'browser_resources',
          'variables': {
            'grit_grd_file': 'browser/browser_resources.grd',
          },
          'includes': [ '../build/grit_action.gypi' ],
        },
        {
          'action_name': 'common_resources',
          'variables': {
            'grit_grd_file': 'common/common_resources.grd',
          },
          'includes': [ '../build/grit_action.gypi' ],
        },
        {
          'action_name': 'renderer_resources',
          'variables': {
            'grit_grd_file': 'renderer/renderer_resources.grd',
          },
          'includes': [ '../build/grit_action.gypi' ],
        },
      ],
      'includes': [ '../build/grit_target.gypi' ],
    },
    {
      # TODO(mark): It would be better if each static library that needed
      # to run grit would list its own .grd files, but unfortunately some
      # of the static libraries currently have circular dependencies among
      # generated headers.
      'target_name': 'chrome_strings',
      'type': 'none',
      'conditions': [
        ['OS=="win"', {
          # HACK(nsylvain): We want to enforce a fake dependency on
          # intaller_util_string.  install_util depends on both
          # chrome_strings and installer_util_strings, but for some reasons
          # Incredibuild does not enforce it (most likely a bug). By changing
          # the type and making sure we depend on installer_util_strings, it
          # will always get built before installer_util.
          'type': 'dummy_executable',
          'dependencies': ['chrome.gyp:installer_util_strings'],
        }],
      ],
      'actions': [
        # Localizable resources.
        {
          'action_name': 'locale_settings',
          'variables': {
            'grit_grd_file': 'app/resources/locale_settings.grd',
          },
          'includes': [ '../build/grit_action.gypi' ],
        },
        {
          'action_name': 'chromium_strings.grd',
          'variables': {
            'grit_grd_file': 'app/chromium_strings.grd',
          },
          'includes': [ '../build/grit_action.gypi' ],
        },
        {
          'action_name': 'generated_resources',
          'variables': {
            'grit_grd_file': 'app/generated_resources.grd',
          },
          'includes': [ '../build/grit_action.gypi' ],
        },
        {
          'action_name': 'google_chrome_strings',
          'variables': {
            'grit_grd_file': 'app/google_chrome_strings.grd',
          },
          'includes': [ '../build/grit_action.gypi' ],
        },
      ],
      'includes': [ '../build/grit_target.gypi' ],
    },
    {
      'target_name': 'platform_locale_settings',
      'type': 'none',
      'variables': {
        'conditions': [
          ['OS=="win"', {
            'platform_locale_settings_grd':
                'app/resources/locale_settings_win.grd',
          },],
          ['OS=="linux"', {
            'conditions': [
              ['chromeos==1', {
                'platform_locale_settings_grd':
                    'app/resources/locale_settings_cros.grd',
              }],
              ['chromeos!=1', {
                'platform_locale_settings_grd':
                    'app/resources/locale_settings_linux.grd',
              }],
            ],
          },],
          ['os_posix == 1 and OS != "mac" and OS != "linux"', {
            'platform_locale_settings_grd':
                'app/resources/locale_settings_linux.grd',
          },],
          ['OS=="mac"', {
            'platform_locale_settings_grd':
                'app/resources/locale_settings_mac.grd',
          }],
        ],  # conditions
      },  # variables
      'actions': [
        {
          'action_name': 'platform_locale_settings',
          'variables': {
            'grit_grd_file': '<(platform_locale_settings_grd)',
          },
          'includes': [ '../build/grit_action.gypi' ],
        },
      ],
      'includes': [ '../build/grit_target.gypi' ],
    },
    {
      'target_name': 'theme_resources',
      'type': 'none',
      'actions': [
        {
          'action_name': 'theme_resources',
          'variables': {
            'grit_grd_file': 'app/theme/theme_resources.grd',
          },
          'includes': [ '../build/grit_action.gypi' ],
        },
        {
          'action_name': 'theme_resources_large',
          'variables': {
            'grit_grd_file': 'app/theme/theme_resources_large.grd',
          },
          'includes': [ '../build/grit_action.gypi' ],
        },
        {
          'action_name': 'theme_resources_standard',
          'variables': {
            'grit_grd_file': 'app/theme/theme_resources_standard.grd',
          },
          'includes': [ '../build/grit_action.gypi' ],
        },
      ],
      'includes': [ '../build/grit_target.gypi' ],
    },
    {
      'target_name': 'packed_extra_resources',
      'type': 'none',
      'variables': {
        'repack_path': '../tools/grit/grit/format/repack.py',
      },
      'dependencies': [
        'chrome_extra_resources',
      ],
      'actions': [
        {
          'includes': ['chrome_repack_resources.gypi']
        },
      ],
      'conditions': [
        ['OS != "mac"', {
          # We'll install the resource files to the product directory.  The Mac
          # copies the results over as bundle resources in its own special way.
          'copies': [
            {
              'destination': '<(PRODUCT_DIR)',
              'files': [
                '<(SHARED_INTERMEDIATE_DIR)/repack/resources.pak'
              ],
            },
          ],
        }]
      ]
    },
    {
      'target_name': 'packed_resources',
      'type': 'none',
      'variables': {
        'repack_path': '../tools/grit/grit/format/repack.py',
      },
      'dependencies': [
        # MSVS needs the dependencies explictly named, Make is able to
        # derive the dependencies from the output files.
        'chrome_resources',
        'chrome_strings',
        'default_plugin/default_plugin.gyp:default_plugin_resources',
        'platform_locale_settings',
        'theme_resources',
        '<(DEPTH)/content/content_resources.gyp:content_resources',
        '<(DEPTH)/net/net.gyp:net_resources',
        '<(DEPTH)/ui/base/strings/ui_strings.gyp:ui_strings',
        '<(DEPTH)/ui/ui.gyp:gfx_resources',
        '<(DEPTH)/ui/ui.gyp:ui_resources',
        '<(DEPTH)/ui/ui.gyp:ui_resources_large',
        '<(DEPTH)/ui/ui.gyp:ui_resources_standard',
        '<(DEPTH)/webkit/support/webkit_support.gyp:webkit_resources',
        '<(DEPTH)/webkit/support/webkit_support.gyp:webkit_strings',
      ],
      'actions': [
        {
          'includes': ['chrome_repack_chrome.gypi']
        },
        {
          'includes': ['chrome_repack_locales.gypi']
        },
        {
          'includes': ['chrome_repack_pseudo_locales.gypi']
        },
      ],
      'conditions': [
        ['OS != "mac"', {
          # We'll install the resource files to the product directory.  The Mac
          # copies the results over as bundle resources in its own special way.
          'copies': [
            {
              'destination': '<(PRODUCT_DIR)',
              'files': [
                '<(SHARED_INTERMEDIATE_DIR)/repack/chrome.pak'
              ],
            },
            {
              'destination': '<(PRODUCT_DIR)/locales',
              'files': [
                '<!@pymod_do_main(repack_locales -o -g <(grit_out_dir) -s <(SHARED_INTERMEDIATE_DIR) -x <(SHARED_INTERMEDIATE_DIR) <(locales))'
              ],
            },
            {
              'destination': '<(PRODUCT_DIR)/pseudo_locales',
              'files': [
                '<!@pymod_do_main(repack_locales -o -g <(grit_out_dir) -s <(SHARED_INTERMEDIATE_DIR) -x <(SHARED_INTERMEDIATE_DIR) <(pseudo_locales))'
              ],
            },
          ],
          'conditions': [
            ['branding=="Chrome"', {
              'copies': [
                {
                  # This location is for the Windows and Linux builds. For
                  # Windows, the chrome.release file ensures that these files
                  # are copied into the installer. Note that we have a separate
                  # section in chrome_dll.gyp to copy these files for Mac, as it
                  # needs to be dropped inside the framework.
                  'destination': '<(PRODUCT_DIR)/default_apps',
                  'files': ['<@(default_apps_list)']
                },
              ],
            }],
          ], # conditions
        }], # end OS != "mac"
      ], # conditions
    },
  ], # targets
}
