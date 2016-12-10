// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'oobe-welcome-dialog',
  properties: {
    /**
     * Currently selected system language (display name).
     */
    currentLanguage: {
      type: String,
      value: '',
    },
    /**
     * Controls visibility of "Timezone" button.
     */
    timezoneButtonVisible: {
      type: Boolean,
      value: false,
    },
  },

  onLanguageClicked_: function() {
    this.fire('language-button-clicked');
  },

  onAccessibilityClicked_: function() {
    this.fire('accessibility-button-clicked');
  },

  onTimezoneClicked_: function() {
    this.fire('timezone-button-clicked');
  },

  onNextClicked_: function() {
    this.fire('next-button-clicked');
  }
});
