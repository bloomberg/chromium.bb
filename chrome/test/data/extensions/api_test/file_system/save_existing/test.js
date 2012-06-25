// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  function saveFile() {
    chrome.fileSystem.chooseFile({type: 'saveFile'}, chrome.test.callbackPass(
        function(entry) {
      chrome.test.assertEq('save_existing.txt', entry.name);
      // Test that the file can be readt.
      entry.file(chrome.test.callback(function(file) {
        var reader = new FileReader();
        reader.onloadend = chrome.test.callbackPass(function(e) {
          chrome.test.assertEq(reader.result.indexOf("Can you see me?"), 0);
          // Test that we can write to the file.
          entry.createWriter(function(fileWriter) {
            fileWriter.onwriteend = chrome.test.callback(function(e) {
              if (fileWriter.error) {
                chrome.test.fail("Error writing to file: " +
                                 fileWriter.error.toString());
              } else {
                // Get a new entry and check the data got to disk.
                chrome.fileSystem.chooseFile(chrome.test.callbackPass(
                    function(readEntry) {
                  readEntry.file(chrome.test.callback(function(readFile) {
                    var readReader = new FileReader();
                    readReader.onloadend = function(e) {
                      chrome.test.assertEq(readReader.result.indexOf("HoHoHo!"),
                                           0);
                      chrome.test.succeed();
                    };
                    readReader.onerror = function(e) {
                      chrome.test.fail('Failed to read file after write.');
                    };
                    readReader.readAsText(readFile);
                  }));
                }));
              }
            });
            var blob = new Blob(['HoHoHo!'], {type: 'text/plain'});
            fileWriter.write(blob);
          });
        });
        reader.onerror = chrome.test.callback(function(e) {
          chrome.test.fail("Error reading file contents.");
        });
        reader.readAsText(file);
      }));
    }));
  }
]);
