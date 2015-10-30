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

  /** @private */
  onShowDialog_: function() {
    this.$.resetDialog.open();
  },

  /** @private */
  onCancelTap_: function() {
    this.$.resetDialog.close();
  },

  /** @private */
  onResetTap_: function() {
    // TODO(dpapad): Set up C++ handlers and figure out when it is OK to close
    // the dialog.
    this.$.resetDialog.close();
  },

  /** @private */
  onLearnMoreTap_: function() {
    window.open(loadTimeData.getString('resetPageLearnMoreUrl'));
  }
});
