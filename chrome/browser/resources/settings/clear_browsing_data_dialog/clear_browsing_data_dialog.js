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
     * Results of browsing data counters, keyed by the suffix of
     * the corresponding data type deletion preference, as reported
     * by the C++ side.
     * @private {!Object<string>}
     */
    counters_: {
      type: Object,
      // Will be filled as results are reported.
      value: function() {
        return {};
      }
    },

    /**
     * List of options for the dropdown menu.
     * @private {!DropdownMenuOptionList}
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
    clearingInProgress_: {
      type: Boolean,
      value: false,
    },

    /** @private */
    isSupervised_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean('isSupervised');
      },
    },

    /** @private */
    showHistoryDeletionDialog_: {
      type: Boolean,
      value: false,
    },

    /** @private {!Array<ImportantSite>} */
    importantSites_: {
      type: Array,
      value: function() {
        return [];
      }
    },

    /** @private */
    importantSitesFlagEnabled_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean('importantSitesInCbd');
      },
    },

    /** @private */
    showImportantSitesDialog_: {type: Boolean, value: false},
  },

  /** @private {settings.ClearBrowsingDataBrowserProxy} */
  browserProxy_: null,

  /** @override */
  ready: function() {
    this.$.clearFrom.menuOptions = this.clearFromOptions_;
    this.addWebUIListener('update-footer', this.updateFooter_.bind(this));
    this.addWebUIListener(
        'update-counter-text', this.updateCounterText_.bind(this));
  },

  /** @override */
  attached: function() {
    this.browserProxy_ =
        settings.ClearBrowsingDataBrowserProxyImpl.getInstance();
    this.browserProxy_.initialize().then(function() {
      this.$.clearBrowsingDataDialog.showModal();
    }.bind(this));

    if (this.importantSitesFlagEnabled_) {
      this.browserProxy_.getImportantSites().then(function(sites) {
        this.importantSites_ = sites;
      }.bind(this));
    }
  },

  /**
   * Updates the footer to show only those sentences that are relevant to this
   * user.
   * @param {boolean} syncing Whether the user is syncing data.
   * @param {boolean} otherFormsOfBrowsingHistory Whether the user has other
   *     forms of browsing history in their account.
   * @private
   */
  updateFooter_: function(syncing, otherFormsOfBrowsingHistory) {
    this.$.googleFooter.hidden = !otherFormsOfBrowsingHistory;
    this.$.syncedDataSentence.hidden = !syncing;
    this.$.clearBrowsingDataDialog.classList.add('fully-rendered');
  },

  /**
   * Updates the text of a browsing data counter corresponding to the given
   * preference.
   * @param {string} prefName Browsing data type deletion preference.
   * @param {string} text The text with which to update the counter
   * @private
   */
  updateCounterText_: function(prefName, text) {
    // Data type deletion preferences are named "browser.clear_data.<datatype>".
    // Strip the common prefix, i.e. use only "<datatype>".
    var matches = prefName.match(/^browser\.clear_data\.(\w+)$/);
    this.set('counters_.' + assert(matches[1]), text);
  },

  /**
   * @return {boolean} Whether the ImportantSites dialog should be shown.
   * @private
   */
  shouldShowImportantSites_: function() {
    if (!this.importantSitesFlagEnabled_)
      return false;
    if (!this.$.cookiesCheckbox.checked)
      return false;

    var haveImportantSites = this.importantSites_.length > 0;
    chrome.send(
        'metricsHandler:recordBooleanHistogram',
        ['History.ClearBrowsingData.ImportantDialogShown', haveImportantSites]);
    return haveImportantSites;
  },

  /**
   * Handles the tap on the Clear Data button.
   * @private
   */
  onClearBrowsingDataTap_: function() {
    if (this.shouldShowImportantSites_()) {
      this.showImportantSitesDialog_ = true;
      this.$.clearBrowsingDataDialog.close();
      // Show important sites dialog after dom-if is applied.
      this.async(function() {
        this.$$('#importantSitesDialog').showModal();
      });
    } else {
      this.clearBrowsingData_();
    }
  },

  /**
   * Handles closing of the clear browsing data dialog. Stops the close
   * event from propagating if another dialog is shown to prevent the
   * privacy-page from closing this dialog.
   * @private
   */
  onClearBrowsingDataDialogClose_: function(event) {
    if (this.showImportantSitesDialog_)
      event.stopPropagation();
  },

  /**
   * Clears browsing data and maybe shows a history notice.
   * @private
   */
  clearBrowsingData_: function() {
    this.clearingInProgress_ = true;

    this.browserProxy_.clearBrowsingData(this.importantSites_)
        .then(
            /**
             * @param {boolean} shouldShowNotice Whether we should show the
             * notice about other forms of browsing history before closing the
             * dialog.
             */
            function(shouldShowNotice) {
              this.clearingInProgress_ = false;
              this.showHistoryDeletionDialog_ = shouldShowNotice;
              if (!shouldShowNotice)
                this.closeDialogs_();
            }.bind(this));
  },

  /**
   * Closes the clear browsing data or important site dialog if they are open.
   * @private
   */
  closeDialogs_: function() {
    if (this.$.clearBrowsingDataDialog.open)
      this.$.clearBrowsingDataDialog.close();
    if (this.showImportantSitesDialog_)
      this.$$('#importantSitesDialog').close();
  },

  /** @private */
  onCancelTap_: function() {
    this.$.clearBrowsingDataDialog.cancel();
  },

  /**
   * Handles the tap confirm button in important sites.
   * @private
   */
  onImportantSitesConfirmTap_: function() {
    this.clearBrowsingData_();
  },

  /** @private */
  onImportantSitesCancelTap_: function() {
    this.$$('#importantSitesDialog').cancel();
  },

  /**
   * Handles the closing of the notice about other forms of browsing history.
   * @private
   */
  onHistoryDeletionDialogClose_: function() {
    this.showHistoryDeletionDialog_ = false;
    this.closeDialogs_();
  },
});
