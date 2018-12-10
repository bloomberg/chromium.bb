// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  // Test getting the value for a key that is not set.
  function getUnsetKey() {
    chrome.inputMethodPrivate.getSetting('test', 'key', (val) => {
      chrome.test.assertEq(null, val);
      chrome.test.succeed();
    });
  },
  // Test setting and getting a string value.
  function getSetString() {
    chrome.inputMethodPrivate.setSetting('test', 'key-string', 'value1', () => {
      chrome.inputMethodPrivate.getSetting('test', 'key-string', (val) => {
        chrome.test.assertEq("value1", val);
        chrome.test.succeed();
      });
    });
  },
  // Test setting and getting an int value.
  function getSetInt() {
    chrome.inputMethodPrivate.setSetting('test', 'key-int', 42, () => {
      chrome.inputMethodPrivate.getSetting('test', 'key-int', (val) => {
        chrome.test.assertEq(42, val);
        chrome.test.succeed();
      });
    });
  },
  // Test setting and getting a bool value.
  function getSetBool() {
    chrome.inputMethodPrivate.setSetting('test', 'key-bool', true, () => {
      chrome.inputMethodPrivate.getSetting('test', 'key-bool', (val) => {
        chrome.test.assertEq(true, val);
        chrome.test.succeed();
      });
    });
  },
  // Test updating a key with a new value.
  function updateKey() {
    chrome.inputMethodPrivate.setSetting('test', 'key-update', 42, () => {
      chrome.inputMethodPrivate.setSetting('test', 'key-update', 'new', () => {
        chrome.inputMethodPrivate.getSetting('test', 'key-update', (val) => {
          chrome.test.assertEq('new', val);
          chrome.test.succeed();
        });
      });
    });
  },
  // Test setting and getting a multiple keys.
  function getSetMultiple() {
    chrome.inputMethodPrivate.setSetting('test', 'key1', 'value1', () => {
      chrome.inputMethodPrivate.setSetting('test', 'key2', 'value2', () => {
        chrome.inputMethodPrivate.getSetting('test', 'key1', (val) => {
          chrome.test.assertEq("value1", val);
          chrome.inputMethodPrivate.getSetting('test', 'key2', (val) => {
            chrome.test.assertEq("value2", val);
            chrome.test.succeed();
          });
        });
      });
    });
  },
  // Test setting and getting the same key from different IMEs.
  function getSetSameKeyDifferentIMEs() {
    chrome.inputMethodPrivate.setSetting('ime1', 'key', 'value1', () => {
      chrome.inputMethodPrivate.setSetting('ime2', 'key', 'value2', () => {
        chrome.inputMethodPrivate.getSetting('ime1', 'key', (val) => {
          chrome.test.assertEq("value1", val);
          chrome.inputMethodPrivate.getSetting('ime2', 'key', (val) => {
            chrome.test.assertEq("value2", val);
            chrome.test.succeed();
          });
        });
      });
    });
  },
  // Test OnSettingsChanged event gets raised when a new key is added.
  function eventRaisedWhenSettingToInitialValue() {
    const listener = (ime, key, value) => {
      chrome.test.assertEq('ime', ime);
      chrome.test.assertEq('key', key);
      chrome.test.assertEq('value', value);
      chrome.test.succeed();

      chrome.inputMethodPrivate.onSettingsChanged.removeListener(listener);
    };

    chrome.inputMethodPrivate.onSettingsChanged.addListener(listener);
    chrome.inputMethodPrivate.setSetting('ime', 'key', 'value');
  },
  // Test OnSettingsChanged event gets raised when a key is changed.
  function eventRaisedWhenSettingChanged() {
    const listener = (ime, key, value) => {
      chrome.test.assertEq('ime', ime);
      chrome.test.assertEq('key', key);
      chrome.test.assertEq('value2', value);
      chrome.test.succeed();

      chrome.inputMethodPrivate.onSettingsChanged.removeListener(listener);
    };

    chrome.inputMethodPrivate.setSetting('ime', 'key', 'value1', () => {
      chrome.inputMethodPrivate.onSettingsChanged.addListener(listener);
      chrome.inputMethodPrivate.setSetting('ime', 'key', 'value2');
    });
  }
]);
