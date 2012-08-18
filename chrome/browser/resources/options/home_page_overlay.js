// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  /** @const */ var OptionsPage = options.OptionsPage;
  /** @const */ var SettingsDialog = options.SettingsDialog;

  /**
   * HomePageOverlay class
   * Dialog that allows users to set the home page.
   * @extends {SettingsDialog}
   */
  function HomePageOverlay() {
    SettingsDialog.call(this, 'homePageOverlay',
                        loadTimeData.getString('homePageOverlayTabTitle'),
                        'home-page-overlay',
                        $('home-page-confirm'), $('home-page-cancel'));
  }

  cr.addSingletonGetter(HomePageOverlay);

  HomePageOverlay.prototype = {
    __proto__: SettingsDialog.prototype,

    /**
     * An autocomplete list that can be attached to the home page URL field.
     * @type {cr.ui.AutocompleteList}
     * @private
     */
    autocompleteList_: null,

    /**
     * Initialize the page.
     */
    initializePage: function() {
      // Call base class implementation to start preference initialization.
      SettingsDialog.prototype.initializePage.call(this);

      var self = this;
      $('homepage-use-ntp').onchange = this.updateHomePageInput_.bind(this);
      $('homepage-use-url').onchange = this.updateHomePageInput_.bind(this);

      var urlField = $('homepage-url-field');
      urlField.addEventListener('keydown', function(event) {
        // Focus the 'OK' button when the user hits enter since people expect
        // feedback indicating that they are done editing.
        if (event.keyIdentifier == 'Enter' && self.autocompleteList_.hidden)
          $('home-page-confirm').focus();
      });
      urlField.addEventListener('change', this.updateFavicon_.bind(this));

      var suggestionList = new cr.ui.AutocompleteList();
      suggestionList.autoExpands = true;
      suggestionList.suggestionUpdateRequestCallback =
          this.requestAutocompleteSuggestions_.bind(this);
      $('home-page-overlay').appendChild(suggestionList);
      this.autocompleteList_ = suggestionList;

      urlField.addEventListener('focus', function(event) {
        self.autocompleteList_.attachToInput(urlField);
      });
      urlField.addEventListener('blur', function(event) {
        self.autocompleteList_.detach();
      });

      // Text fields may change widths when the window changes size, so make
      // sure the suggestion list stays in sync.
      window.addEventListener('resize', function() {
        self.autocompleteList_.syncWidthToInput();
      });
    },

    /** @inheritDoc */
    didShowPage: function() {
      this.updateHomePageInput_();
      this.updateFavicon_();
    },

    /**
     * Updates the state of the homepage text input. The input is enabled only
     * if the |homepage-use-url| radio button is checked.
     * @private
     */
    updateHomePageInput_: function() {
      var urlField = $('homepage-url-field');
      var homePageUseURL = $('homepage-use-url');
      urlField.setDisabled('radio-choice', !homePageUseURL.checked);
    },

    /**
     * Updates the background of the url field to show the favicon for the
     * URL that is currently typed in.
     * @private
     */
    updateFavicon_: function() {
      var urlField = $('homepage-url-field');
      urlField.style.backgroundImage = url('chrome://favicon/' +
                                           urlField.value);
    },

    /**
     * Sends an asynchronous request for new autocompletion suggestions for the
     * the given query. When new suggestions are available, the C++ handler will
     * call updateAutocompleteSuggestions_.
     * @param {string} query List of autocomplete suggestions.
     * @private
     */
    requestAutocompleteSuggestions_: function(query) {
      chrome.send('requestAutocompleteSuggestionsForHomePage', [query]);
    },

    /**
     * Updates the autocomplete suggestion list with the given entries.
     * @param {Array} pages List of autocomplete suggestions.
     * @private
     */
    updateAutocompleteSuggestions_: function(suggestions) {
      var list = this.autocompleteList_;
      // If the trigger for this update was a value being selected from the
      // current list, do nothing.
      if (list.targetInput && list.selectedItem &&
          list.selectedItem['url'] == list.targetInput.value)
        return;
      list.suggestions = suggestions;
    },

    /**
     * Sets the 'show home button' and 'home page is new tab page' preferences.
     * (The home page url preference is set automatically by the SettingsDialog
     * code.)
     */
    handleConfirm: function() {
      // Strip whitespace.
      var urlField = $('homepage-url-field');
      var homePageValue = urlField.value.replace(/\s*/g, '');
      urlField.value = homePageValue;

      // Don't save an empty URL for the home page. If the user left the field
      // empty, switch to the New Tab page.
      if (!homePageValue)
        $('homepage-use-ntp').checked = true;

      SettingsDialog.prototype.handleConfirm.call(this);
    },
  };

  HomePageOverlay.updateAutocompleteSuggestions = function() {
    var instance = HomePageOverlay.getInstance();
    instance.updateAutocompleteSuggestions_.apply(instance, arguments);
  };

  // Export
  return {
    HomePageOverlay: HomePageOverlay
  };
});
