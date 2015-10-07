// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

    /** @type {!Array<string>} */
    savedUrlList: {
      type: Array,
    },
  },

  observers: [
    'prefsChanged_(prefs.session.startup_urls.value.*)',
  ],

  attached: function() {
    var self = this;
    cr.define('Settings', function() {
      return {
        updateStartupPages: function() {
          return self.updateStartupPages_.apply(self, arguments);
        },
      };
    });
  },


  /** @private */
  prefsChanged_: function(change) {
    if (!this.savedUrlList) {
      var pref = /** @type {chrome.settingsPrivate.PrefObject} */(
          this.get('prefs.session.startup_urls'));
      if (pref)
        this.savedUrlList = pref.value.slice();
    }
  },

  /** @private */
  updateStartupPages_: function(data) {
    var urlArray = [];
    for (var i = 0; i < data.length; ++i)
      urlArray.push(data[i].url);
    this.set('prefs.session.startup_urls.value', urlArray);
  },

  /** @private */
  onUseCurrentPagesTap_: function() {
    chrome.send('setStartupPagesToCurrentPages');
  },

  /** @private */
  onCancelTap_: function() {
    if (this.savedUrlList !== undefined) {
      this.set('prefs.session.startup_urls.value', this.savedUrlList.slice());
    }
  },

  /** @private */
  onOkTap_: function() {
    var value = this.newUrl && this.newUrl.trim();
    if (!value)
      return;
    this.push('prefs.session.startup_urls.value', value);
    this.newUrl = '';
  },

  /**
   * @param {!{model: !{index: number}}} e
   * @private
   */
  onRemoveUrlTap_: function(e) {
    this.splice('prefs.session.startup_urls.value', e.model.index, 1);
  },
});
