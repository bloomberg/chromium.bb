// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Verifies that the filesystem_handler extension returned the content of the
// file.
function verifyFileContent(file, originalText, receivedText, callback) {
  var error = undefined;
  if (receivedText != originalText)
    error = {message: 'Received content does not match. ' +
                      'Expected "' + originalText + '", ' +
                      'Got "' + receivedText + '".'};
  callback(error);
};

chrome.test.runTests([function tab() {
  var expectedTasks = {'AbcAction': ['filesystem:*.abc'],
                       'BaseAction': ['filesystem:*', 'filesystem:*.*']};
  var expectations = new TestExpectations(expectedTasks, verifyFileContent);

  var testRunner = new TestRunner(expectations);
  testRunner.runTest();
}]);
