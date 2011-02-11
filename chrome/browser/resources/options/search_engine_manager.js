// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  const OptionsPage = options.OptionsPage;
  const ArrayDataModel = cr.ui.ArrayDataModel;

  /**
   * Encapsulated handling of search engine management page.
   * @constructor
   */
  function SearchEngineManager() {
    this.activeNavTab = null;
    OptionsPage.call(this, 'searchEngines',
                     templateData.searchEngineManagerPageTabTitle,
                     'searchEngineManagerPage');
  }

  cr.addSingletonGetter(SearchEngineManager);

  SearchEngineManager.prototype = {
    __proto__: OptionsPage.prototype,

    /**
     * List for default search engine options
     * @type {boolean}
     * @private
     */
    defaultsList_: null,

    /**
     * List for other search engine options
     * @type {boolean}
     * @private
     */
    othersList_: null,

    /** inheritDoc */
    initializePage: function() {
      OptionsPage.prototype.initializePage.call(this);

      this.defaultsList_ = $('defaultSearchEngineList');
      this.setUpList_(this.defaultsList_);

      this.othersList_ = $('otherSearchEngineList');
      this.setUpList_(this.othersList_);
    },

    /**
     * Sets up the given list as a search engine list
     * @param {List} list The list to set up.
     * @private
     */
    setUpList_: function(list) {
      options.search_engines.SearchEngineList.decorate(list);
      list.autoExpands = true;
    },

    /**
     * Updates the search engine list with the given entries.
     * @private
     * @param {Array} defaultEngines List of possible default search engines.
     * @param {Array} otherEngines List of other search engines.
     */
    updateSearchEngineList_: function(defaultEngines, otherEngines) {
      this.defaultsList_.dataModel = new ArrayDataModel(defaultEngines);
      var othersModel = new ArrayDataModel(otherEngines);
      // Add a "new engine" row.
      othersModel.push({
        'modelIndex': '-1'
      });
      this.othersList_.dataModel = othersModel;
    },
  };

  SearchEngineManager.updateSearchEngineList = function(defaultEngines,
                                                        otherEngines) {
    SearchEngineManager.getInstance().updateSearchEngineList_(defaultEngines,
                                                              otherEngines);
  };

  SearchEngineManager.validityCheckCallback = function(validity, modelIndex) {
    // Forward to both lists; the one without a matching modelIndex will ignore
    // it.
    SearchEngineManager.getInstance().defaultsList_.validationComplete(
        validity, modelIndex);
    SearchEngineManager.getInstance().othersList_.validationComplete(
        validity, modelIndex);
  };

  // Export
  return {
    SearchEngineManager: SearchEngineManager
  };

});

