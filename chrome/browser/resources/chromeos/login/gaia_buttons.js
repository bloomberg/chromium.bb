// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @enum {string} */
const GaiaButtonType = {
  NONE: '',
  LINK: 'link',
  DIALOG: 'dialog',
};

Polymer({
  is: 'gaia-button',

  properties: {
    disabled: {type: Boolean, value: false, reflectToAttribute: true},

    /** @type GaiaButtonType */
    type: {
      type: String,
      value: GaiaButtonType.NONE,
      reflectToAttribute: true,
      observer: 'typeChanged_'
    }
  },

  focus: function() {
    this.$.button.focus();
  },

  /** @private */
  focusedChanged_: function() {
    if (this.type == GaiaButtonType.LINK || this.type == GaiaButtonType.DIALOG)
      return;
    this.$.button.raised = this.$.button.focused;
  },

  /** @private */
  typeChanged_: function() {
    if (this.type == GaiaButtonType.LINK)
      this.$.button.setAttribute('noink', '');
    else
      this.$.button.removeAttribute('noink');
  },

  /** @private */
  onClick_: function(e) {
    if (this.disabled)
      e.stopPropagation();
  }
});

Polymer({
  is: 'gaia-icon-button',

  properties: {
    disabled: {type: Boolean, value: false, reflectToAttribute: true},

    icon: String,

    ariaLabel: String
  },

  focus: function() {
    this.$.iconButton.focus();
  },

  /** @private */
  onClick_: function(e) {
    if (this.disabled)
      e.stopPropagation();
  }
});
