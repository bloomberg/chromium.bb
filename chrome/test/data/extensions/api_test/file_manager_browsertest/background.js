// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// These must match the files created in the C++ code.
var EXPECTED_FILES_BEFORE = [
  ['hello.txt', '123 bytes', 'Plain text', 'Sep 4, 1998 12:34 PM'],
  ['world.mpeg', '1,000 bytes', 'MPEG video', 'Jul 4, 2012 10:35 AM'],
  ['My Desktop Background.png', '1 KB', 'PNG image', 'Jan 18, 2038 1:02 AM'],
  ['photos', '--', 'Folder', 'Jan 1, 1980 11:59 PM']
  // ['.warez', '--', 'Folder', 'Oct 26, 1985 1:39 PM']  # should be hidden
];
EXPECTED_FILES_BEFORE.sort();

var EXPECTED_FILES_AFTER = EXPECTED_FILES_BEFORE.slice();
EXPECTED_FILES_AFTER.push(
  ['newly added file.mp3', '2 KB', 'MP3 audio', 'Sep 4, 1998 12:00 AM']);
EXPECTED_FILES_AFTER.sort()

// Injects code into the file manager tab to get a list of the currently
// displayed files.
function getFileList(callback) {
  var GET_FILE_LIST_CODE =
      '(function() {\n' +
      '  var table = document.getElementById("detail-table");\n' +
      '  var rows = table.getElementsByTagName("li");\n' +
      '  var fileList = [];\n' +
      '  for (var j = 0; j < rows.length; ++j) {\n' +
      '    var row = rows[j];\n' +
      '    fileList.push([\n' +
      '      row.getElementsByClassName("filename-label")[0].textContent,\n' +
      '      row.getElementsByClassName("size")[0].textContent,\n' +
      '      row.getElementsByClassName("type")[0].textContent,\n' +
      '      row.getElementsByClassName("date")[0].textContent,\n' +
      '    ]);\n' +
      '  }\n' +
      '  return fileList;\n' +
      '})();';
  chrome.tabs.executeScript(null, {code: GET_FILE_LIST_CODE}, function(result) {
    var fileList = result[0];
    fileList.sort();
    callback(fileList);
  });
}

// Calls getFileList until the number of displayed files is different from
// lengthBefore.
function waitForFileListChange(lengthBefore, callback) {
  function helper() {
    getFileList(function(files) {
      if (files.length !== lengthBefore) {
        callback(files);
      } else {
        window.setTimeout(helper, 50);
      }
    });
  }
  helper();
}

// Waits until a dialog with an OK button is shown and accepts it.
function waitAndAcceptDialog(callback) {
  var CHECK_DLG_VISIBLE_CODE =
      '(function() {\n' +
      '  var button = document.querySelector(".cr-dialog-ok");\n' +
      '  if (button) {\n' +
      '    button.click();\n' +
      '    return true;\n' +
      '  }\n' +
      '  return false;\n' +
      '})();';
  function helper() {
    chrome.tabs.executeScript(null,
                              {code: CHECK_DLG_VISIBLE_CODE}, function(result) {
      if (result[0])
        callback();
      else
        window.setTimeout(helper, 50);
    });
  }
  helper();
}

// Checks that the files initially added by the C++ side are displayed, and
// that a subsequently added file shows up.
function testFileDisplay() {
  getFileList(function(actualFilesBefore) {
    chrome.test.assertEq(EXPECTED_FILES_BEFORE, actualFilesBefore);
    chrome.test.sendMessage('initial check done', function(reply) {
      chrome.test.assertEq('file added', reply);
      waitForFileListChange(
          EXPECTED_FILES_BEFORE.length,
          chrome.test.callbackPass(function(actualFilesAfter) {
            chrome.test.assertEq(EXPECTED_FILES_AFTER, actualFilesAfter);
          }));
    });
  });
}

// Injects the keyboard test code into the file manager tab and runs the
// keyboard delete test.
function doKeyboardTestWithConfirmation(code) {
  chrome.tabs.executeScript(null, {file: 'fake_keypress.js'}, function(result) {
    chrome.tabs.executeScript(null, {code: code}, function(result) {
      chrome.test.assertFalse(!result[0]);
      waitAndAcceptDialog(function() {
        // Succeed here if anything happens; the C++ code checks what happened.
        waitForFileListChange(EXPECTED_FILES_BEFORE.length,
                              chrome.test.succeed);
      });
    });
  });
}

// Injects the keyboard test code into the file manager tab and runs the
// keyboard copy test.
function doKeyboardTest(code) {
  chrome.tabs.executeScript(null, {file: 'fake_keypress.js'}, function(result) {
    chrome.tabs.executeScript(null, {code: code}, function(result) {
      chrome.test.assertFalse(!result[0]);
      // Succeed here if anything happens; the C++ code checks what happened.
      waitForFileListChange(EXPECTED_FILES_BEFORE.length, chrome.test.succeed);
    });
  });
}

chrome.test.runTests([
  // Waits for the C++ code to send a string identifying a test, then runs that
  // test.
  function testRunner() {
    chrome.test.sendMessage('which test', function(reply) {
      // These test names are provided by file_manager_browsertest.cc.
      if (reply === 'file display') {
        testFileDisplay();
      } else if (reply === 'keyboard copy') {
        doKeyboardTest('fakeFileCopy("world.mpeg");');
      } else if (reply === 'keyboard delete') {
        doKeyboardTestWithConfirmation('fakeFileDelete("world.mpeg");');
      } else {
        chrome.test.fail('Bogus test name passed to testRunner()');
      }
    });
  }
]);
