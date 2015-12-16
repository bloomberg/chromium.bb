// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @typedef {{
 *   'title': string,
 *   'tooltip': string,
 *   'url': string
 * }}
 */
var StartupPageInfo;

/**
 * @fileoverview 'settings-startup-urls-page' is the settings page
 * containing the urls that will be opened when chrome is started.
 *
 * Example:
 *
 *    <neon-animated-pages>
 *      <settings-startup-urls-page prefs="{{prefs}}">
 *      </settings-startup-urls-page>
 *      ... other pages ...
 *    </neon-animated-pages>
 *
 * @group Chrome Settings Elements
 * @element settings-startup-urls-page
 */
Polymer({
  is: 'settings-startup-urls-page',

  properties: {
    /**
     * Preferences state.
     */
    prefs: {
      type: Object,
      notify: true,
    },

    newUrl: {
      type: String,
    },

    /**
     * Pages to load upon browser startup.
     * @private {!Array<!StartupPageInfo>}
     */
    startupPages_: Array,
  },

  attached: function() {
    var self = this;
    cr.define('Settings', function() {
      return {
        updateStartupPages: function() {
          return self.updateStartupPages_.apply(self, arguments);
        },
      };
    });
    chrome.send('onStartupPrefsPageLoad');
  },

  /** @private */
  updateStartupPages_: function(startupPages) {
    this.startupPages_ = startupPages;
  },

  /** @private */
  onAddPageTap_: function() {
    this.$.addUrlDialog.open();
  },

  /** @private */
  onUseCurrentPagesTap_: function() {
    chrome.send('setStartupPagesToCurrentPages');
  },

  /** @private */
  onCancelTap_: function() {
    this.$.addUrlDialog.close();
  },

  /** @private */
  onOkTap_: function() {
    var value = this.newUrl && this.newUrl.trim();
    if (!value)
      return;
    chrome.send('addStartupPage', [value]);
    this.newUrl = '';
    this.$.addUrlDialog.close();
  },

  /**
   * @param {!{model: !{index: number}}} e
   * @private
   */
  onRemoveUrlTap_: function(e) {
    chrome.send('removeStartupPage', [e.model.index]);
  },
});
