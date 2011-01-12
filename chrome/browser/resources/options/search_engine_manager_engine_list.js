// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options.search_engines', function() {
  const InlineEditableItemList = options.InlineEditableItemList;
  const InlineEditableItem = options.InlineEditableItem;
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
    __proto__: InlineEditableItem.prototype,

    /**
     * Input field for editing the engine name.
     * @type {HTMLElement}
     * @private
     */
    nameField_: null,

    /**
     * Input field for editing the engine keyword.
     * @type {HTMLElement}
     * @private
     */
    keywordField_: null,

    /**
     * Input field for editing the engine url.
     * @type {HTMLElement}
     * @private
     */
    urlField_: null,

    /**
     * Whether or not this is a placeholder for adding an engine.
     * @type {boolean}
     * @private
     */
    isPlaceholder_: false,

    /**
     * Whether or not an input validation request is currently outstanding.
     * @type {boolean}
     * @private
     */
    waitingForValidation_: false,

    /**
     * Whether or not the current set of input is known to be valid.
     * @type {boolean}
     * @private
     */
    currentlyValid_: false,

    /** @inheritDoc */
    decorate: function() {
      InlineEditableItem.prototype.decorate.call(this);

      var engine = this.searchEngine_;

      if (engine['modelIndex'] == '-1') {
        this.isPlaceholder_ = true;
        engine['name'] = '';
        engine['keyword'] = '';
        engine['url'] = '';
      }

      this.currentlyValid_ = !this.isPlaceholder_;

      if (engine['heading']) {
        this.classList.add('heading');
        this.editable = false;
      } else if (engine['default']) {
        this.classList.add('default');
      }

      this.deletable = engine['canBeRemoved'];

      var nameText = engine['name'];
      var keywordText = engine['keyword'];
      var urlText = engine['url'];
      if (engine['heading']) {
        nameText = engine['heading'];
        keywordText = localStrings.getString('searchEngineTableKeywordHeader');
        urlText = localStrings.getString('searchEngineTableURLHeader');
      }

      // Construct the name column.
      var nameColEl = this.ownerDocument.createElement('div');
      nameColEl.className = 'name-column';
      this.contentElement.appendChild(nameColEl);

      // For non-heading rows, start with a favicon.
      if (!engine['heading']) {
        var faviconDivEl = this.ownerDocument.createElement('div');
        faviconDivEl.className = 'favicon';
        var imgEl = this.ownerDocument.createElement('img');
        imgEl.src = 'chrome://favicon/iconurl/' + engine['iconURL'];
        faviconDivEl.appendChild(imgEl);
        nameColEl.appendChild(faviconDivEl);
      }

      var nameEl = this.createEditableTextCell_(nameText);
      nameColEl.appendChild(nameEl);

      // Then the keyword column.
      var keywordEl = this.createEditableTextCell_(keywordText);
      keywordEl.className = 'keyword-column';
      this.contentElement.appendChild(keywordEl);

      // And the URL column.
      var urlEl = this.createEditableTextCell_(urlText);
      urlEl.className = 'url-column';
      this.contentElement.appendChild(urlEl);

      // Do final adjustment to the input fields.
      if (!engine['heading']) {
        this.nameField_ = nameEl.querySelector('input');
        this.keywordField_ = keywordEl.querySelector('input');
        this.urlField_ = urlEl.querySelector('input');

        if (engine['urlLocked'])
          this.urlField_.disabled = true;

        if (this.isPlaceholder_) {
          this.nameField_.placeholder =
              localStrings.getString('searchEngineTableNamePlaceholder');
          this.keywordField_.placeholder =
              localStrings.getString('searchEngineTableKeywordPlaceholder');
          this.urlField_.placeholder =
              localStrings.getString('searchEngineTableURLPlaceholder');
        }

        var fields = [ this.nameField_, this.keywordField_, this.urlField_ ];
          for (var i = 0; i < fields.length; i++) {
          fields[i].oninput = this.startFieldValidation_.bind(this);
        }
      }

      // Listen for edit events.
      this.addEventListener('edit', this.onEditStarted_.bind(this));
      this.addEventListener('canceledit', this.onEditCancelled_.bind(this));
      this.addEventListener('commitedit', this.onEditCommitted_.bind(this));
    },

    /**
     * Returns a div containing an <input>, as well as static text if needed.
     * @param {string} text The text of the cell.
     * @return {HTMLElement} The HTML element for the cell.
     * @private
     */
    createEditableTextCell_: function(text) {
      var container = this.ownerDocument.createElement('div');

      if (!this.isPlaceholder_) {
        var textEl = this.ownerDocument.createElement('div');
        textEl.className = 'static-text';
        textEl.textContent = text;
        textEl.setAttribute('editmode', false);
        container.appendChild(textEl);
      }

      var inputEl = this.ownerDocument.createElement('input');
      inputEl.type = 'text';
      inputEl.value = text;
      if (!this.isPlaceholder_) {
        inputEl.setAttribute('editmode', true);
        inputEl.staticVersion = textEl;
      }
      container.appendChild(inputEl);

      return container;
    },

    /** @inheritDoc */
    get initialFocusElement() {
      return this.nameField_;
    },

    /** @inheritDoc */
    get currentInputIsValid() {
      return !this.waitingForValidation_ && this.currentlyValid_;
    },

    /** @inheritDoc */
    hasBeenEdited: function(e) {
      var engine = this.searchEngine_;
      return this.nameField_.value != engine['name'] ||
             this.keywordField_.value != engine['keyword'] ||
             this.urlField_.value != engine['url'];
    },

    /**
     * Called when entering edit mode; starts an edit session in the model.
     * @param {Event} e The edit event.
     * @private
     */
    onEditStarted_: function(e) {
      var editIndex = this.searchEngine_['modelIndex'];
      chrome.send('editSearchEngine', [String(editIndex)]);
    },

    /**
     * Called when committing an edit; updates the model.
     * @param {Event} e The end event.
     * @private
     */
    onEditCommitted_: function(e) {
      chrome.send('searchEngineEditCompleted', this.getInputFieldValues_());
      // Update the static version immediately to prevent flickering before
      // the model update callback updates the UI.
      var editFields = [ this.nameField_, this.keywordField_, this.urlField_ ];
      for (var i = 0; i < editFields.length; i++) {
        var staticLabel = editFields[i].staticVersion;
        if (staticLabel)
          staticLabel.textContent = editFields[i].value;
      }
    },

    /**
     * Called when cancelling an edit; informs the model and resets the control
     * states.
     * @param {Event} e The cancel event.
     * @private
     */
    onEditCancelled_: function() {
      chrome.send('searchEngineEditCancelled');
      var engine = this.searchEngine_;
      this.nameField_.value = engine['name'];
      this.keywordField_.value = engine['keyword'];
      this.urlField_.value = engine['url'];

      var editFields = [ this.nameField_, this.keywordField_, this.urlField_ ];
      for (var i = 0; i < editFields.length; i++) {
        editFields[i].classList.remove('invalid');
      }
      this.currentlyValid_ = !this.isPlaceholder_;
    },

    /**
     * Returns the input field values as an array suitable for passing to
     * chrome.send. The order of the array is important.
     * @private
     * @return {array} The current input field values.
     */
    getInputFieldValues_: function() {
      return [ this.nameField_.value,
               this.keywordField_.value,
               this.urlField_.value ];
    },

    /**
     * Begins the process of asynchronously validing the input fields.
     * @private
     */
    startFieldValidation_: function() {
      this.waitingForValidation_ = true;
      var args = this.getInputFieldValues_();
      args.push(this.searchEngine_['modelIndex']);
      chrome.send('checkSearchEngineInfoValidity', args);
    },

    /**
     * Callback for the completion of an input validition check.
     * @param {Object} validity A dictionary of validitation results.
     */
    validationComplete: function(validity) {
      this.waitingForValidation_ = false;
      // TODO(stuartmorgan): Implement the full validation UI with
      // checkmark/exclamation mark icons and tooltips.
      if (validity['name'])
        this.nameField_.classList.remove('invalid');
      else
        this.nameField_.classList.add('invalid');

      if (validity['keyword'])
        this.keywordField_.classList.remove('invalid');
      else
        this.keywordField_.classList.add('invalid');

      if (validity['url'])
        this.urlField_.classList.remove('invalid');
      else
        this.urlField_.classList.add('invalid');

      this.currentlyValid_ = validity['name'] && validity['keyword'] &&
          validity['url'];
    },
  };

  var SearchEngineList = cr.ui.define('list');

  SearchEngineList.prototype = {
    __proto__: InlineEditableItemList.prototype,

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

    /**
     * Passes the results of an input validation check to the requesting row
     * if it's still being edited.
     * @param {number} modelIndex The model index of the item that was checked.
     * @param {Object} validity A dictionary of validitation results.
     */
    validationComplete: function(validity, modelIndex) {
      // If it's not still being edited, it no longer matters.
      var currentSelection = this.selectedItem;
      var listItem = this.getListItem(currentSelection);
      if (listItem.editing && currentSelection['modelIndex'] == modelIndex)
        listItem.validationComplete(validity);
    },
  };

  // Export
  return {
    SearchEngineList: SearchEngineList
  };

});

