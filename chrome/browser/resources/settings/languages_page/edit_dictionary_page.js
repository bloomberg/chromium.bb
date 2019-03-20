// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-edit-dictionary-page' is a sub-page for editing
 * the "dictionary" of custom words used for spell check.
 */

// Max valid word size defined in
// https://cs.chromium.org/chromium/src/components/spellcheck/common/spellcheck_common.h?l=28
const MAX_CUSTOM_DICTIONARY_WORD_BYTES = 99;

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
      value: settings.routes.EDIT_DICTIONARY,
    },

    /** @private {!Array<string>} */
    words_: {
      type: Array,
      value: function() {
        return [];
      },
    },

    /** @private {boolean} */
    hasWords_: {
      type: Boolean,
      value: false,
    },
  },

  /** @type {LanguageSettingsPrivate} */
  languageSettingsPrivate: null,

  /** @override */
  ready: function() {
    this.languageSettingsPrivate = settings.languageSettingsPrivateApiForTest ||
        /** @type {!LanguageSettingsPrivate} */
        (chrome.languageSettingsPrivate);

    this.languageSettingsPrivate.getSpellcheckWords(words => {
      this.hasWords_ = words.length > 0;
      this.words_ = words;
    });

    this.languageSettingsPrivate.onCustomDictionaryChanged.addListener(
        this.onCustomDictionaryChanged_.bind(this));

    // Add a key handler for the new-word input.
    this.$.keys.target = this.$.newWord;
  },

  /**
   * Check if the field is empty or invalid.
   * @param {string} word
   * @return {boolean}
   * @private
   */
  disableAddButton_: function(word) {
    return word.trim().length == 0 || this.isWordInvalid_(word);
  },

  /**
   * If the word is invalid, returns true (or a message if one is provided).
   * Otherwise returns false.
   * @param {string} word
   * @param {string} duplicateError
   * @param {string} lengthError
   * @return {string|boolean}
   * @private
   */
  isWordInvalid_: function(word, duplicateError, lengthError) {
    const trimmedWord = word.trim();
    if (this.words_.indexOf(trimmedWord) != -1) {
      return duplicateError || true;
    } else if (trimmedWord.length > MAX_CUSTOM_DICTIONARY_WORD_BYTES) {
      return lengthError || true;
    }

    return false;
  },

  /**
   * Handles updates to the word list. Additions are unshifted to the top
   * of the list so that users can see them easily.
   * @param {!Array<string>} added
   * @param {!Array<string>} removed
   */
  onCustomDictionaryChanged_: function(added, removed) {
    const wasEmpty = this.words_.length == 0;

    for (const word of removed) {
      this.arrayDelete('words_', word);
    }

    if (this.words_.length === 0 && added.length === 0 && !wasEmpty) {
      this.hasWords_ = false;
    }

    // This is a workaround to ensure the dom-if is set to true before items
    // are rendered so that focus works correctly in Polymer 2; see
    // https://crbug.com/912523.
    if (wasEmpty && added.length > 0) {
      this.hasWords_ = true;
    }

    for (const word of added) {
      if (this.words_.indexOf(word) == -1) {
        this.unshift('words_', word);
      }
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

    // Update input enable to reflect new additions/removals.
    // TODO(hsuregan): Remove hack when notifyPath() or notifySplices()
    // is successful at creating DOM changes when applied to words_ (when
    // attached to input newWord), OR when array changes are registered.
    this.$.addWord.disabled = !this.$.newWord.validate();
  },

  /**
   * Handles Enter and Escape key presses for the new-word input.
   * @param {!CustomEvent<!{key: string}>} e
   */
  onKeysPress_: function(e) {
    if (e.detail.key == 'enter' &&
        !this.disableAddButton_(this.newWordValue_)) {
      this.addWordFromInput_();
    } else if (e.detail.key == 'esc') {
      e.detail.keyboardEvent.target.value = '';
    }
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
   * Adds the word in the new-word input to the dictionary.
   */
  addWordFromInput_: function() {
    // Spaces are allowed, but removing leading and trailing whitespace.
    const word = this.newWordValue_.trim();
    this.newWordValue_ = '';
    if (!word) {
      return;
    }

    this.languageSettingsPrivate.addSpellcheckWord(word);
  },
});
