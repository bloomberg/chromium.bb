// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  var OptionsPage = options.OptionsPage;

  /**
   * HomePageOverlay class
   * Dialog that allows users to set the home page.
   * @class
   */
  function HomePageOverlay() {
    OptionsPage.call(this, 'homePageOverlay', '', 'home-page-overlay');
  }

  cr.addSingletonGetter(HomePageOverlay);

  HomePageOverlay.prototype = {
    // Inherit HomePageOverlay from OptionsPage.
    __proto__: OptionsPage.prototype,

    /**
     * Initialize the page.
     */
    initializePage: function() {
      // Call base class implementation to start preference initialization.
      OptionsPage.prototype.initializePage.call(this);

      var self = this;
      $('home-page-confirm').onclick = function(event) {
        self.handleCancel_();
      };

      $('home-page-cancel').onclick = function(event) {
        self.handleCancel_();
      };
    },

    /**
     * Handles the cancel button by closing the overlay.
     * @private
     */
    handleCancel_: function() {
      OptionsPage.closeOverlay();
    },

  };

  // Export
  return {
    HomePageOverlay: HomePageOverlay
  };
});
