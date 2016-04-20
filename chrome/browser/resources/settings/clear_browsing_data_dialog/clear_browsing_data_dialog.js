// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-clear-browsing-data-dialog' allows the user to delete
 * browsing data that has been cached by Chromium.
 */
Polymer({
  is: 'settings-clear-browsing-data-dialog',

  behaviors: [WebUIListenerBehavior],

  properties: {
    /**
     * Preferences state.
     */
    prefs: {
      type: Object,
      notify: true,
    },

    /**
     * List of options for the dropdown menu.
     * @private {!DropdownMenuOptionList>}
     */
    clearFromOptions_: {
      readOnly: true,
      type: Array,
      value: [
        {value: 0, name: loadTimeData.getString('clearDataHour')},
        {value: 1, name: loadTimeData.getString('clearDataDay')},
        {value: 2, name: loadTimeData.getString('clearDataWeek')},
        {value: 3, name: loadTimeData.getString('clearData4Weeks')},
        {value: 4, name: loadTimeData.getString('clearDataEverything')},
      ],
    },

    /** @private */
    clearingInProgress_: Boolean,
  },

  /** @private {!settings.ClearBrowsingDataBrowserProxy} */
  browserProxy_: null,

  /** @override */
  ready: function() {
    this.$.clearFrom.menuOptions = this.clearFromOptions_;
    this.addWebUIListener(
        'browsing-history-pref-changed',
        this.setAllowDeletingHistory_.bind(this));
    this.browserProxy_ =
        settings.ClearBrowsingDataBrowserProxyImpl.getInstance();
  },

  /**
   * @param {boolean} allowed Whether the user is allowed to delete histories.
   * @private
   */
  setAllowDeletingHistory_: function(allowed) {
    this.$.browsingCheckbox.disabled = !allowed;
    this.$.downloadCheckbox.disabled = !allowed;
    if (!allowed) {
      this.set('prefs.browser.clear_data.browsing_history.value', false);
      this.set('prefs.browser.clear_data.download_history.value', false);
    }
  },

  open: function() {
    this.$.dialog.open();
  },

  /** @private */
  onClearBrowsingDataTap_: function() {
    this.clearingInProgress_ = true;
    this.browserProxy_.clearBrowsingData().then(function() {
      this.clearingInProgress_ = false;
      this.$.dialog.close();
    }.bind(this));
  },
});
