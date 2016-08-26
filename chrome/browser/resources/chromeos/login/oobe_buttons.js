// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'oobe-icon-button',

  properties: {
    disabled: {type: Boolean, value: false, reflectToAttribute: true},

    icon: String,

    ariaLabel: String
  },

  focus: function() {
    this.$.iconButton.focus();
  },

  onClick_: function(e) {
    if (this.disabled)
      e.stopPropagation();
  }
});

Polymer({
  is: 'oobe-text-button',

  properties: {
    disabled: {type: Boolean, value: false, reflectToAttribute: true},

    label: String,

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
