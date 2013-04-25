// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Namespace for test related things.
 */
var test = test || {};

/**
 * Namespace for test utility functions.
 */
test.util = {};

/**
 * Extension ID of the testing extension.
 * @type {string}
 * @const
 */
test.util.TESTING_EXTENSION_ID = 'oobinhbdbiehknkpbpejbbpdbkdjmoco';

/**
 * Opens the main Files.app's window and waits until it is ready.
 *
 * @param {string} path Path of the directory to be opened.
 * @param {function(string)} callback Completion callback with the new window's
 *     App ID.
 */
test.util.openMainWindow = function(path, callback) {
  var appId = launchFileManager({defaultPath: path});
  function helper() {
    if (appWindows[appId]) {
      var contentWindow = appWindows[appId].contentWindow;
      var table = contentWindow.document.querySelector('#detail-table');
      if (table) {
        callback(appId);
        return;
      }
    }
    window.setTimeout(helper, 50);
  }
  helper();
};

/**
 * Gets total Javascript error count from each app window.
 * @return {number} Error count.
 */
test.util.getErrorCount = function() {
  var totalCount = 0;
  for (var appId in appWindows) {
    var contentWindow = appWindows[appId].contentWindow;
    if (contentWindow.JSErrorCount)
      totalCount += contentWindow.JSErrorCount;
  }
  return totalCount;
};

/**
 * Returns an array with the files currently selected in the file manager.
 *
 * @param {Window} contentWindow Window to be tested.
 * @return {Array.<string>} Array of selected files.
 */
test.util.getSelectedFiles = function(contentWindow) {
  var table = contentWindow.document.querySelector('#detail-table');
  var rows = table.querySelectorAll('li');
  var selected = [];
  for (var i = 0; i < rows.length; ++i) {
    if (rows[i].hasAttribute('selected')) {
      selected.push(
          rows[i].querySelector('.filename-label').textContent);
    }
  }
  return selected;
};

/**
 * Returns an array with the files on the file manager's file list.
 *
 * @param {Window} contentWindow Window to be tested.
 * @return {Array.<Array.<string>>} Array of rows.
 */
test.util.getFileList = function(contentWindow) {
  var table = contentWindow.document.querySelector('#detail-table');
  var rows = table.querySelectorAll('li');
  var fileList = [];
  for (var j = 0; j < rows.length; ++j) {
    var row = rows[j];
    fileList.push([
      row.querySelector('.filename-label').textContent,
      row.querySelector('.size').textContent,
      row.querySelector('.type').textContent,
      row.querySelector('.date').textContent
    ]);
  }
  fileList.sort();
  return fileList;
};

/**
 * Calls getFileList until the number of displayed files is different from
 * lengthBefore.
 *
 * @param {Window} contentWindow Window to be tested.
 * @param {number} lengthBefore Number of items visible before.
 * @param {function(Array.<Array.<string>>)} callback Change callback.
 */
test.util.waitForFileListChange = function(
    contentWindow, lengthBefore, callback) {
  function helper() {
    var files = test.util.getFileList(contentWindow);
    var notReadyRows = files.filter(function(row) {
      return row.filter(function(cell) { return cell == '...'; }).length;
    });
    if (notReadyRows.length === 0 &&
        files.length !== lengthBefore &&
        files.length !== 0)
      callback(files);
    else
      window.setTimeout(helper, 50);
  }
  helper();
};

/**
 * Returns an array of items on the file manager's autocomplete list.
 *
 * @param {Window} contentWindow Window to be tested.
 * @return {Array.<string>} Array of items.
 */
test.util.getAutocompleteList = function(contentWindow) {
  var list = contentWindow.document.querySelector('#autocomplete-list');
  var lines = list.querySelectorAll('li');
  var items = [];
  for (var j = 0; j < lines.length; ++j) {
    var line = lines[j];
    items.push(line.innerText);
  }
  return items;
};

/**
 * Performs autocomplete with the given query and waits until at least
 * |numExpectedItems| items are shown, including the first item which
 * always looks like "'<query>' - search Drive".
 *
 * @param {Window} contentWindow Window to be tested.
 * @param {string} query Query used for autocomplete.
 * @param {number} numExpectedItems number of items to be shown.
 * @param {function(Array.<string>)} callback Change callback.
 */
test.util.performAutocompleteAndWait = function(
    contentWindow, query, numExpectedItems, callback) {
  // Dispatch a 'focus' event to the search box so that the autocomplete list
  // is attached to the search box. Note that calling searchBox.focus() won't
  // dispatch a 'focus' event.
  var searchBox = contentWindow.document.querySelector('#search-box');
  var focusEvent = contentWindow.document.createEvent('Event');
  focusEvent.initEvent('focus', true /* bubbles */, true /* cancelable */);
  searchBox.dispatchEvent(focusEvent);

  // Change the value of the search box and dispatch an 'input' event so that
  // the autocomplete query is processed.
  searchBox.value = query;
  var inputEvent = contentWindow.document.createEvent('Event');
  inputEvent.initEvent('input', true /* bubbles */, true /* cancelable */);
  searchBox.dispatchEvent(inputEvent);

  function helper() {
    var items = test.util.getAutocompleteList(contentWindow);
    if (items.length >= numExpectedItems)
      callback(items);
    else
      window.setTimeout(helper, 50);
  }
  helper();
};

/**
 * Waits until a dialog with an OK button is shown and accepts it.
 *
 * @param {Window} contentWindow Window to be tested.
 * @param {function()} callback Success callback.
 */
test.util.waitAndAcceptDialog = function(contentWindow, callback) {
  var tryAccept = function() {
    var button = contentWindow.document.querySelector('.cr-dialog-ok');
    if (button) {
      button.click();
      callback();
      return;
    }
    window.setTimeout(tryAccept, 50);
  };
  tryAccept();
};

/**
 * Fakes pressing the down arrow until the given |filename| is selected.
 *
 * @param {Window} contentWindow Window to be tested.
 * @param {string} filename Name of the file to be selected.
 * @return {boolean} True if file got selected, false otherwise.
 */
test.util.selectFile = function(contentWindow, filename) {
  var table = contentWindow.document.querySelector('#detail-table');
  var rows = table.querySelectorAll('li');
  for (var index = 0; index < rows.length; ++index) {
    test.util.fakeKeyDown(contentWindow, '#file-list', 'Down', false);
    var selection = test.util.getSelectedFiles(contentWindow);
    if (selection.length === 1 && selection[0] === filename)
      return true;
  }
  console.error('Failed to select file "' + filename + '"');
  return false;
};

/**
 * Selects a volume specified by its icon name
 *
 * @param {Window} contentWindow Window to be tested.
 * @param {string} iconName Name of the volume icon.
 * @return {boolean} True if the target is found and mousedown and click events
 *     are sent.
 */
test.util.selectVolume = function(contentWindow, iconName) {
  var query = '[volume-type-icon=' + iconName + ']';
  // To change the selected volume, we have to send both events 'mousedown' and
  // 'click' to the volume list.
  return test.util.fakeMouseDown(contentWindow, query) &&
      test.util.fakeMouseClick(contentWindow, query);
};

/**
 * Sends an event to the element specified by |targetQuery|.
 *
 * @param {Window} contentWindow Window to be tested.
 * @param {string} targetQuery Query to specify the element.
 * @param {Event} event Event to be sent.
 * @return {boolean} True if the event is sent to the target, false otherwise.
 */
test.util.sendEvent = function(contentWindow, targetQuery, event) {
  var target = contentWindow.document.querySelector(targetQuery);
  if (target) {
    target.dispatchEvent(event);
    return true;
  } else {
    console.error('Target element for ' + targetQuery + ' not found.');
    return false;
  }
};

/**
 * Sends a fake key event to the element specified by |targetQuery| with the
 * given |keyIdentifier| and optional |ctrl| modifier to the file manager.
 *
 * @param {Window} contentWindow Window to be tested.
 * @param {string} targetQuery Query to specify the element.
 * @param {string} keyIdentifier Identifier of the emulated key.
 * @param {boolean} ctrl Whether CTRL should be pressed, or not.
 * @return {boolean} True if the event is sent to the target, false otherwise.
 */
test.util.fakeKeyDown = function(
    contentWindow, targetQuery, keyIdentifier, ctrl) {
  var event = new KeyboardEvent(
      'keydown',
      { bubbles: true, keyIdentifier: keyIdentifier, ctrlKey: ctrl });
  return test.util.sendEvent(contentWindow, targetQuery, event);
};

/**
 * Sends a fake mouse click event to the element specified by |targetQuery|.
 *
 * @param {Window} contentWindow Window to be tested.
 * @param {string} targetQuery Query to specify the element.
 * @return {boolean} True if the event is sent to the target, false otherwise.
 */
test.util.fakeMouseClick = function(contentWindow, targetQuery) {
  var event = new MouseEvent('click', { bubbles: true });
  return test.util.sendEvent(contentWindow, targetQuery, event);
};

/**
 * Sends a fake mouse down event to the element specified by |targetQuery|.
 *
 * @param {Window} contentWindow Window to be tested.
 * @param {string} targetQuery Query to specify the element.
 * @return {boolean} True if the event is sent to the target, false otherwise.
 */
test.util.fakeMouseDown = function(contentWindow, targetQuery) {
  var event = new MouseEvent('mousedown', { bubbles: true });
  return test.util.sendEvent(contentWindow, targetQuery, event);
};

/**
 * Selects |filename| and fakes pressing Ctrl+C, Ctrl+V (copy, paste).
 *
 * @param {Window} contentWindow Window to be tested.
 * @param {string} filename Name of the file to be copied.
 * @return {boolean} True if copying got simulated successfully. It does not
 *     say if the file got copied, or not.
 */
test.util.copyFile = function(contentWindow, filename) {
  if (!test.util.selectFile(contentWindow, filename))
    return false;
  // Ctrl+C and Ctrl+V
  test.util.fakeKeyDown(contentWindow, '#file-list', 'U+0043', true);
  test.util.fakeKeyDown(contentWindow, '#file-list', 'U+0056', true);
  return true;
};

/**
 * Selects |filename| and fakes pressing the Delete key.
 *
 * @param {Window} contentWindow Window to be tested.
 * @param {string} filename Name of the file to be deleted.
 * @return {boolean} True if deleting got simulated successfully. It does not
 *     say if the file got deleted, or not.
 */
test.util.deleteFile = function(contentWindow, filename) {
  if (!test.util.selectFile(contentWindow, filename))
    return false;
  // Delete
  test.util.fakeKeyDown(contentWindow, '#file-list', 'U+007F', false);
  return true;
};

/**
 * Registers message listener, which runs test utility functions.
 */
test.util.registerRemoteTestUtils = function() {
  var onMessage = chrome.runtime ? chrome.runtime.onMessageExternal :
      chrome.extension.onMessageExternal;
  onMessage.addListener(function(request, sender, sendResponse) {
    if (sender.id != test.util.TESTING_EXTENSION_ID) {
      console.error('The testing extension must be white-listed.');
      return false;
    }
    var contentWindow;
    if (request.appId) {
      if (!appWindows[request.appId]) {
        console.error('Specified window not found.');
        return false;
      }
      contentWindow = appWindows[request.appId].contentWindow;
    }
    if (!contentWindow) {
      // Global functions, not requiring a window.
      switch (request.func) {
        case 'openMainWindow':
          test.util.openMainWindow(request.args[0], sendResponse);
          return true;
        case 'getErrorCount':
          sendResponse(test.util.getErrorCount());
          return true;
        default:
          console.error('Global function ' + request.func + ' not found.');
      }
    } else {
      // Functions working on a window.
      switch (request.func) {
        case 'getSelectedFiles':
          sendResponse(test.util.getSelectedFiles(contentWindow));
          return false;
        case 'getFileList':
          sendResponse(test.util.getFileList(contentWindow));
          return false;
        case 'performAutocompleteAndWait':
          test.util.performAutocompleteAndWait(
              contentWindow, request.args[0], request.args[1], sendResponse);
          return true;
        case 'waitForFileListChange':
          test.util.waitForFileListChange(
              contentWindow, request.args[0], sendResponse);
          return true;
        case 'waitAndAcceptDialog':
          test.util.waitAndAcceptDialog(contentWindow, sendResponse);
          return true;
        case 'selectFile':
          sendResponse(test.util.selectFile(contentWindow, request.args[0]));
          return false;
        case 'selectVolume':
          sendResponse(test.util.selectVolume(contentWindow, request.args[0]));
          return false;
        case 'fakeKeyDown':
          sendResponse(test.util.fakeKeyDown(contentWindow,
                                             request.args[0],
                                             request.args[1],
                                             request.args[2]));
          return false;
        case 'fakeMouseClick':
          sendResponse(test.util.fakeMouseClick(
              contentWindow, request.args[0]));
          return false;
        case 'fakeMouseDown':
          sendResponse(test.util.fakeMouseDown(
              contentWindow, request.args[0]));
          return false;
        case 'copyFile':
          sendResponse(test.util.copyFile(contentWindow, request.args[0]));
          return false;
        case 'deleteFile':
          sendResponse(test.util.deleteFile(contentWindow, request.args[0]));
          return false;
        default:
          console.error('Window function ' + request.func + ' not found.');
      }
    }
    return false;
  });
};

// Register the test utils.
test.util.registerRemoteTestUtils();
