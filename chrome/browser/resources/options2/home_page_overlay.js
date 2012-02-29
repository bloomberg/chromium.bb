// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  const OptionsPage = options.OptionsPage;
  const SettingsDialog = options.SettingsDialog;

  /**
   * HomePageOverlay class
   * Dialog that allows users to set the home page.
   * @extends {SettingsDialog}
   */
  function HomePageOverlay() {
    SettingsDialog.call(this, 'homePageOverlay',
                        templateData.homePageOverlayTabTitle,
                        'home-page-overlay',
                        $('home-page-confirm'), $('home-page-cancel'));
  }

  cr.addSingletonGetter(HomePageOverlay);

  HomePageOverlay.prototype = {
    __proto__: SettingsDialog.prototype,

    /**
     * Initialize the page.
     */
    initializePage: function() {
      // Call base class implementation to start preference initialization.
      SettingsDialog.prototype.initializePage.call(this);

      var self = this;
      $('homepageURL').addEventListener('keydown', function(event) {
        // Focus the 'OK' button when the user hits enter since people expect
        // feedback indicating that they are done editing.
        if (event.keyIdentifier == 'Enter')
          $('home-page-confirm').focus();
      });

      // TODO(jhawkins): Refactor BrowserOptions.autocompleteList and use it
      // here.
    },

    /**
     * Sets the 'show home button' and 'home page is new tab page' preferences.
     * (The home page url preference is set automatically by the SettingsDialog
     * code.)
     */
    handleConfirm: function() {
      // Strip whitespace.
      var homePageValue = $('homepageURL').value.replace(/\s*/g, '');
      $('homepageURL').value = homePageValue;

      // Don't save an empty URL for the home page. If the user left the field
      // empty, act as if they clicked Cancel.
      if (!homePageValue) {
        this.handleCancel();
      } else {
        SettingsDialog.prototype.handleConfirm.call(this);
        Preferences.setBooleanPref('browser.show_home_button', true);
        Preferences.setBooleanPref('homepage_is_newtabpage', false);
        BrowserOptions.getInstance().updateHomePageSelector();
      }
    },

    /**
     * Resets the <select> on the browser options page to the appropriate value,
     * based on the current preferences.
     */
    handleCancel: function() {
      SettingsDialog.prototype.handleCancel.call(this);
      BrowserOptions.getInstance().updateHomePageSelector();
    },
  };

  // Export
  return {
    HomePageOverlay: HomePageOverlay
  };
});
