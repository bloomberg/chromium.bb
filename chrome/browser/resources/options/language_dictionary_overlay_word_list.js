// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options.dictionary_words', function() {
  var InlineEditableItemList = options.InlineEditableItemList;
  var InlineEditableItem = options.InlineEditableItem;

  function DictionaryWordsListItem(dictionaryWord) {
    var el = cr.doc.createElement('div');
    el.dictionaryWord_ = dictionaryWord;
    DictionaryWordsListItem.decorate(el);
    return el;
  }

  DictionaryWordsListItem.decorate = function(el) {
    el.__proto__ = DictionaryWordsListItem.prototype;
    el.decorate();
  };

  DictionaryWordsListItem.prototype = {
    __proto__: InlineEditableItem.prototype,

    wordField_: null,

    decorate: function() {
      InlineEditableItem.prototype.decorate.call(this);
      if (this.dictionaryWord_ == '')
        this.isPlaceholder = true;
      else
        this.editable = false;

      var wordEl = this.createEditableTextCell(this.dictionaryWord_);
      wordEl.classList.add('weakrtl');
      wordEl.classList.add('dictionary-word-list-item');
      this.contentElement.appendChild(wordEl);

      this.wordField_ = wordEl.querySelector('input');
      this.wordField_.classList.add('dictionary-word-list-item');
      if (this.isPlaceholder) {
        this.wordField_.placeholder =
            loadTimeData.getString('languageDictionaryOverlayAddWordLabel');
      }

      this.addEventListener('commitedit', this.onEditCommitted_.bind(this));
    },

    get hasBeenEdited() {
      return this.wordField_.value != this.dictionaryWord_;
    },

    onEditCommitted_: function(e) {
      chrome.send('addDictionaryWord', [this.wordField_.value]);
    },
  };

  var DictionaryWordsList = cr.ui.define('list');

  DictionaryWordsList.prototype = {
    __proto__: InlineEditableItemList.prototype,

    createItem: function(dictionaryWord) {
      return new DictionaryWordsListItem(dictionaryWord);
    },

    deleteItemAtIndex: function(index) {
      chrome.send('removeDictionaryWord', [String(index)]);
    },
  };

  return {
    DictionaryWordsList: DictionaryWordsList
  };

});

