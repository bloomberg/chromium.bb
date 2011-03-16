// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  const ArrayDataModel = cr.ui.ArrayDataModel;
  const List = cr.ui.List;
  const ListItem = cr.ui.ListItem;

  /**
   * Creates a new autocomplete list item.
   * @param {Object} pageInfo The page this item represents.
   * @constructor
   * @extends {cr.ui.ListItem}
   */
  function AutocompleteListItem(pageInfo) {
    var el = cr.doc.createElement('div');
    el.pageInfo_ = pageInfo;
    AutocompleteListItem.decorate(el);
    return el;
  }

  /**
   * Decorates an element as an autocomplete list item.
   * @param {!HTMLElement} el The element to decorate.
   */
  AutocompleteListItem.decorate = function(el) {
    el.__proto__ = AutocompleteListItem.prototype;
    el.decorate();
  };

  AutocompleteListItem.prototype = {
    __proto__: ListItem.prototype,

    /** @inheritDoc */
    decorate: function() {
      ListItem.prototype.decorate.call(this);

      var title = this.pageInfo_['title'];
      var url = this.pageInfo_['displayURL'];
      var titleEl = this.ownerDocument.createElement('span');
      titleEl.className = 'title';
      titleEl.textContent = title || url;
      this.appendChild(titleEl);

      if (title && title.length > 0 && url != title) {
        var separatorEl = this.ownerDocument.createTextNode(' - ');
        this.appendChild(separatorEl);

        var urlEl = this.ownerDocument.createElement('span');
        urlEl.className = 'url';
        urlEl.textContent = url;
        this.appendChild(urlEl);
      }
    },
  };

  /**
   * Creates a new autocomplete list popup.
   * @constructor
   * @extends {cr.ui.List}
   */
  var AutocompleteList = cr.ui.define('list');

  AutocompleteList.prototype = {
    __proto__: List.prototype,

    /**
     * The text field the autocomplete popup is currently attached to, if any.
     * @type {HTMLElement}
     * @private
     */
    targetInput_: null,

    /**
     * Keydown event listener to attach to a text field.
     * @type {Function}
     * @private
     */
    textFieldKeyHandler_: null,

    /** @inheritDoc */
    decorate: function() {
      List.prototype.decorate.call(this);
      this.classList.add('autocomplete-suggestions');
      this.selectionModel = new cr.ui.ListSingleSelectionModel;

      this.textFieldKeyHandler_ = this.handleAutocompleteKeydown_.bind(this);
    },

    /** @inheritDoc */
    createItem: function(pageInfo) {
      return new AutocompleteListItem(pageInfo);
    },

    set suggestions(suggestions) {
      this.dataModel = new ArrayDataModel(suggestions);
      this.hidden = suggestions.length == 0;
    },

    /**
     * Attaches the popup to the given input element. Requires
     * that the input be wrapped in a block-level container of the same width.
     * @param {HTMLElement} input The input element to attach to.
     */
    attachToInput: function(input) {
      if (this.targetInput_ == input)
        return;

      this.detach();
      this.targetInput_ = input;
      input.parentNode.appendChild(this);
      const MENU_BOTTOM_OFFSET = 3;
      this.style.top = input.getBoundingClientRect().height +
          MENU_BOTTOM_OFFSET + 'px';
      // Start hidden; when the data model gets results the list will show.
      this.hidden = true;

      input.addEventListener('keydown', this.textFieldKeyHandler_);
    },

    /**
     * Detaches the autocomplete popup from its current input element, if any.
     */
    detach: function() {
      if (!this.targetInput_)
        return;

      this.targetInput_.removeEventListener('keydown',
                                            this.textFieldKeyHandler_);
      this.targetInput_ = null;
      var parentNode = this.parentNode;
      if (parentNode)
        parentNode.removeChild(this);
      this.suggestions = [];
    },

    /**
     * The text field the autocomplete popup is currently attached to, if any.
     * @return {HTMLElement}
     */
    get targetInput() {
      return this.targetInput_;
    },

    /**
     * Handles input field key events that should be interpreted as autocomplete
     * commands.
     * @param {Event} event The keydown event.
     * @private
     */
    handleAutocompleteKeydown_: function(event) {
      switch (event.keyIdentifier) {
        case 'U+001B':  // Esc
        case 'Enter':
          this.suggestions = [];
          event.preventDefault();
          break;
        case 'Up':
        case 'Down':
          this.dispatchEvent(event);
          break;
      }
    },
  };

  return {
    AutocompleteList: AutocompleteList
  };
});
