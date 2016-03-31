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
 */
Polymer({
  is: 'settings-startup-urls-page',

  behaviors: [WebUIListenerBehavior],

  properties: {
    /**
     * Preferences state.
     */
    prefs: {
      type: Object,
      notify: true,
    },

    /** @type {settings.StartupUrlsPageBrowserProxy} */
    browserProxy_: Object,

    /** @private {string} */
    newUrl_: {
      type: String,
      value: '',
    },

    /**
     * Pages to load upon browser startup.
     * @private {!Array<!StartupPageInfo>}
     */
    startupPages_: Array,
  },

  created: function() {
    this.browserProxy_ = settings.StartupUrlsPageBrowserProxyImpl.getInstance();
  },

  attached: function() {
    this.addWebUIListener('update-startup-pages',
                          this.updateStartupPages_.bind(this));
    this.browserProxy_.loadStartupPages();
  },

  /**
   * @param {string} url Location of an image to get a set of icons fors.
   * @return {string} A set of icon URLs.
   * @private
   */
  getIconSet_: function(url) {
    return getFaviconImageSet(url);
  },

  /** @private */
  updateStartupPages_: function(startupPages) {
    this.startupPages_ = startupPages;
  },

  /** @private */
  onAddPageTap_: function() {
    this.newUrl_ = '';
    this.$.addUrlDialog.open();
  },

  /** @private */
  onUseCurrentPagesTap_: function() {
    this.browserProxy_.useCurrentPages();
  },

  /** @private */
  onCancelTap_: function() {
    this.$.addUrlDialog.close();
  },

  /**
   * @return {boolean} Whether tapping the Add button should be allowed.
   * @private
   */
  isAddEnabled_: function() {
    return this.browserProxy_.canAddPage(this.newUrl_);
  },

  /** @private */
  onAddTap_: function() {
    assert(this.isAddEnabled_());
    this.browserProxy_.addStartupPage(this.newUrl_);
    this.$.addUrlDialog.close();
  },

  /**
   * @param {!{model: !{index: number}}} e
   * @private
   */
  onRemoveUrlTap_: function(e) {
    this.browserProxy_.removeStartupPage(e.model.index);
  },
});
