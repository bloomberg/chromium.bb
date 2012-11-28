// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options.dictionary_words', function() {
  /** @const */ var InlineEditableItemList = options.InlineEditableItemList;
  /** @const */ var InlineEditableItem = options.InlineEditableItem;

  /**
   * Creates a new dictionary word list item.
   * @param {string} dictionaryWord The dictionary word.
   * @param {function(string)} addDictionaryWordCallback Callback for
   * adding a dictionary word.
   * @constructor
   * @extends {cr.ui.InlineEditableItem}
   */
  function DictionaryWordsListItem(dictionaryWord, addDictionaryWordCallback) {
    var el = cr.doc.createElement('div');
    el.dictionaryWord_ = dictionaryWord;
    el.addDictionaryWordCallback_ = addDictionaryWordCallback;
    DictionaryWordsListItem.decorate(el);
    return el;
  }

  /**
   * Decorates an HTML element as a dictionary word list item.
   * @param {HTMLElement} el The element to decorate.
   */
  DictionaryWordsListItem.decorate = function(el) {
    el.__proto__ = DictionaryWordsListItem.prototype;
    el.decorate();
  };

  DictionaryWordsListItem.prototype = {
    __proto__: InlineEditableItem.prototype,

    /** @override */
    decorate: function() {
      InlineEditableItem.prototype.decorate.call(this);
      if (this.dictionaryWord_ == '')
        this.isPlaceholder = true;
      else
        this.editable = false;

      var wordEl = this.createEditableTextCell(this.dictionaryWord_);
      wordEl.classList.add('weakrtl');
      wordEl.classList.add('language-dictionary-overlay-word-list-item');
      this.contentElement.appendChild(wordEl);

      var wordField = wordEl.querySelector('input');
      wordField.classList.add('language-dictionary-overlay-word-list-item');
      if (this.isPlaceholder) {
        wordField.placeholder =
            loadTimeData.getString('languageDictionaryOverlayAddWordLabel');
      }

      this.addEventListener('commitedit', this.onEditCommitted_.bind(this));
    },

    /** @override */
    get hasBeenEdited() {
      return this.querySelector('input').value.length > 0;
    },

    /**
     * Adds a word to the dictionary.
     * @param {Event} e Edit committed event.
     * @private
     */
    onEditCommitted_: function(e) {
      var input = e.currentTarget.querySelector('input');
      this.addDictionaryWordCallback_(input.value);
      input.value = '';
    },
  };

  /**
   * A list of words in the dictionary.
   * @extends {cr.ui.InlineEditableItemList}
   */
  var DictionaryWordsList = cr.ui.define('list');

  DictionaryWordsList.prototype = {
    __proto__: InlineEditableItemList.prototype,

    /**
     * The function to notify that the word list has changed.
     * @type {function()}
     */
    onWordListChanged: null,

    /**
     * The list of all words in the dictionary. Used to generate a filtered word
     * list in the |search(searchTerm)| method.
     * @type {Array}
     * @private
     */
    allWordsList_: null,

    /**
     * Add a dictionary word.
     * @param {string} dictionaryWord The word to add.
     * @private
     */
    addDictionaryWord_: function(dictionaryWord) {
      this.allWordsList_.push(dictionaryWord);
      this.dataModel.splice(this.dataModel.length - 1, 0, dictionaryWord);
      this.onWordListChanged();
      chrome.send('addDictionaryWord', [dictionaryWord]);
    },

    /**
     * Search the list for the matching words.
     * @param {string} searchTerm The search term.
     */
    search: function(searchTerm) {
      var filteredWordList = this.allWordsList_.filter(
          function(element, index, array) {
            return element.indexOf(searchTerm) > -1;
          });
      filteredWordList.push('');
      this.dataModel = new cr.ui.ArrayDataModel(filteredWordList);
      this.onWordListChanged();
    },

    /**
     * Set the list of dictionary words.
     * @param {Array} entries The list of dictionary words.
     */
    setWordList: function(entries) {
      this.allWordsList_ = entries.slice(0);
      // Empty string is a placeholder for entering new words.
      entries.push('');
      this.dataModel = new cr.ui.ArrayDataModel(entries);
      this.onWordListChanged();
    },

    /**
     * True if the data model contains no words, otherwise false.
     * @type {boolean}
     */
    get empty() {
      // A data model without dictionary words contains one placeholder for
      // entering new words.
      return this.dataModel.length < 2;
    },

    /** @override */
    createItem: function(dictionaryWord) {
      return new DictionaryWordsListItem(
          dictionaryWord,
          this.addDictionaryWord_.bind(this));
    },

    /** @override */
    deleteItemAtIndex: function(index) {
      // The last element in the data model is an undeletable placeholder for
      // entering new words.
      assert(index > -1 && index < this.dataModel.length - 1);
      var item = this.dataModel.item(index);
      var allWordsListIndex = this.allWordsList_.indexOf(item);
      assert(allWordsListIndex > -1);
      this.allWordsList_.splice(allWordsListIndex, 1);
      this.dataModel.splice(index, 1);
      this.onWordListChanged();
      chrome.send('removeDictionaryWord', [item]);
    },

    /** @override */
    shouldFocusPlaceholder: function() {
      return false;
    },
  };

  return {
    DictionaryWordsList: DictionaryWordsList
  };

});

