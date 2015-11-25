# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'appearance_fonts_page',
      'variables': {
        'depends': [
          '../../../../../ui/webui/resources/js/compiled_resources.gyp:assert',
          '../../../../../ui/webui/resources/js/compiled_resources.gyp:load_time_data',
          '../../../../../ui/webui/resources/js/compiled_resources.gyp:cr',
          '../../../../../ui/webui/resources/js/cr.js',
          '../../../../../ui/webui/resources/js/i18n_behavior.js',
          '../../../../../third_party/polymer/v1_0/components-chromium/iron-a11y-keys-behavior/iron-a11y-keys-behavior-extracted.js',
          '../../../../../third_party/polymer/v1_0/components-chromium/iron-behaviors/iron-button-state-extracted.js',
          '../../../../../third_party/polymer/v1_0/components-chromium/iron-behaviors/iron-control-state-extracted.js',
          '../../../../../third_party/polymer/v1_0/components-chromium/iron-form-element-behavior/iron-form-element-behavior-extracted.js',
          '../../../../../third_party/polymer/v1_0/components-chromium/iron-range-behavior/iron-range-behavior-extracted.js',
          '../../../../../third_party/polymer/v1_0/components-chromium/paper-behaviors/paper-inky-focus-behavior-extracted.js',
          '../../../../../third_party/polymer/v1_0/components-chromium/paper-behaviors/paper-ripple-behavior-extracted.js',
          '../../../../../third_party/polymer/v1_0/components-chromium/paper-ripple/paper-ripple-extracted.js',
          '../../../../../third_party/polymer/v1_0/components-chromium/paper-slider/paper-slider-extracted.js',
          '../controls/settings_dropdown_menu.js',
          '../prefs/pref_util.js'
        ],
        'externs': [
          '../../../../../third_party/closure_compiler/externs/settings_private.js'
        ],
      },
      'includes': ['../../../../../third_party/closure_compiler/compile_js.gypi'],
    },
    {
      'target_name': 'appearance_page',
      'variables': {
        'depends': [
          '../../../../../ui/webui/resources/js/compiled_resources.gyp:assert',
          '../../../../../ui/webui/resources/js/compiled_resources.gyp:load_time_data',
          '../../../../../ui/webui/resources/js/cr.js',
          '../../../../../ui/webui/resources/js/i18n_behavior.js',
          '../controls/settings_dropdown_menu.js',
          '../prefs/pref_util.js',
          '../settings_page/settings_animated_pages.js'
        ],
        'externs': [
          '../../../../../third_party/closure_compiler/externs/settings_private.js',
          '../../../../../third_party/closure_compiler/externs/chrome_send.js',
        ],
      },
      'includes': ['../../../../../third_party/closure_compiler/compile_js.gypi'],
    },
  ],
}
