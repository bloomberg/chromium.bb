// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-edit-dictionary-page' is a sub-page for editing
 * the "dictionary" of custom words used for spell check.
 */
Polymer({
  is: 'settings-edit-dictionary-page',

  properties: {
    /** @private {string} */
    newWordValue_: String,

    /** @private {!Array<string>} */
    words_: {
      type: Array,
      value: function() { return []; },
    },
  },

  /** @type {LanguageSettingsPrivate} */
  languageSettingsPrivate: null,

  ready: function() {
    this.languageSettingsPrivate =
        settings.languageSettingsPrivateApiForTest ||
        /** @type {!LanguageSettingsPrivate} */(chrome.languageSettingsPrivate);

    this.languageSettingsPrivate.getSpellcheckWords(function(words) {
      this.words_ = words;
    }.bind(this));

    // Updates are applied locally so they appear immediately, but we should
    // listen for changes in case they come from elsewhere.
    this.languageSettingsPrivate.onCustomDictionaryChanged.addListener(
        this.onCustomDictionaryChanged_.bind(this));

    // Add a key handler for the paper-input.
    this.$.keys.target = this.$.newWord;
  },

  /**
   * Check if the new word text-field is empty.
   * @private
   * @param {string} value
   * @return {boolean} true if value is empty, false otherwise.
   */
  validateWord_: function(value) {
    return !!value.trim();
  },

  /**
   * Handles updates to the word list. Additions triggered by this element are
   * de-duped so the word list remains a set. Words are appended to the end
   * instead of re-sorting the list so it's clear what words were added.
   * @param {!Array<string>} added
   * @param {!Array<string>} removed
   */
  onCustomDictionaryChanged_: function(added, removed) {
    for (var i = 0; i < removed.length; i++)
      this.arrayDelete('words_', removed[i]);

    for (var i = 0; i < added.length; i++) {
      if (this.words_.indexOf(added[i]) == -1)
        this.push('words_', added[i]);
    }
  },

  /**
   * Handles Enter and Escape key presses for the paper-input.
   * @param {!{detail: !{key: string}}} e
   */
  onKeysPress_: function(e) {
    if (e.detail.key == 'enter')
      this.addWordFromInput_();
    else if (e.detail.key == 'esc')
      e.detail.keyboardEvent.target.value = '';
  },

  /**
   * Handles tapping on the Add Word button.
   */
  onAddWordTap_: function(e) {
    this.addWordFromInput_();
    this.$.newWord.focus();
  },

  /**
   * Handles tapping on a paper-item's Remove Word icon button.
   * @param {!{model: !{item: string}}} e
   */
  onRemoveWordTap_: function(e) {
    this.languageSettingsPrivate.removeSpellcheckWord(e.model.item);
    this.arrayDelete('words_', e.model.item);
  },

  /**
   * Adds the word in the paper-input to the dictionary, also appending it
   * to the end of the list of words shown to the user.
   */
  addWordFromInput_: function() {
    // Spaces are allowed, but removing leading and trailing whitespace.
    var word = this.newWordValue_.trim();
    this.newWordValue_ = '';
    if (!word)
      return;

    var index = this.words_.indexOf(word);
    if (index == -1) {
      this.languageSettingsPrivate.addSpellcheckWord(word);
      this.push('words_', word);
      index = this.words_.length - 1;
    }

    // Scroll to the word (usually the bottom, or to the index if the word
    // is already present).
    this.async(function(){
      this.root.querySelector('#list').scrollToIndex(index);
    });
  },

  /**
   * Checks if any words exists in the dictionary.
   * @private
   * @return {boolean}
   */
  hasWords_: function() {
    return this.words_.length > 0;
  }
});
