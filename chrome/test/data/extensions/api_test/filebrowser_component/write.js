// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Checks that filesystem_handler read the file correctly (original and received
// text match) and changed its content in expected way (file should contain
// originalText + originalText).
function verifyFileContent(file, originalText, request, callback) {
  if (request.fileContent != originalText) {
    callback({message:'Received content does not match. ' +
                      'Expected: "' + originalText + '", ' +
                      'Got "' + request.fileContent + '".'});
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

function getFileExtensionFromLocationHref() {
  var loc = window.location.href;
  console.log("Opening tab " + loc);
  if (loc.indexOf("#") == -1 ) {
    console.log("No params in url, faling back to default.");
    return undefined;
  }

  loc = unescape(loc.substr(loc.indexOf("#") + 1));
  return loc;
}

function createTestExpectations(defaultFileExtension) {
  var fileExtension = getFileExtensionFromLocationHref();
  if (!fileExtension)
    fileExtension = defaultFileExtension;

  var fileActionName = 'TestAction_' + fileExtension;
  var fileActionFilter = ['filesystem:*.' + fileExtension.toLowerCase()];
  var expectedTasks = {};
  expectedTasks[fileActionName] = fileActionFilter;
  fileExtension = '.' + fileExtension;

  return new TestExpectations(fileExtension, expectedTasks, verifyFileContent);
}

chrome.test.runTests([function tab() {
  var expectations = createTestExpectations('aBc');

  var testRunner = new TestRunner(expectations);
  testRunner.runTest();
}]);
