// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'gaia-input-form',

  properties: {
    disabled: {
      type: Boolean,
      observer: 'onDisabledChanged_',
    },

    buttonText: String
  },

  submit: function() {
    this.fire('submit');
  },

  onButtonClicked_: function() {
    this.submit();
  },

  getInputs_: function() {
    return Polymer.dom(this.$.inputs).getDistributedNodes();
  },

  onKeyDown_: function(e) {
    if (e.keyCode != 13 || this.$.button.disabled)
      return;
    if (this.getInputs_().indexOf(e.target) == -1)
      return;
    this.onButtonClicked_();
  },

  getControls_: function() {
    var controls = this.getInputs_();
    controls.push(this.$.button);
    return controls.concat(Polymer.dom(this).querySelectorAll('gaia-button'));
  },

  onDisabledChanged_: function(disabled) {
    this.getControls_().forEach(function(control) {
      control.disabled = disabled;
    });
  }
});
