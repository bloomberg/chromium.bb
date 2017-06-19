// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-edit-dictionary-page' is a sub-page for editing
 * the "dictionary" of custom words used for spell check.
 */
Polymer({
  is: 'settings-edit-dictionary-page',

  behaviors: [settings.GlobalScrollTargetBehavior],

  properties: {
    /** @private {string} */
    newWordValue_: String,

    /**
     * Needed by GlobalScrollTargetBehavior.
     * @override
     */
    subpageRoute: {
      type: Object,
      value: settings.Route.EDIT_DICTIONARY,
    },

    /** @private {!Array<string>} */
    words_: {
      type: Array,
      value: function() {
        return [];
      },
    },
  },

  /** @type {LanguageSettingsPrivate} */
  languageSettingsPrivate: null,

  /** @override */
  ready: function() {
    this.languageSettingsPrivate = settings.languageSettingsPrivateApiForTest ||
        /** @type {!LanguageSettingsPrivate} */
        (chrome.languageSettingsPrivate);

    this.languageSettingsPrivate.getSpellcheckWords(function(words) {
      this.words_ = words;
    }.bind(this));

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
   * Handles updates to the word list. Additions are unshifted to the top
   * of the list so that users can see them easily.
   * @param {!Array<string>} added
   * @param {!Array<string>} removed
   */
  onCustomDictionaryChanged_: function(added, removed) {
    var wasEmpty = this.words_.length == 0;

    for (var i = 0; i < removed.length; i++)
      this.arrayDelete('words_', removed[i]);

    for (var i = 0; i < added.length; i++) {
      if (this.words_.indexOf(added[i]) == -1)
        this.unshift('words_', added[i]);
    }

    // When adding a word to an _empty_ list, the template is expanded. This
    // is a workaround to resize the iron-list as well.
    // TODO(dschuyler): Remove this hack after iron-list no longer needs
    // this workaround to update the list at the same time the template
    // wrapping the list is expanded.
    if (wasEmpty && this.words_.length > 0) {
      Polymer.dom.flush();
      this.$$('#list').notifyResize();
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
   * Handles tapping on a "Remove word" icon button.
   * @param {!{model: !{item: string}}} e
   */
  onRemoveWordTap_: function(e) {
    this.languageSettingsPrivate.removeSpellcheckWord(e.model.item);
  },

  /**
   * Adds the word in the paper-input to the dictionary.
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
    }
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
