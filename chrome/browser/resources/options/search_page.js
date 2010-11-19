// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  const OptionsPage = options.OptionsPage;

  /**
   * Encapsulated handling of the search page.
   */
  function SearchPage() {
    OptionsPage.call(this, 'search', templateData.searchPage, 'searchPage');
  }

  cr.addSingletonGetter(SearchPage);

  SearchPage.prototype = {
    // Inherit SearchPage from OptionsPage.
    __proto__: OptionsPage.prototype,

    // Initialize SearchPage.
    initializePage: function() {
      // Call base class implementation to start preference initialization.
      OptionsPage.prototype.initializePage.call(this);

      // Create a search field element.
      var searchField = document.createElement('input');
      searchField.id = 'searchField';
      searchField.type = 'search';
      searchField.setAttribute('autosave', 'org.chromium.options.search');
      searchField.setAttribute('results', '10');

      // Replace the contents of the navigation tab with the search field.
      this.tab.textContent = '';
      this.tab.appendChild(searchField);
    },
  };

  SearchPage.updateForEmptySearch = function() {
    $('searchPageInfo').classList.remove('hidden');
    $('searchPageNoMatches').classList.add('hidden');
  };

  SearchPage.updateForNoSearchResults = function(message) {
    $('searchPageInfo').classList.add('hidden');
    $('searchPageNoMatches').classList.remove('hidden');
  };

  SearchPage.updateForSuccessfulSearch = function(enable) {
    $('searchPageInfo').classList.add('hidden');
    $('searchPageNoMatches').classList.add('hidden');
  };

  // Export
  return {
    SearchPage: SearchPage
  };

});
