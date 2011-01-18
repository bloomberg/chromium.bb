// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  const OptionsPage = options.OptionsPage;
  const ArrayDataModel = cr.ui.ArrayDataModel;
  const ListSingleSelectionModel = cr.ui.ListSingleSelectionModel;

  /**
   * Encapsulated handling of search engine management page.
   * @constructor
   */
  function SearchEngineManager() {
    this.activeNavTab = null;
    OptionsPage.call(this, 'searchEngines',
                     templateData.searchEngineManagerPage,
                     'searchEngineManagerPage');
  }

  cr.addSingletonGetter(SearchEngineManager);

  SearchEngineManager.prototype = {
    __proto__: OptionsPage.prototype,
    list_: null,

    initializePage: function() {
      OptionsPage.prototype.initializePage.call(this);

      this.list_ = $('searchEngineList')
      options.search_engines.SearchEngineList.decorate(this.list_);
      var selectionModel = new ListSingleSelectionModel;
      this.list_.selectionModel = selectionModel;
      this.list_.autoExpands = true;
    },

    /**
     * Updates the search engine list with the given entries.
     * @private
     * @param {Array} engineList List of available search engines.
     */
    updateSearchEngineList_: function(engineList) {
      var model = new ArrayDataModel(engineList);
      model.push({
        'modelIndex': '-1'
      });
      this.list_.dataModel = model;
    },
  };

  SearchEngineManager.updateSearchEngineList = function(engineList) {
    SearchEngineManager.getInstance().updateSearchEngineList_(engineList);
  };

  SearchEngineManager.validityCheckCallback = function(validity, modelIndex) {
    SearchEngineManager.getInstance().list_.validationComplete(validity,
                                                               modelIndex);
  };

  // Export
  return {
    SearchEngineManager: SearchEngineManager
  };

});

