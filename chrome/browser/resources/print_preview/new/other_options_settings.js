// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'print-preview-other-options-settings',

  behaviors: [SettingsBehavior],

  properties: {
    /** @private {boolean} */
    duplexValue_: Boolean,
  },

  observers: [
    'onInitialized_(settings.duplex.value)',
    'onDuplexChange_(duplexValue_)',
  ],

  isInitialized_: false,

  onInitialized_: function() {
    if (this.isInitialized_)
      return;
    this.set('duplexValue_', this.getSetting('duplex').value);
    this.isInitialized_ = true;
  },

  onDuplexChange_: function() {
    this.setSetting('duplex', this.duplexValue_);
  },
});
