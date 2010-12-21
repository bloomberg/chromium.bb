// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options.search_engines', function() {
  const DeletableItem = options.DeletableItem;
  const DeletableItemList = options.DeletableItemList;
  const ListInlineHeaderSelectionController =
      options.ListInlineHeaderSelectionController;

  /**
   * Creates a new search engine list item.
   * @param {Object} searchEnigne The search engine this represents.
   * @constructor
   * @extends {cr.ui.ListItem}
   */
  function SearchEngineListItem(searchEngine) {
    var el = cr.doc.createElement('div');
    el.searchEngine_ = searchEngine;
    SearchEngineListItem.decorate(el);
    return el;
  }

  /**
   * Decorates an element as a search engine list item.
   * @param {!HTMLElement} el The element to decorate.
   */
  SearchEngineListItem.decorate = function(el) {
    el.__proto__ = SearchEngineListItem.prototype;
    el.decorate();
  };

  SearchEngineListItem.prototype = {
    __proto__: DeletableItem.prototype,

    /** @inheritDoc */
    decorate: function() {
      DeletableItem.prototype.decorate.call(this);

      var engine = this.searchEngine_;

      if (engine['heading'])
        this.classList.add('heading');
      else if (engine['default'])
        this.classList.add('default');

      this.deletable = engine['canBeRemoved'];

      var nameEl = this.ownerDocument.createElement('div');
      nameEl.className = 'name';
      if (engine['heading']) {
        nameEl.textContent = engine['heading'];
      } else {
        nameEl.textContent = engine['name'];
        nameEl.classList.add('favicon-cell');
        nameEl.style.backgroundImage = url('chrome://favicon/iconurl/' +
                                           engine['iconURL']);
      }
      this.contentElement.appendChild(nameEl);

      var keywordEl = this.ownerDocument.createElement('div');
      keywordEl.className = 'keyword';
      keywordEl.textContent = engine['heading'] ?
          localStrings.getString('searchEngineTableKeywordHeader') :
          engine['keyword'];
      this.contentElement.appendChild(keywordEl);
    },
  };

  var SearchEngineList = cr.ui.define('list');

  SearchEngineList.prototype = {
    __proto__: DeletableItemList.prototype,

    /** @inheritDoc */
    createItem: function(searchEngine) {
      return new SearchEngineListItem(searchEngine);
    },

    /** @inheritDoc */
    createSelectionController: function(sm) {
      return new ListInlineHeaderSelectionController(sm, this);
    },

    /** @inheritDoc */
    deleteItemAtIndex: function(index) {
      var modelIndex = this.dataModel.item(index)['modelIndex']
      chrome.send('removeSearchEngine', [String(modelIndex)]);
    },

    /**
     * Returns true if the given item is selectable.
     * @param {number} index The index to check.
     */
    canSelectIndex: function(index) {
      return !this.dataModel.item(index).hasOwnProperty('heading');
    },
  };

  // Export
  return {
    SearchEngineList: SearchEngineList
  };

});

