// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'oobe-text-button',

  properties: {
    disabled: {type: Boolean, value: false, reflectToAttribute: true},

    inverse: Boolean,
  },

  focus: function() {
    this.$.textButton.focus();
  },

  onClick_: function(e) {
    if (this.disabled)
      e.stopPropagation();
  }
});

Polymer({
  is: 'oobe-back-button',

  properties: {
    disabled: {type: Boolean, value: false, reflectToAttribute: true},
  },

  focus: function() {
    this.$.button.focus();
  },

  onClick_: function(e) {
    if (this.disabled)
      e.stopPropagation();
  }
});

Polymer({
  is: 'oobe-welcome-secondary-button',

  properties: {
    icon: String,

    ariaLabel: String
  },
});
