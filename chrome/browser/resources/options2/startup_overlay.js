// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  var OptionsPage = options.OptionsPage;
  var ArrayDataModel = cr.ui.ArrayDataModel;

  const localStrings = new LocalStrings();

  /**
   * StartupOverlay class
   * Encapsulated handling of the 'Set Startup pages' overlay page.
   * @constructor
   * @class
   */
  function StartupOverlay() {
    OptionsPage.call(this, 'startup',
                     templateData.startupOverlayTabTitle,
                     'startup-overlay');
  };

  cr.addSingletonGetter(StartupOverlay);

  StartupOverlay.prototype = {
    // Inherit from OptionsPage.
    __proto__: OptionsPage.prototype,

    startup_pages_pref_: {
      'name': 'session.urls_to_restore_on_startup',
      'disabled': false
    },

    /**
     * An autocomplete list that can be attached to a text field during editing.
     * @type {HTMLElement}
     * @private
     */
    autocompleteList_ : null,

    /**
     * Initialize the page.
     */
    initializePage: function() {
      // Call base class implementation to start preference initialization.
      OptionsPage.prototype.initializePage.call(this);

      var self = this;

      var startupPagesList = $('startupPagesList');
      options.browser_options.StartupPageList.decorate(startupPagesList);
      startupPagesList.autoExpands = true;

      $('startupUseCurrentButton').onclick = function(event) {
        chrome.send('setStartupPagesToCurrentPages');
      };

      $('startup-overlay-ok').onclick = function() {
        OptionsPage.closeOverlay();
      }

      // Initialize control enabled states.
      Preferences.getInstance().addEventListener(
          this.startup_pages_pref_.name,
          this.handleStartupPageListChange_.bind(this));

      var suggestionList = new cr.ui.AutocompleteList();
      suggestionList.autoExpands = true;
      suggestionList.suggestionUpdateRequestCallback =
          this.requestAutocompleteSuggestions_.bind(this);
      $('startup-overlay').appendChild(suggestionList);
      this.autocompleteList_ = suggestionList;
      startupPagesList.autocompleteList = suggestionList;
    },

    /**
     * Updates the startup pages list with the given entries.
     * @param {Array} pages List of startup pages.
     * @private
     */
    updateStartupPages_: function(pages) {
      var model = new ArrayDataModel(pages);
      // Add a "new page" row.
      model.push({
        'modelIndex': '-1'
      });
      $('startupPagesList').dataModel = model;
    },

    /**
     * Sends an asynchronous request for new autocompletion suggestions for the
     * the given query. When new suggestions are available, the C++ handler will
     * call updateAutocompleteSuggestions_.
     * @param {string} query List of autocomplete suggestions.
     * @private
     */
    requestAutocompleteSuggestions_: function(query) {
      chrome.send('requestAutocompleteSuggestionsForStartupPages', [query]);
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
          list.selectedItem['url'] == list.targetInput.value) {
        return;
      }
      list.suggestions = suggestions;
    },

    /**
     * Handle change events of the preference
     * 'session.urls_to_restore_on_startup'.
     * @param {event} preference changed event.
     * @private
     */
    handleStartupPageListChange_: function(event) {
      this.startup_pages_pref_.disabled = event.value['disabled'];
    },
  };

  // Forward public APIs to private implementations.
  [
    'updateStartupPages',
    'updateAutocompleteSuggestions',
  ].forEach(function(name) {
    StartupOverlay[name] = function(value) {
      StartupOverlay.getInstance()[name + '_'](value);
    };
  });

  // Export
  return {
    StartupOverlay: StartupOverlay
  };
});
