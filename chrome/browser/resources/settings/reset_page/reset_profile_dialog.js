// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-reset-profile-dialog' is the dialog shown for clearing profile
 * settings.
 */
Polymer({
  is: 'settings-reset-profile-dialog',

  behaviors: [WebUIListenerBehavior],

  /** @private {!settings.ResetBrowserProxy} */
  browserProxy_: null,

  /** @override */
  ready: function() {
    this.browserProxy_ = settings.ResetBrowserProxyImpl.getInstance();

    this.addEventListener('iron-overlay-canceled', function() {
      this.browserProxy_.onHideResetProfileDialog();
    }.bind(this));
  },

  open: function() {
    this.$.dialog.open();
    this.browserProxy_.onShowResetProfileDialog();
  },

  /** @private */
  onCancelTap_: function() {
    this.$.dialog.cancel();
  },

  /** @private */
  onResetTap_: function() {
    this.$.resetSpinner.active = true;
    this.browserProxy_.performResetProfileSettings(
        this.$.sendSettings.checked).then(function() {
      this.$.resetSpinner.active = false;
      this.$.dialog.close();
      this.dispatchEvent(new CustomEvent('reset-done'));
    }.bind(this));
  },

  /**
   * Displays the settings that will be reported in a new tab.
   * @private
   */
  onShowReportedSettingsTap_: function() {
    this.browserProxy_.getReportedSettings().then(function(settings) {
      var feedbackObj = {};
      settings.forEach(function(entry) {
        // Break strings with '\n' characters into arrays to make the settings a
        // bit more readable.
        var values = entry.value.split('\n');
        feedbackObj[entry.key] = values.length == 1 ? values[0] : values;
      });
      var win = window.open('about:blank');
      var div = win.document.createElement('div');
      div.textContent = JSON.stringify(feedbackObj, null, 2 /* spaces */);
      div.style.whiteSpace = 'pre';
      win.document.body.appendChild(div);
    });
  },
});
