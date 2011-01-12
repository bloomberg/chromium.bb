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

      selectionModel.addEventListener('change',
          this.selectionChanged_.bind(this));

      var self = this;
      // This is a temporary hack to allow the "Make Default" button to
      // continue working despite the new list behavior of removing selection
      // on focus loss.
      // Once drag-and-drop is supported, so items can be moved into the default
      // section, this button will go away entirely.
      $('makeDefaultSearchEngineButton').onmousedown = function(event) {
        self.pendingDefaultEngine_ = self.list_.selectedItem;
      };
      $('makeDefaultSearchEngineButton').onclick = function(event) {
        chrome.send('managerSetDefaultSearchEngine',
                    [self.pendingDefaultEngine_['modelIndex']]);
        self.pendingDefaultEngine_ = null;
      };
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

    /**
     * Callback from the selection model when the selection changes.
     * @private
     * @param {!cr.Event} e Event with change info.
     */
    selectionChanged_: function(e) {
      var engine = this.list_.selectedItem || this.pendingDefaultEngine_;
      $('makeDefaultSearchEngineButton').disabled =
          !(engine && engine['canBeDefault']);
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

