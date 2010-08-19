// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options.search_engines', function() {
  const List = cr.ui.List;
  const ListInlineHeaderSelectionController =
      options.ListInlineHeaderSelectionController;
  const ListItem = cr.ui.ListItem;

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
    __proto__: ListItem.prototype,

    /** @inheritDoc */
    decorate: function() {
      ListItem.prototype.decorate.call(this);

      var engine = this.searchEngine_;
      if (engine['heading']) {
        var titleEl = this.ownerDocument.createElement('div');
        titleEl.textContent = engine['heading'];
        this.classList.add('heading');
        this.appendChild(titleEl);
      } else {
        var nameEl = this.ownerDocument.createElement('div');
        nameEl.className = 'name';
        nameEl.classList.add('favicon-cell');
        nameEl.textContent = engine['name'];
        nameEl.style.backgroundImage = url('chrome://favicon/iconurl/' +
                                           engine['iconURL']);

        var keywordEl = this.ownerDocument.createElement('div');
        keywordEl.className = 'keyword';
        keywordEl.textContent = engine['keyword'];

        this.appendChild(nameEl);
        this.appendChild(keywordEl);

        if (engine['default'])
          this.classList.add('default');
      }
    },
  };

  var SearchEngineList = cr.ui.define('list');

  SearchEngineList.prototype = {
    __proto__: List.prototype,

    /** @inheritDoc */
    createItem: function(searchEngine) {
      return new SearchEngineListItem(searchEngine);
    },

    /** @inheritDoc */
    createSelectionController: function(sm) {
      return new ListInlineHeaderSelectionController(sm, this);
    },

    /**
     * Returns true if the given item is selectable.
     * @param {number} index The index to check.
     */
    canSelectIndex: function(index) {
      return !this.dataModel.item(index).hasOwnProperty('heading');
    }
  };

  // Export
  return {
    SearchEngineList: SearchEngineList
  };

});

