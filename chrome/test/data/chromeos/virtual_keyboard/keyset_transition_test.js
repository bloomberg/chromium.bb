/*
 * Copyright 2013 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * Returns the unique transition key identified by the innerText,
 * alignment and keyset it is in.
 * @param {string} alignment The alignment of the key. One of {left,right}.
 * @param {string} initialKeyset The keyset the key is in.
 * @param {string} text The inner text of the key we want.
 * @return {Object} The unique key which satisfies these properties.
 */
function getTransitionKey(alignment, initialKeyset, text) {
  var keyset = keyboard.querySelector('#qwerty-' + initialKeyset);
  assertTrue(!!keyset, "Keyset " + initialKeyset + " not found.");
  var candidates = keyset.querySelectorAll('[align="' +
      alignment +'"]').array();
  assertTrue(candidates.length > 0, "No " +
      alignment + " aligned keys found.");
  var results = candidates.filter(function(key) {
        return key.innerText == text;
      });
  assertEquals(1, results.length, 'Unexpected number of ' +
      text + ' keys with alignment: ' + alignment);
  return results[0];
}

// helper functions for getting different keys.
function getSymbolKey(alignment, initialKeyset) {
  return getTransitionKey(alignment, initialKeyset, '#123');
}

function getAbcKey(alignment, initialKeyset) {
  return getTransitionKey(alignment, initialKeyset, 'abc');
}

function getMoreKey(alignment) {
  return getTransitionKey(alignment, 'symbol', 'more');
}

function getShiftKey(alignment, keyset) {
  return getTransitionKey(alignment, keyset, 'shift');
}

/**
 * Tests all the basic transitions between keysets using keys
 * of a particular alignment.
 * @param {string} alignment The alignment of the keys to test
 */
function checkBasicTransitions(alignment) {
  var mockEvent = {pointerId:1, isPrimary:true};
  assertEquals('lower', keyboard.keyset,
      "Unexpected initial keyset.");

  // Test the path abc -> symbol -> more -> symbol -> abc.
  var symbol = getSymbolKey(alignment, 'lower');
  symbol.down(mockEvent);
  symbol.up(mockEvent);
  assertEquals('symbol', keyboard.keyset,
      "Did not transition from lower to symbol keyset. ");

  var more = getMoreKey(alignment);
  more.down(mockEvent);
  more.up(mockEvent);
  assertEquals('more', keyboard.keyset,
      "Did not transition from symbol to more keyset.");

  symbol = getSymbolKey(alignment, 'more');
  symbol.down(mockEvent);
  symbol.up(mockEvent);
  assertEquals('symbol', keyboard.keyset,
      "Did not transition from  more to symbol. ");

  var abc = getAbcKey(alignment, 'symbol');
  abc.down(mockEvent);
  abc.up(mockEvent);
  assertEquals('lower', keyboard.keyset,
      "Did not transition from symbol to lower keyset. ");

  // Test the path abc -> symbol -> more -> abc.
  symbol = getSymbolKey(alignment, 'lower');
  symbol.down(mockEvent);
  symbol.up(mockEvent);
  assertEquals('symbol', keyboard.keyset,
      "Did not transition from lower to symbol keyset in second path. ");

  more = getMoreKey(alignment);
  more.down(mockEvent);
  more.up(mockEvent);
  assertEquals('more', keyboard.keyset,
      "Did not transition from symbol to more keyset in second path.");
  abc = getAbcKey(alignment, 'more');
  abc.down(mockEvent);
  abc.up(mockEvent);
  assertEquals('lower', keyboard.keyset,
      "Did not transition from more to lower keyset. ");
}

/**
 * Tests that capitalization persists among keyset transitions
 * when using keys of a particular alignment.
 * @param {string} alignment The alignment of the key to transition with.
 */
function checkPersistantCapitalization(alignment) {
  var mockEvent = {pointerId:1, isPrimary:true};
  assertEquals('lower', keyboard.keyset,
      "Unexpected initial keyset.");

  var lowerShift = getShiftKey(alignment, 'lower');
  lowerShift.down(mockEvent);
  // Long press to capslock.
  mockTimer.tick(1000);
  assertEquals('upper', keyboard.keyset,
      "Did not transition to locked keyset on long press");

  var symbol = getSymbolKey(alignment, 'upper');
  symbol.down(mockEvent);
  symbol.up(mockEvent);
  assertEquals('symbol', keyboard.keyset,
      "Did not transition from upper to symbol keyset. ");
  var more = getMoreKey(alignment);
  more.down(mockEvent);
  more.up(mockEvent);
  assertEquals('more', keyboard.keyset,
      "Did not transition from symbol to more keyset.");
  var abc = getAbcKey(alignment, 'more');
  abc.down(mockEvent);
  abc.up(mockEvent);
  assertEquals('upper', keyboard.keyset,
      "Did not persist capitalization on keyset transition. ");

  // Reset to lower
  var upperShift = getShiftKey(alignment, 'upper');
  upperShift.up(mockEvent);
  lowerShift.down(mockEvent);
  assertEquals('lower', keyboard.keyset,
      "Unexpected final keyset.");
}

/**
 * Tests that capitalizion persists on keyset transitions.
 * The test is run asynchronously since the keyboard loads keysets dynamically.
 * @param {function} testDoneCallback The function to be called on completion.
 */
function testPersistantCapitalizationAsync(testDoneCallback) {
  var runTest = function() {
  var alignments = ['left', 'right'];
    for (var i in alignments) {
      checkPersistantCapitalization(alignments[i]);
    }
  };
  onKeyboardReady('testPersistantCapitalizationAsync',
      runTest, testDoneCallback);
}

/**
 * Tests that changing the input type changes the layout. The test is run
 * asynchronously since the keyboard loads keysets dynamically.
 * @param {function} testDoneCallback The function to be called on completion.
 */
function testInputTypeResponsivenessAsync(testDoneCallback) {
  var testName = "testInputTypeResponsivenessAsync";
  var transition = function (expectedKeyset, nextInputType, error) {
    return function() {
      assertEquals(expectedKeyset, keyboard.activeKeysetId, error);
      keyboard.inputTypeValue = nextInputType;
    }
  }

  var setupWork = function() {
    // Check initial state.
    assertEquals('qwerty-lower', keyboard.activeKeysetId,
        "Unexpected initial active keyset");
    // Check that capitalization is not persistant
    var lowerShift = getShiftKey('left', 'lower');
    var upperShift = getShiftKey('left', 'upper');
    var mockEvent = {isPrimary: true, pointerId: 1};
    lowerShift.down(mockEvent);
    mockTimer.tick(1000);
    upperShift.up(mockEvent);
    assertEquals('qwerty-upper', keyboard.activeKeysetId,
        "Unexpected transition on long press.");
    keyboard.inputTypeValue = 'text';
  }

  var subtasks = [];
  subtasks.push(transition('qwerty-lower', 'number',
      "Did not reset keyboard on focus change"));
  subtasks.push(transition('numeric-symbol', 'text',
      "Did not switch to numeric keypad."));
  // Clean up
  subtasks.push(function() {
   assertEquals('qwerty-lower', keyboard.activeKeysetId,
        "Unexpected final active keyset");
  });
  onKeyboardReady(testName, setupWork, testDoneCallback, subtasks);
}

 /**
 * Tests that keyset transitions work as expected.
 * The test is run asynchronously since the keyboard loads keysets dynamically.
 * @param {function} testDoneCallback The function to be called on completion.
 */
function testKeysetTransitionsAsync(testDoneCallback) {
  var runTest = function() {
    var alignments = ['left', 'right'];
    for (var i in alignments) {
      checkBasicTransitions(alignments[i]);
    }
  };
  onKeyboardReady('testKeysetTransitionsAsync',
     runTest, testDoneCallback);
}
