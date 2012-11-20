// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * TestFixture for EditDictionaryOverlay WebUI testing.
 * @extends {testing.Test}
 * @constructor
 **/
function EditDictionaryWebUITest() {}

EditDictionaryWebUITest.prototype = {
  __proto__: testing.Test.prototype,

  /**
   * Browse to the edit dictionary page & call our preLoad().
   */
  browsePreload: 'chrome://settings-frame/editDictionary',

  /**
   * Register a mock dictionary handler.
   */
  preLoad: function() {
    this.makeAndRegisterMockHandler(
        ['refreshDictionaryWords',
         'addDictionaryWord',
         'removeDictionaryWord',
        ]);
    this.mockHandler.stubs().refreshDictionaryWords().
        will(callFunction(function() {
          EditDictionaryOverlay.setWordList([]);
        }));
    this.mockHandler.stubs().addDictionaryWord(ANYTHING);
    this.mockHandler.stubs().removeDictionaryWord(ANYTHING);
  },
};

TEST_F('EditDictionaryWebUITest', 'testAddRemoveWords', function() {
  var testWord = 'foo';
  $('language-dictionary-overlay-page').querySelector('input').value = testWord;

  this.mockHandler.expects(once()).addDictionaryWord([testWord]).
      will(callFunction(function() {
          EditDictionaryOverlay.setWordList([testWord]);
      }));
  EditDictionaryOverlay.getInstance().wordList_.items[0].onEditCommitted_(null);

  this.mockHandler.expects(once()).removeDictionaryWord([String(0)]).
      will(callFunction(function() {
          EditDictionaryOverlay.setWordList([]);
      }));
  EditDictionaryOverlay.getInstance().wordList_.deleteItemAtIndex(0);
});
