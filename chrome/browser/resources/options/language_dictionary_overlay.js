// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  var OptionsPage = options.OptionsPage;
  var ArrayDataModel = cr.ui.ArrayDataModel;
  var DictionaryWordsList = options.dictionary_words.DictionaryWordsList;

  function EditDictionaryOverlay() {
    OptionsPage.call(this, 'editDictionary',
                     loadTimeData.getString('languageDictionaryOverlayPage'),
                     'language-dictionary-overlay-page');
  }

  cr.addSingletonGetter(EditDictionaryOverlay);

  EditDictionaryOverlay.prototype = {
    __proto__: OptionsPage.prototype,

    wordList_: null,

    initializePage: function() {
      OptionsPage.prototype.initializePage.call(this);
      this.wordList_ = $('language-dictionary-overlay-word-list');
      DictionaryWordsList.decorate(this.wordList_);
      this.wordList_.autoExpands = true;
      $('language-dictionary-overlay-done-button').onclick = function(e) {
        OptionsPage.closeOverlay();
      };
    },

    didShowPage: function() {
      chrome.send('refreshDictionaryWords');
    },

    setWordList_: function(entries) {
      entries.push('');
      this.wordList_.dataModel = new ArrayDataModel(entries);
    },
  };

  EditDictionaryOverlay.setWordList = function(entries) {
    EditDictionaryOverlay.getInstance().setWordList_(entries);
  };

  return {
    EditDictionaryOverlay: EditDictionaryOverlay
  };
});
