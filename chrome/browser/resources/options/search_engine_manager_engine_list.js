// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options.search_engines', function() {
  const InlineEditableItemList = options.InlineEditableItemList;
  const InlineEditableItem = options.InlineEditableItem;
  const ListSelectionController = cr.ui.ListSelectionController;

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

      if (engine['default'])
        this.classList.add('default');

      this.deletable = engine['canBeRemoved'];

      // Construct the name column.
      var nameColEl = this.ownerDocument.createElement('div');
      nameColEl.className = 'name-column';
      this.contentElement.appendChild(nameColEl);

      // Add the favicon.
      var faviconDivEl = this.ownerDocument.createElement('div');
      faviconDivEl.className = 'favicon';
      var imgEl = this.ownerDocument.createElement('img');
      imgEl.src = 'chrome://favicon/iconurl/' + engine['iconURL'];
      faviconDivEl.appendChild(imgEl);
      nameColEl.appendChild(faviconDivEl);

      var nameEl = this.createEditableTextCell(engine['name'],
                                               this.isPlaceholder_);
      nameColEl.appendChild(nameEl);

      // Then the keyword column.
      var keywordEl = this.createEditableTextCell(engine['keyword'],
                                                  this.isPlaceholder_);
      keywordEl.className = 'keyword-column';
      this.contentElement.appendChild(keywordEl);

      // And the URL column.
      var urlEl = this.createEditableTextCell(engine['url'],
                                              this.isPlaceholder_);
      var urlWithButtonEl = this.ownerDocument.createElement('div');
      urlWithButtonEl.appendChild(urlEl);
      urlWithButtonEl.className = 'url-column';
      this.contentElement.appendChild(urlWithButtonEl);
      // Add the Make Default button. Temporary until drag-and-drop re-ordering
      // is implemented. When this is removed, remove the extra div above.
      if (engine['canBeDefault']) {
        var makeDefaultButtonEl = this.ownerDocument.createElement('button');
        makeDefaultButtonEl.className = "raw-button";
        makeDefaultButtonEl.textContent =
            templateData.makeDefaultSearchEngineButton;
        makeDefaultButtonEl.onclick = function(e) {
          chrome.send('managerSetDefaultSearchEngine', [engine['modelIndex']]);
        };
        // Don't select the row when clicking the button.
        makeDefaultButtonEl.onmousedown = function(e) {
          e.stopPropagation();
        };
        urlWithButtonEl.appendChild(makeDefaultButtonEl);
      }

      // Do final adjustment to the input fields.
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

      // Listen for edit events.
      this.addEventListener('edit', this.onEditStarted_.bind(this));
      this.addEventListener('canceledit', this.onEditCancelled_.bind(this));
      this.addEventListener('commitedit', this.onEditCommitted_.bind(this));
    },

    /** @inheritDoc */
    get currentInputIsValid() {
      return !this.waitingForValidation_ && this.currentlyValid_;
    },

    /** @inheritDoc */
    get hasBeenEdited() {
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
      this.startFieldValidation_();
    },

    /**
     * Called when committing an edit; updates the model.
     * @param {Event} e The end event.
     * @private
     */
    onEditCommitted_: function(e) {
      chrome.send('searchEngineEditCompleted', this.getInputFieldValues_());
    },

    /**
     * Called when cancelling an edit; informs the model and resets the control
     * states.
     * @param {Event} e The cancel event.
     * @private
     */
    onEditCancelled_: function() {
      chrome.send('searchEngineEditCancelled');

      if (this.isPlaceholder_) {
        var engine = this.searchEngine_;
        this.nameField_.value = '';
        this.keywordField_.value = '';
        this.urlField_.value = '';
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
      // checkmark/exclamation mark icons and tooltips showing the errors.
      if (validity['name']) {
        this.nameField_.setCustomValidity('');
      } else {
        this.nameField_.setCustomValidity(
            templateData.editSearchEngineInvalidTitleToolTip);
      }

      if (validity['keyword']) {
        this.keywordField_.setCustomValidity('');
      } else {
        this.keywordField_.setCustomValidity(
            templateData.editSearchEngineInvalidKeywordToolTip);
      }

      if (validity['url']) {
        this.urlField_.setCustomValidity('');
      } else {
        this.urlField_.setCustomValidity(
            templateData.editSearchEngineInvalidURLToolTip);
      }

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
    deleteItemAtIndex: function(index) {
      var modelIndex = this.dataModel.item(index)['modelIndex']
      chrome.send('removeSearchEngine', [String(modelIndex)]);
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
      if (!currentSelection)
        return;
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

