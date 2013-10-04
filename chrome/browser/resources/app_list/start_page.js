// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview App launcher start page implementation.
 */

<include src="recommended_apps.js"/>

cr.define('appList.startPage', function() {
  'use strict';

  /**
   * Creates a StartPage object.
   * @constructor
   * @extends {HTMLDivElement}
   */
  var StartPage = cr.ui.define('div');

  StartPage.prototype = {
    __proto__: HTMLDivElement.prototype,

    /**
     * Instance of the recommended apps card.
     * @type {appsList.startPage.RecommendedApps}
     * @private
     */
    recommendedApps_: null,

    /** @override */
    decorate: function() {
      this.recommendedApps_ = new appList.startPage.RecommendedApps();
      this.appendChild(this.recommendedApps_);
    },

    /**
     * Sets the recommended apps.
     * @param {!Array.<!{appId: string,
     *                   iconUrl: string,
     *                   textTitle: string}>} apps An array of app info
     *     dictionary to be displayed in the AppItemView.
     */
    setRecommendedApps: function(apps) {
      this.recommendedApps_.setApps(apps);
    }
  };

  /**
   * Initialize the page.
   */
  function initialize() {
    StartPage.decorate($('start-page'));
    chrome.send('initialize');
  }

  /**
   * Sets the recommended apps.
   * @param {Array.<Object>} apps An array of app info dictionary.
   */
  function setRecommendedApps(apps) {
    $('start-page').setRecommendedApps(apps);
  }

  return {
    initialize: initialize,
    setRecommendedApps: setRecommendedApps
  };
});

document.addEventListener('contextmenu', function(e) { e.preventDefault(); });
document.addEventListener('DOMContentLoaded', appList.startPage.initialize);
