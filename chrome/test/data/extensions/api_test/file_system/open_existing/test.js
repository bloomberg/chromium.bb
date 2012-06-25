// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  function openFile() {
    chrome.fileSystem.chooseFile(chrome.test.callbackPass(function(entry) {
      chrome.test.assertEq('open_existing.txt', entry.name);
      // Test that the file can be read.
      entry.file(chrome.test.callback(function(file) {
        var reader = new FileReader();
        reader.onloadend = chrome.test.callback(function(e) {
          chrome.test.assertEq(reader.result.indexOf("Can you see me?"), 0);
          // Test that we cannot write to the file.
          entry.createWriter(chrome.test.callback(function(fileWriter) {
            fileWriter.onwriteend = chrome.test.callback(function(e) {
              if (fileWriter.error)
                chrome.test.succeed();
              else
                chrome.test.fail("No error while writing without write access");
            });
            var blob = new Blob(['HoHoHo!'], {type: 'text/plain'});
            fileWriter.write(blob);
          }));
        });
        reader.onerror = chrome.test.callback(function(e) {
          chrome.test.fail("Error reading file contents.");
        });
        reader.readAsText(file);
      }));
    }));
  }
]);
