// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  const ArrayDataModel = cr.ui.ArrayDataModel;
  const OptionsPage = options.OptionsPage;
  const SettingsDialog = options.SettingsDialog;

  /**
   * StartupOverlay class
   * Encapsulated handling of the 'Set Startup pages' overlay page.
   * @constructor
   * @class
   */
  function StartupOverlay() {
    SettingsDialog.call(this, 'startup', templateData.startupPagesDialogTitle,
      'startup-overlay',
      $('startup-overlay-confirm'), $('startup-overlay-cancel'));
  };

  cr.addSingletonGetter(StartupOverlay);

  StartupOverlay.prototype = {
    __proto__: SettingsDialog.prototype,

    /**
     * An autocomplete list that can be attached to a text field during editing.
     * @type {HTMLElement}
     * @private
     */
    autocompleteList_: null,

    // TODO(tbreisacher): Work with jhawkins to refactor this so that we're not
    // overriding private handle* methods in SettingsDialog.

    /**
     * @override
     */
    handleConfirm_: function() {
      OptionsPage.closeOverlay();
      chrome.send('commitStartupPrefChanges');
    },

    /**
     * @override
     */
    handleCancel_: function() {
      OptionsPage.closeOverlay();
      chrome.send('cancelStartupPrefChanges');
    },

    /**
     * Initialize the page.
     */
    initializePage: function() {
      SettingsDialog.prototype.initializePage.call(this);

      var self = this;

      var startupPagesList = $('startupPagesList');
      options.browser_options.StartupPageList.decorate(startupPagesList);
      startupPagesList.autoExpands = true;

      $('startupUseCurrentButton').onclick = function(event) {
        chrome.send('setStartupPagesToCurrentPages');
      };

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
