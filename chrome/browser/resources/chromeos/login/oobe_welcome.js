// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'oobe-welcome-md',

  properties: {
    disabled: {
      type: Boolean,
      value: false,
    },
    currentLanguage: {
      type: String,
      value: 'English (US)',
    },
  },

  focus: function() {
    this.$.welcomeNextButton.focus();
  },

  onAnimationFinish_: function() {
    this.focus();
  },

  onWelcomeNextButtonClicked_: function() {
    $('oobe-connect').hidden = false;
    $('oobe-welcome-md').hidden = true;
  }
});
