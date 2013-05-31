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
  var appId;
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
  launchFileManager({defaultPath: path},
                    undefined,  // opt_type
                    undefined,  // opt_id
                    function(id) {
                      appId = id;
                      helper();
                    });
};

/**
 * Waits for a window with the specified App ID prefix. Eg. `files` will match
 * windows such as files#0, files#1, etc.
 *
 * @param {string} appIdPrefix ID prefix of the requested window.
 * @param {function(string)} callback Completion callback with the new window's
 *     App ID.
 */
test.util.waitForWindow = function(appIdPrefix, callback) {
  var appId;
  function helper() {
    for (var appId in appWindows) {
      if (appId.indexOf(appIdPrefix) == 0) {
        callback(appId);
        return;
      }
    }
    window.setTimeout(helper, 50);
  }
  helper();
};

/**
 * Gets a document in the Files.app's window, including iframes.
 *
 * @param {Window} contentWindow Window to be used.
 * @param {string=} opt_iframeQuery Query for the iframe.
 * @return {Document=} Returns the found document or undefined if not found.
 * @private
 */
test.util.getDocument_ = function(contentWindow, opt_iframeQuery) {
  if (opt_iframeQuery) {
    var iframe = contentWindow.document.querySelector(opt_iframeQuery);
    return iframe && iframe.contentWindow && iframe.contentWindow.document;
  }

  return contentWindow.document;
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
 * Resizes the window to the specified dimensions.
 *
 * @param {Window} contentWindow Window to be tested.
 * @param {number} width Window width.
 * @param {number} height Window height.
 * @return {boolean} True for success.
 */
test.util.resizeWindow = function(contentWindow, width, height) {
  appWindows[contentWindow.appID].resizeTo(width, height);
  return true;
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
 * Waits until the window is set to the specified dimensions.
 *
 * @param {Window} contentWindow Window to be tested.
 * @param {number} width Requested width.
 * @param {number} height Requested height.
 * @param {function(Object)} callback Success callback with the dimensions.
 */
test.util.waitForWindowGeometry = function(
    contentWindow, width, height, callback) {
  function helper() {
    if (contentWindow.innerWidth == width &&
        contentWindow.innerHeight == height) {
       callback({width: width, height: height});
       return;
    }
    window.setTimeout(helper, 50);
  }
  helper();
};

/**
 * Waits for an element and returns it as an array of it's attributes.
 *
 * @param {Window} contentWindow Window to be tested.
 * @param {string} targetQuery Query to specify the element.
 * @param {?string} iframeQuery Iframe selector or null if no iframe.
 * @param {function(Object)} callback Callback with a hash array of attributes
 *     and contents as text.
 */
test.util.waitForElement = function(
    contentWindow, targetQuery, iframeQuery, callback) {
  function helper() {
    var doc = test.util.getDocument_(contentWindow, iframeQuery);
    if (doc) {
      var element = doc.querySelector(targetQuery);
      if (element) {
        var attributes = {};
        for (var i = 0; i < element.attributes.length; i++) {
          attributes[element.attributes[i].nodeName] =
              element.attributes[i].nodeValue;
        }
        var text = element.textContent;
        callback({attributes: attributes, text: text});
        return;
      }
    }
    window.setTimeout(helper, 50);
  }
  helper();
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
 * @param {function(boolean)} callback Callback function to notify the caller
 *     whether the target is found and mousedown and click events are sent.
 */
test.util.selectVolume = function(contentWindow, iconName, callback) {
  var query = '[volume-type-icon=' + iconName + ']';
  var driveQuery = '[volume-type-icon=drive]';
  var isDriveSubVolume = iconName == 'drive_recent' ||
                         iconName == 'drive_shared_with_me' ||
                         iconName == 'drive_offline';
  var preSelection = false;
  var steps = {
    checkQuery: function() {
      if (contentWindow.document.querySelector(query)) {
        steps.sendEvents();
        return;
      }
      // If the target volume is sub-volume of drive, we must click 'drive'
      // before clicking the sub-item.
      if (!preSelection) {
        if (!isDriveSubVolume) {
          callback(false);
          return;
        }
        if (!(test.util.fakeMouseDown(contentWindow, driveQuery) &&
              test.util.fakeMouseClick(contentWindow, driveQuery))) {
          callback(false);
          return;
        }
        preSelection = true;
      }
      setTimeout(steps.checkQuery, 50);
    },
    sendEvents: function() {
      // To change the selected volume, we have to send both events 'mousedown'
      // and 'click' to the volume list.
      callback(test.util.fakeMouseDown(contentWindow, query) &&
               test.util.fakeMouseClick(contentWindow, query));
    }
  };
  steps.checkQuery();
};

/**
 * Waits the contents of file list becomes to equal to expected contents.
 *
 * @param {Window} contentWindow Window to be tested.
 * @param {Array.<Array.<string>>} expected Expected contents of file list.
 * @param {function(boolean)} callback Callback function to notify the caller
 *     whether expected files turned up or not.
 */
test.util.waitForFiles = function(contentWindow,
                                  expected,
                                  callback) {
  var step = function() {
    if (chrome.test.checkDeepEq(expected,
                                test.util.getFileList(contentWindow))) {
      callback(true);
      return;
    }
    setTimeout(step, 50);
  };
  step();
};

/**
 * Sends an event to the element specified by |targetQuery|.
 *
 * @param {Window} contentWindow Window to be tested.
 * @param {string} targetQuery Query to specify the element.
 * @param {Event} event Event to be sent.
 * @param {string=} opt_iframeQuery Optional iframe selector.
 * @return {boolean} True if the event is sent to the target, false otherwise.
 */
test.util.sendEvent = function(
    contentWindow, targetQuery, event, opt_iframeQuery) {
  var doc = test.util.getDocument_(contentWindow, opt_iframeQuery);
  if (doc) {
    var target = doc.querySelector(targetQuery);
    if (target) {
      target.dispatchEvent(event);
      return true;
    }
  }
  console.error('Target element for ' + targetQuery + ' not found.');
  return false;
};

/**
 * Sends a fake key event to the element specified by |targetQuery| with the
 * given |keyIdentifier| and optional |ctrl| modifier to the file manager.
 *
 * @param {Window} contentWindow Window to be tested.
 * @param {string} targetQuery Query to specify the element.
 * @param {string} keyIdentifier Identifier of the emulated key.
 * @param {boolean} ctrl Whether CTRL should be pressed, or not.
 * @param {string=} opt_iframeQuery Optional iframe selector.
 * @return {boolean} True if the event is sent to the target, false otherwise.
 */
test.util.fakeKeyDown = function(
    contentWindow, targetQuery, keyIdentifier, ctrl, opt_iframeQuery) {
  var event = new KeyboardEvent(
      'keydown',
      { bubbles: true, keyIdentifier: keyIdentifier, ctrlKey: ctrl });
  return test.util.sendEvent(
      contentWindow, targetQuery, event, opt_iframeQuery);
};

/**
 * Sends a fake mouse click event (left button, single click) to the element
 * specified by |targetQuery|.
 *
 * @param {Window} contentWindow Window to be tested.
 * @param {string} targetQuery Query to specify the element.
 * @param {string=} opt_iframeQuery Optional iframe selector.
 * @return {boolean} True if the event is sent to the target, false otherwise.
 */
test.util.fakeMouseClick = function(
    contentWindow, targetQuery, opt_iframeQuery) {
  var event = new MouseEvent('click', { bubbles: true, detail: 1 });
  return test.util.sendEvent(
      contentWindow, targetQuery, event, opt_iframeQuery);
};

/**
 * Simulates a fake double click event (left button) to the element specified by
 * |targetQuery|.
 *
 * @param {Window} contentWindow Window to be tested.
 * @param {string} targetQuery Query to specify the element.
 * @param {string=} opt_iframeQuery Optional iframe selector.
 * @return {boolean} True if the event is sent to the target, false otherwise.
 */
test.util.fakeMouseDoubleClick = function(
    contentWindow, targetQuery, opt_iframeQuery) {
  // Double click is always preceeded with a single click.
  if (!test.util.fakeMouseClick(contentWindow, targetQuery, opt_iframeQuery))
    return false;

  // Send the second click event, but with detail equal to 2 (number of clicks)
  // in a row.
  var event = new MouseEvent('click', { bubbles: true, detail: 2 });
  if (!test.util.sendEvent(
      contentWindow, targetQuery, event, opt_iframeQuery)) {
    return false;
  }

  // Send the double click event.
  var event = new MouseEvent('dblclick', { bubbles: true });
  if (!test.util.sendEvent(
      contentWindow, targetQuery, event, opt_iframeQuery)) {
    return false;
  }

  return true;
};

/**
 * Sends a fake mouse down event to the element specified by |targetQuery|.
 *
 * @param {Window} contentWindow Window to be tested.
 * @param {string} targetQuery Query to specify the element.
 * @param {string=} opt_iframeQuery Optional iframe selector.
 * @return {boolean} True if the event is sent to the target, false otherwise.
 */
test.util.fakeMouseDown = function(
    contentWindow, targetQuery, opt_iframeQuery) {
  var event = new MouseEvent('mousedown', { bubbles: true });
  return test.util.sendEvent(
      contentWindow, targetQuery, event, opt_iframeQuery);
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
  // Return true for asynchronous functions and false for synchronous.
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
        case 'waitForWindow':
          test.util.waitForWindow(request.args[0], sendResponse);
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
        case 'resizeWindow':
          sendResponse(test.util.resizeWindow(
              contentWindow, request.args[0], request.args[1]));
          return false;
        case 'getSelectedFiles':
          sendResponse(test.util.getSelectedFiles(contentWindow));
          return false;
        case 'getFileList':
          sendResponse(test.util.getFileList(contentWindow));
          return false;
        case 'waitForWindowGeometry':
          test.util.waitForWindowGeometry(
              contentWindow, request.args[0], request.args[1], sendResponse);
          return true;
        case 'waitForElement':
          test.util.waitForElement(
              contentWindow, request.args[0], request.args[1], sendResponse);
          return true;
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
          test.util.selectVolume(contentWindow, request.args[0], sendResponse);
          return true;
        case 'fakeKeyDown':
          sendResponse(test.util.fakeKeyDown(contentWindow,
                                             request.args[0],
                                             request.args[1],
                                             request.args[2]));
          return false;
        case 'fakeMouseClick':
          sendResponse(test.util.fakeMouseClick(
              contentWindow, request.args[0], request.args[1]));
          return false;
        case 'fakeMouseDoubleClick':
          sendResponse(test.util.fakeMouseDoubleClick(
              contentWindow, request.args[0], request.args[1]));
          return false;
        case 'fakeMouseDown':
          sendResponse(test.util.fakeMouseDown(
              contentWindow, request.args[0], request.args[1]));
          return false;
        case 'copyFile':
          sendResponse(test.util.copyFile(contentWindow, request.args[0]));
          return false;
        case 'deleteFile':
          sendResponse(test.util.deleteFile(contentWindow, request.args[0]));
          return false;
        case 'waitForFiles':
          test.util.waitForFiles(contentWindow, request.args[0], sendResponse);
          return true;
        case 'execCommand':
          sendResponse(contentWindow.document.execCommand(request.args[0]));
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
