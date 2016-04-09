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
 */
Polymer({
  is: 'settings-startup-urls-page',

  behaviors: [WebUIListenerBehavior],

  properties: {
    /** @type {settings.StartupUrlsPageBrowserProxy} */
    browserProxy_: Object,

    /**
     * Pages to load upon browser startup.
     * @private {!Array<!StartupPageInfo>}
     */
    startupPages_: Array,

    showStartupUrlDialog_: Boolean,
  },

  /** @override */
  attached: function() {
    this.browserProxy_ = settings.StartupUrlsPageBrowserProxyImpl.getInstance();
    this.addWebUIListener('update-startup-pages', function(startupPages) {
      this.startupPages_ = startupPages;
    }.bind(this));
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

  /**
   * Opens the dialog and registers a listener for removing the dialog from the
   * DOM once is closed. The listener is destroyed when the dialog is removed
   * (because of 'restamp').
   * @private
   */
  onAddPageTap_: function() {
    this.showStartupUrlDialog_ = true;
    this.async(function() {
      var dialog = this.$$('settings-startup-url-dialog');
      dialog.addEventListener('iron-overlay-closed', function() {
        this.showStartupUrlDialog_ = false;
      }.bind(this));
    }.bind(this));
  },

  /** @private */
  onUseCurrentPagesTap_: function() {
    this.browserProxy_.useCurrentPages();
  },

  /**
   * @param {!{model: !{index: number}}} e
   * @private
   */
  onRemoveUrlTap_: function(e) {
    this.browserProxy_.removeStartupPage(e.model.index);
  },
});
