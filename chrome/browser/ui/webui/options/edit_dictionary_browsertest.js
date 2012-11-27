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
  $('language-dictionary-overlay-word-list').querySelector('input').value =
      testWord;

  this.mockHandler.expects(once()).addDictionaryWord([testWord]).
      will(callFunction(function() {
          EditDictionaryOverlay.setWordList([testWord]);
      }));
  var addWordItem = EditDictionaryOverlay.getWordListForTesting().items[0];
  addWordItem.onEditCommitted_({currentTarget: addWordItem});

  this.mockHandler.expects(once()).removeDictionaryWord([testWord]).
      will(callFunction(function() {
          EditDictionaryOverlay.setWordList([]);
      }));
  EditDictionaryOverlay.getWordListForTesting().deleteItemAtIndex(0);
});

TEST_F('EditDictionaryWebUITest', 'testSearch', function() {
  EditDictionaryOverlay.setWordList(['foo', 'bar']);
  expectEquals(3, EditDictionaryOverlay.getWordListForTesting().items.length);

  /**
   * @param {Element} el The element to dispatch an event on.
   * @param {string} value The text of the search event.
   */
  var fakeSearchEvent = function(el, value) {
    el.value = value;
    cr.dispatchSimpleEvent(el, 'search');
  };
  var searchField = $('language-dictionary-overlay-search-field');
  fakeSearchEvent(searchField, 'foo');
  expectEquals(2, EditDictionaryOverlay.getWordListForTesting().items.length);

  fakeSearchEvent(searchField, '');
  expectEquals(3, EditDictionaryOverlay.getWordListForTesting().items.length);
});
