// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'cr-settings-startup-urls-page' is the settings page
 * containing the urls that will be opened when chrome is started.
 *
 * Example:
 *
 *    <neon-animated-pages>
 *      <cr-settings-startup-urls-page prefs="{{prefs}}">
 *      </cr-settings-startup-urls-page>
 *      ... other pages ...
 *    </neon-animated-pages>
 *
 * @group Chrome Settings Elements
 * @element cr-settings-startup-urls-page
 */
Polymer({
  is: 'cr-settings-startup-urls-page',

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
      value: function() { return []; }
    },
  },

  /** @private */
  onUseCurrentPagesTap_: function() {
    // TODO(dschuyler): I'll be making a chrome.send call here.
  },

  /** @private */
  onCancelTap_: function() {
    this.set('prefs.session.startup_urls.value', this.savedUrlList.slice());
  },

  /** @private */
  onOkTap_: function() {
    var value = this.newUrl && this.newUrl.trim();
    if (!value)
      return;
    this.push('prefs.session.startup_urls.value', value);
    this.newUrl = undefined;
  },

  /** @private */
  onRemoveUrlTap_: function(e) {
    this.splice('prefs.session.startup_urls.value', e.model.index, 1);
  },
});
