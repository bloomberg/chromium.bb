// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Testing with directory access, but no write permission.
chrome.test.runTests([
  function openFile() {
    chrome.fileSystem.chooseEntry(
        {type: 'openDirectory'},
        chrome.test.callbackPass(function(directoryEntry) {
      directoryEntry.getFile(
          'open_existing.txt', {},
          chrome.test.callback(function(entry) {
        checkEntry(entry, 'open_existing.txt', false, false);
      }));
    }));
  },
  function readDirectory() {
    chrome.fileSystem.chooseEntry(
        {type: 'openDirectory'},
        chrome.test.callbackPass(function(directoryEntry) {
      var reader = directoryEntry.createReader();
      reader.readEntries(chrome.test.callback(function(entries) {
        chrome.test.assertEq(entries.length, 1);
        var entry = entries[0];
        checkEntry(entry, 'open_existing.txt', false, false);
      }));
    }));
  },
  function removeFile() {
    chrome.fileSystem.chooseEntry(
        {type: 'openDirectory'},
        chrome.test.callbackPass(function(directoryEntry) {
      directoryEntry.getFile(
          'open_existing.txt', {}, chrome.test.callback(function(entry) {
        entry.remove(chrome.test.callback(function() {
          chrome.test.fail('Could delete a file without permission');
        }), chrome.test.callback(function() {
          chrome.test.succeed();
        }));
      }));
    }));
  },
  function copyFile() {
    chrome.fileSystem.chooseEntry(
        {type: 'openDirectory'},
        chrome.test.callbackPass(function(directoryEntry) {
      directoryEntry.getFile(
          'open_existing.txt', {}, chrome.test.callback(function(entry) {
        entry.copyTo(
            directoryEntry, 'copy.txt', chrome.test.callback(function() {
          chrome.test.fail('Could copy a file without permission.');
        }), chrome.test.callback(function() {
          chrome.test.succeed();
        }));
      }));
    }));
  },
  function moveFile() {
    chrome.fileSystem.chooseEntry(
        {type: 'openDirectory'},
        chrome.test.callbackPass(function(directoryEntry) {
      directoryEntry.getFile(
          'open_existing.txt', {}, chrome.test.callback(function(entry) {
        entry.moveTo(
            directoryEntry, 'moved.txt', chrome.test.callback(function() {
          chrome.test.fail('Could move a file without permission.');
        }), chrome.test.callback(function() {
          chrome.test.succeed();
        }));
      }));
    }));
  },
  function createFile() {
    chrome.fileSystem.chooseEntry(
        {type: 'openDirectory'},
        chrome.test.callbackPass(function(directoryEntry) {
      directoryEntry.getFile(
          'new.txt', {create: true}, chrome.test.callback(function() {
        chrome.test.fail('Could create a file without permission.');
      }), chrome.test.callback(function() {
        chrome.test.succeed();
      }));
    }));
  },
  function createDirectory() {
    chrome.fileSystem.chooseEntry(
        {type: 'openDirectory'},
        chrome.test.callbackPass(function(directoryEntry) {
      directoryEntry.getDirectory(
          'new', {create: true}, chrome.test.callback(function() {
        chrome.test.fail('Could create a directory without permission.');
      }), chrome.test.callback(function() {
        chrome.test.succeed();
      }));
    }));
  }
]);
