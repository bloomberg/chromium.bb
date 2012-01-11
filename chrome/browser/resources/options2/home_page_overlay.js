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
      $('homepage-use-ntp').onchange = this.updateHomePageInput_.bind(this);
      $('homepage-use-url').onchange = this.updateHomePageInput_.bind(this);
      $('home-page-confirm').onclick = this.handleCancel_.bind(this);
      $('home-page-cancel').onclick = this.handleCancel_.bind(this);

      $('homepageURL').addEventListener('keydown', function(event) {
        // Focus the 'OK' button when the user hits enter since people expect
        // feedback indicating that they are done editing.
        if (event.keyIdentifier == 'Enter')
          $('home-page-confirm').focus();
      });

      // TODO(jhawkins): Refactor BrowserOptions.autocompleteList and use it
      // here.
    },

    didShowPage: function() {
      // Set initial state.
      this.updateHomePageInput_();
    },

    /**
     * Updates the state of the homepage text input. The input is enabled only
     * if the |homepageUseURLBUtton| radio is checked.
     * @private
     */
    updateHomePageInput_: function() {
      var homepageInput = $('homepageURL');
      var homepageUseURL = $('homepage-use-url');
      homepageInput.disabled = !homepageUseURL.checked;
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
