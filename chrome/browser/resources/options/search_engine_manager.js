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
      var selectionModel = new ListSingleSelectionModel
      this.list_.selectionModel = selectionModel;
      this.list_.autoExpands = true;

      selectionModel.addEventListener('change',
          this.selectionChanged_.bind(this));

      var self = this;
      $('addSearchEngineButton').onclick = function(event) {
        chrome.send('editSearchEngine', ["-1"]);
        OptionsPage.showOverlay('editSearchEngineOverlay');
      };
      $('editSearchEngineButton').onclick = function(event) {
        chrome.send('editSearchEngine', [self.selectedModelIndex_]);
        OptionsPage.showOverlay('editSearchEngineOverlay');
      };
      $('makeDefaultSearchEngineButton').onclick = function(event) {
        chrome.send('managerSetDefaultSearchEngine',
                    [self.selectedModelIndex_]);
      };

      // Remove Windows-style accelerators from button labels.
      // TODO(stuartmorgan): Remove this once the strings are updated.
      $('addSearchEngineButton').textContent =
          localStrings.getStringWithoutAccelerator('addSearchEngineButton');
    },

    /**
     * Updates the search engine list with the given entries.
     * @private
     * @param {Array} engineList List of available search engines.
     */
    updateSearchEngineList_: function(engineList) {
      this.list_.dataModel = new ArrayDataModel(engineList);
    },

    /**
     * Returns the currently selected list item's underlying model index.
     * @private
     */
    get selectedModelIndex_() {
      var listIndex = this.list_.selectionModel.selectedIndex;
      return this.list_.dataModel.item(listIndex)['modelIndex'];
    },

    /**
     * Callback from the selection model when the selection changes.
     * @private
     * @param {!cr.Event} e Event with change info.
     */
    selectionChanged_: function(e) {
      var selectedIndex = this.list_.selectionModel.selectedIndex;
      var engine = selectedIndex != -1 ?
          this.list_.dataModel.item(selectedIndex) : null;

      $('editSearchEngineButton').disabled = engine == null;
      $('makeDefaultSearchEngineButton').disabled =
          !(engine && engine['canBeDefault']);
    },
  };

  SearchEngineManager.updateSearchEngineList = function(engineList) {
    SearchEngineManager.getInstance().updateSearchEngineList_(engineList);
  };

  // Export
  return {
    SearchEngineManager: SearchEngineManager
  };

});

