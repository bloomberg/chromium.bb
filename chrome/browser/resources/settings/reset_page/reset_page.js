// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-reset-page' is the settings page containing reset
 * settings.
 *
 * Example:
 *
 *    <iron-animated-pages>
 *      <settings-reset-page prefs="{{prefs}}">
 *      </settings-reset-page>
 *      ... other pages ...
 *    </iron-animated-pages>
 *
 * @group Chrome Settings Elements
 * @element settings-reset-page
 */
Polymer({
  is: 'settings-reset-page',

  properties: {
    feedbackInfo_: String,
  },

  attached: function() {
    cr.define('SettingsResetPage', function() {
      return {
        doneResetting: function() {
          this.$.resetSpinner.active = false;
          this.$.resetDialog.close();
        }.bind(this),

        setFeedbackInfo: function(data) {
          this.set('feedbackInfo_', data.feedbackInfo);
          this.async(function() {
            this.$.resetDialog.center();
          });
        }.bind(this),
      };
    }.bind(this));
  },

  /** @private */
  onShowDialog_: function() {
    this.$.resetDialog.open();
    chrome.send('onShowResetProfileDialog');
  },

  /** @private */
  onCancelTap_: function() {
    this.$.resetDialog.close();
    chrome.send('onHideResetProfileDialog');
  },

  /** @private */
  onResetTap_: function() {
    this.$.resetSpinner.active = true;
    chrome.send('performResetProfileSettings', [this.$.sendSettings.checked]);
  },

  /** @private */
  onLearnMoreTap_: function() {
    window.open(loadTimeData.getString('resetPageLearnMoreUrl'));
  },

  /** @private */
  onSendSettingsChange_: function() {
    // TODO(dpapad): Update how settings info is surfaced when final mocks
    // exist.
    this.$.settings.hidden = !this.$.sendSettings.checked;
    this.$.resetDialog.center();
  }
});
