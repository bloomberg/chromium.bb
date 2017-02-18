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

    /**
     * Controls displaying of "Enable debugging features" link.
     */
     debuggingLinkVisible: Boolean,
  },

  /**
   * This is stored ID of currently focused element to restore id on returns
   * to this dialog from Language / Timezone Selection dialogs.
   */
  focusedElement_: 'languageSelectionButton',

  onLanguageClicked_: function() {
    this.focusedElement_ = "languageSelectionButton";
    this.fire('language-button-clicked');
  },

  onAccessibilityClicked_: function() {
    this.focusedElement_ = "accessibilitySettingsButton";
    this.fire('accessibility-button-clicked');
  },

  onTimezoneClicked_: function() {
    this.focusedElement_ = "timezoneSettingsButton";
    this.fire('timezone-button-clicked');
  },

  onNextClicked_: function() {
    this.focusedElement_ = "welcomeNextButton";
    this.fire('next-button-clicked');
  },

  onDebuggingLinkClicked_: function() {
    chrome.send('login.NetworkScreen.userActed',
        ['connect-debugging-features']);
  },

  attached: function() {
    this.focus();
  },

  focus: function() {
    var focusedElement = this.$[this.focusedElement_];
    if (focusedElement)
      focusedElement.focus();
  },

  /**
    * This is called from oobe_welcome when this dialog is shown.
    */
  show: function() {
    this.focus();
  },

  /**
    * This function formats message for labels.
    * @param String label i18n string ID.
    * @param String parameter i18n string parameter.
    * @private
    */
  formatMessage_: function(label, parameter) {
    return loadTimeData.getStringF(label, parameter);
  },
});
