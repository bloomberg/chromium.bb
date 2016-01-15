# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'media_router',
      'variables': {
        'depends': [
          '../../../../third_party/polymer/v1_0/components-chromium/iron-a11y-keys-behavior/iron-a11y-keys-behavior-extracted.js',
          '../../../../third_party/polymer/v1_0/components-chromium/iron-behaviors/iron-button-state-extracted.js',
          '../../../../third_party/polymer/v1_0/components-chromium/iron-behaviors/iron-control-state-extracted.js',
          '../../../../third_party/polymer/v1_0/components-chromium/iron-form-element-behavior/iron-form-element-behavior-extracted.js',
          '../../../../third_party/polymer/v1_0/components-chromium/iron-icon/iron-icon-extracted.js',
          '../../../../third_party/polymer/v1_0/components-chromium/iron-menu-behavior/iron-menu-behavior-extracted.js',
          '../../../../third_party/polymer/v1_0/components-chromium/iron-meta/iron-meta-extracted.js',
          '../../../../third_party/polymer/v1_0/components-chromium/iron-selector/iron-multi-selectable-extracted.js',
          '../../../../third_party/polymer/v1_0/components-chromium/iron-selector/iron-selectable-extracted.js',
          '../../../../third_party/polymer/v1_0/components-chromium/iron-selector/iron-selection-extracted.js',
          '../../../../third_party/polymer/v1_0/components-chromium/iron-validatable-behavior/iron-validatable-behavior-extracted.js',
          '../../../../third_party/polymer/v1_0/components-chromium/paper-behaviors/paper-button-behavior-extracted.js',
          '../../../../third_party/polymer/v1_0/components-chromium/paper-behaviors/paper-ripple-behavior-extracted.js',
          '../../../../third_party/polymer/v1_0/components-chromium/paper-behaviors/paper-inky-focus-behavior-extracted.js',
          '../../../../third_party/polymer/v1_0/components-chromium/paper-dropdown-menu/paper-dropdown-menu-extracted.js',
          '../../../../third_party/polymer/v1_0/components-chromium/paper-button/paper-button-extracted.js',
          '../../../../third_party/polymer/v1_0/components-chromium/paper-icon-button/paper-icon-button-extracted.js',
          '../../../../third_party/polymer/v1_0/components-chromium/paper-input/paper-input-behavior-extracted.js',
          '../../../../third_party/polymer/v1_0/components-chromium/paper-item/paper-item-behavior-extracted.js',
          '../../../../third_party/polymer/v1_0/components-chromium/paper-item/paper-item-extracted.js',
          '../../../../third_party/polymer/v1_0/components-chromium/paper-menu/paper-menu-extracted.js',
          '../../../../third_party/polymer/v1_0/components-chromium/paper-ripple/paper-ripple-extracted.js',
          '../../../../third_party/polymer/v1_0/components-chromium/paper-spinner/paper-spinner-behavior-extracted.js',
          '../../../../third_party/polymer/v1_0/components-chromium/paper-spinner/paper-spinner-extracted.js',
          '../../../../third_party/polymer/v1_0/components-chromium/paper-toolbar/paper-toolbar-extracted.js',
          '../../../../ui/webui/resources/js/cr.js',
          '../../../../ui/webui/resources/js/compiled_resources.gyp:load_time_data',
          '../../../../ui/webui/resources/js/util.js',
          'media_router.js',
          'media_router_data.js',
          'media_router_ui_interface.js',
          'elements/issue_banner/issue_banner.js',
          'elements/route_details/route_details.js',
          'elements/media_router_header/media_router_header.js',
          'elements/media_router_container/media_router_container.js',
        ],
        'externs': [
          '<(EXTERNS_DIR)/chrome_send.js',
          'externs.js',
        ],
      },
      'includes': ['../../../../third_party/closure_compiler/compile_js.gypi'],
    }
  ],
}

