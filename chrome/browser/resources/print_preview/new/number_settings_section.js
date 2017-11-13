// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'print-preview-number-settings-section',

  behaviors: [print_preview_new.InputSettingsBehavior],

  properties: {
    /** @type {string} */
    initValue: String,

    /** @type {number} */
    maxValue: Number,

    /** @type {number} */
    minValue: Number,

    /** @type {string} */
    inputLabel: String,

    /** @type {string} */
    hintMessage: String,
  },

  /** @type {string} Overrides value in InputSettingsBehavior. */
  defaultValue: '0',

  /** @override */
  ready: function() {
    this.defaultValue = this.initValue;
  },
});
