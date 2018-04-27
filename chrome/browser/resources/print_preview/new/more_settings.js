// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'print-preview-more-settings',

  behaviors: [I18nBehavior, print_preview_new.SettingsSectionBehavior],

  properties: {
    settingsExpanded: {
      type: Boolean,
      notify: true,
    },

    disabled: Boolean,
  },

  listeners: {
    'click': 'onMoreSettingsClick_',
  },

  /**
   * @return {string} 'plus-icon' if settings are collapsed, 'minus-icon' if
   *     they are expanded.
   * @private
   */
  getIconClass_: function() {
    return this.settingsExpanded ? 'minus-icon' : 'plus-icon';
  },

  /**
   * @return {string} The text to display on the label.
   * @private
   */
  getLabelText_: function() {
    return this.i18n(
        this.settingsExpanded ? 'lessOptionsLabel' : 'moreOptionsLabel');
  },

  /** @private */
  onMoreSettingsClick_: function() {
    this.settingsExpanded = !this.settingsExpanded;
  },
});
