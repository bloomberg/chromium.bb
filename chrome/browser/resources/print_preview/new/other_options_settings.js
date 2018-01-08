// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'print-preview-other-options-settings',

  behaviors: [SettingsBehavior],

  observers: [
    'onHeaderFooterSettingChange_(settings.headerFooter.value)',
    'onDuplexSettingChange_(settings.duplex.value)',
    'onCssBackgroundSettingChange_(settings.cssBackground.value)',
    'onRasterizeSettingChange_(settings.rasterize.value)',
    'onSelectionOnlySettingChange_(settings.selectionOnly.value)',
  ],

  /**
   * @param {boolean} value The new value of the header footer setting.
   * @private
   */
  onHeaderFooterSettingChange_: function(value) {
    this.$$('#header-footer').checked = value;
  },

  /**
   * @param {boolean} value The new value of the duplex setting.
   * @private
   */
  onDuplexSettingChange_: function(value) {
    this.$$('#duplex').checked = value;
  },

  /**
   * @param {boolean} value The new value of the css background setting.
   * @private
   */
  onCssBackgroundSettingChange_: function(value) {
    this.$$('#css-background').checked = value;
  },

  /**
   * @param {boolean} value The new value of the rasterize setting.
   * @private
   */
  onRasterizeSettingChange_: function(value) {
    this.$$('#rasterize').checked = value;
  },

  /**
   * @param {boolean} value The new value of the selection only setting.
   * @private
   */
  onSelectionOnlySettingChange_: function(value) {
    this.$$('#selection-only').checked = value;
  },

  /** @private */
  onHeaderFooterChange_: function() {
    this.setSetting('headerFooter', this.$$('#header-footer').checked);
  },

  /** @private */
  onDuplexChange_: function() {
    this.setSetting('duplex', this.$$('#duplex').checked);
  },

  /** @private */
  onCssBackgroundChange_: function() {
    this.setSetting('cssBackground', this.$$('#css-background').checked);
  },

  /** @private */
  onRasterizeChange_: function() {
    this.setSetting('rasterize', this.$$('#rasterize').checked);
  },

  /** @private */
  onSelectionOnlyChange_: function() {
    this.setSetting('selectionOnly', this.$$('#selection-only').checked);
  },
});
