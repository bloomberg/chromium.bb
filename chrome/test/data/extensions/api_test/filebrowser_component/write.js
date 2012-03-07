// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function readFile(entry, successCallback, errorCallback) {
  var reader = new FileReader();
  reader.onloadend = function(e) {
    successCallback(reader.result);
  };
  reader.onerror = function(e) {
    errorCallback(reader.error);
  };
  entry.file(function(file) {
    reader.readAsText(file);
  });
};

// Checks that filesystem_handler read the file correctly (original and received
// text match) and changed its content in expected way (file should contain
// originalText + originalText).
function verifyFileContent(file, originalText, receivedText, callback) {
  if (receivedText != originalText) {
    callback({message:'Received content does not match. ' +
                      'Expected: "' + originalText + '", ' +
                      'Got "' + receivedText + '".'});
    return;
  }

  readFile(file,
     function(text) {
       var error = undefined;
       var expectedText = originalText + originalText;
       if (text != expectedText) {
         error = {message: 'File content not as expected. ' +
                           'Expected: "' + expectedText + '", ' +
                           'Read: ' + text + '".'};
       }
       callback(error);
     },
     callback);
};

chrome.test.runTests([function tab() {
  var expectedTasks = {'AbcAction': ['filesystem:*.abc'],
                       'BaseAction': ['filesystem:*', 'filesystem:*.*']};
  var expectations = new TestExpectations(expectedTasks, verifyFileContent);

  var testRunner = new TestRunner(expectations);
  testRunner.runTest();
}]);
