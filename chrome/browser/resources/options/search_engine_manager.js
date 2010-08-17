// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  const OptionsPage = options.OptionsPage;
  const ArrayDataModel = cr.ui.ArrayDataModel;
  const List = cr.ui.List;
  const ListItem = cr.ui.ListItem;
  const ListSelectionController = cr.ui.ListSelectionController;
  const ListSingleSelectionModel = cr.ui.ListSingleSelectionModel;

  /////////////////////////////////////////////////////////////////////////////
  // SearchEngineList class:

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
        titleEl.className = 'heading';
        titleEl.textContent = engine['heading'];
        this.appendChild(titleEl);
      } else {
        var nameEl = this.ownerDocument.createElement('div');
        nameEl.className = 'name';
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

  /**
   * Creates a selection controller with a delegate that controls whether or
   * not individual items are selectable. This is used for lists containing
   * subgroups with headings that are in the list, since the headers themselves
   * should not be selectable.
   *
   * @param {cr.ui.ListSelectionModel} selectionModel The selection model to
   *     interact with.
   * @param {*} selectabilityDelegate A delegate responding to
   *     canSelectIndex(index).
   *
   * @constructor
   * @extends {!cr.ui.ListSelectionController}
   */
  function InlineHeaderSelectionController(selectionModel,
                                           selectabilityDelegate) {
    ListSelectionController.call(this, selectionModel);
    this.selectabilityDelegate_ = selectabilityDelegate;
  }

  InlineHeaderSelectionController.prototype = {
    __proto__: ListSelectionController.prototype,

    /** @inheritDoc */
    getIndexBelow: function(index) {
      var next = ListSelectionController.prototype.getIndexBelow.call(this,
                                                                      index);
      if (next == -1 || this.canSelect(next))
        return next;
      return this.getIndexBelow(next);
    },

    /** @inheritDoc */
    getNextIndex: function(index) {
      return this.getIndexBelow(index);
    },

    /** @inheritDoc */
    getIndexAbove: function(index) {
      var previous = ListSelectionController.prototype.getIndexAbove.call(
          this, index);
      if (previous == -1 || this.canSelect(previous))
        return previous;
      return this.getIndexAbove(previous);
    },

    /** @inheritDoc */
    getPreviousIndex: function(index) {
      return this.getIndexAbove(index);
    },

    /** @inheritDoc */
    getFirstIndex: function(index) {
      var first = ListSelectionController.prototype.getFirstIndex.call(this);
      if (this.canSelect(first))
        return first;
      return this.getNextIndex(first);
    },

    /** @inheritDoc */
    getLastIndex: function(index) {
      var last = ListSelectionController.prototype.getLastIndex.call(this);
      if (this.canSelect(last))
        return last;
      return this.getPreviousIndex(last);
    },

    /** @inheritDoc */
    handleMouseDownUp: function(e, index) {
      if (this.canSelect(index)) {
        ListSelectionController.prototype.handleMouseDownUp.call(
            this, e, index);
      }
    },

    /**
     * Returns true if the given index is selectable.
     * @private
     * @param {number} index The index to check.
     */
    canSelect: function(index) {
      return this.selectabilityDelegate_.canSelectIndex(index);
    }
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
      return new InlineHeaderSelectionController(sm, this);
    },

    /**
     * Returns true if the given item is selectable.
     * @param {number} index The index to check.
     */
    canSelectIndex: function(index) {
      return !this.dataModel.item(index).hasOwnProperty('heading');
    }
  };

  /////////////////////////////////////////////////////////////////////////////
  // SearchEngineManager class:

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
      SearchEngineList.decorate(this.list_);
      var selectionModel = new ListSingleSelectionModel
      this.list_.selectionModel = selectionModel;

      selectionModel.addEventListener('change',
          cr.bind(this.selectionChanged_, this));

      var self = this;
      $('addSearchEngineButton').onclick = function(event) {
        // TODO(stuartmorgan): Show an overlay to edit the new search engine.
      };
      $('removeSearchEngineButton').onclick = function(event) {
        chrome.send('removeSearchEngine', [self.selectedModelIndex_]);
      };
      $('editSearchEngineButton').onclick = function(event) {
        // TODO(stuartmorgan): Show an overlay to edit the selected
        // search engine.
      };
      $('makeDefaultSearchEngineButton').onclick = function(event) {
        chrome.send('managerSetDefaultSearchEngine',
                    [self.selectedModelIndex_]);
      };

      // Remove Windows-style accelerators from button labels.
      // TODO(stuartmorgan): Remove this once the strings are updated.
      $('addSearchEngineButton').textContent =
          localStrings.getStringWithoutAccelerator('addSearchEngineButton');
      $('removeSearchEngineButton').textContent =
          localStrings.getStringWithoutAccelerator('removeSearchEngineButton');

      this.addEventListener('visibleChange', function(event) {
          $('searchEngineList').redraw();
      });
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

      $('removeSearchEngineButton').disabled =
          !(engine && engine['canBeRemoved']);
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

