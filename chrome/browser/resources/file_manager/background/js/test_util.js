// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Namespace for test related things.
 */
var test = test || {};

/**
 * Namespace for test utility functions.
 *
 * Public functions in the test.util.sync and the test.util.async namespaces are
 * published to test cases and can be called by using callRemoteTestUtil. The
 * arguments are serialized as JSON internally. If application ID is passed to
 * callRemoteTestUtil, the content window of the application is added as the
 * first argument. The functions in the test.util.async namespace are passed the
 * callback function as the last argument.
 */
test.util = {};

/**
 * Namespace for synchronous utility functions.
 */
test.util.sync = {};

/**
 * Namespace for asynchronous utility functions.
 */
test.util.async = {};

/**
 * Extension ID of the testing extension.
 * @type {string}
 * @const
 */
test.util.TESTING_EXTENSION_ID = 'oobinhbdbiehknkpbpejbbpdbkdjmoco';

/**
 * Interval of checking a condition in milliseconds.
 * @type {number}
 * @const
 * @private
 */
test.util.WAITTING_INTERVAL_ = 50;

/**
 * Repeats the function until it returns true.
 * @param {function()} closure Function expected to return true.
 * @private
 */
test.util.repeatUntilTrue_ = function(closure) {
  var step = function() {
    if (closure())
      return;
    setTimeout(step, test.util.WAITTING_INTERVAL_);
  };
  step();
};

/**
 * Opens the main Files.app's window and waits until it is ready.
 *
 * @param {Object} appState App state.
 * @param {function(string)} callback Completion callback with the new window's
 *     App ID.
 */
test.util.async.openMainWindow = function(appState, callback) {
  var steps = [
    function() {
      launchFileManager(appState,
                        undefined,  // opt_type
                        undefined,  // opt_id
                        steps.shift());
    },
    function(appId) {
      test.util.repeatUntilTrue_(function() {
        if (!background.appWindows[appId])
          return false;
        var contentWindow = background.appWindows[appId].contentWindow;
        var table = contentWindow.document.querySelector('#detail-table');
        if (!table)
          return false;
        callback(appId);
        return true;
      });
    }
  ];
  steps.shift()();
};

/**
 * Waits for a window with the specified App ID prefix. Eg. `files` will match
 * windows such as files#0, files#1, etc.
 *
 * @param {string} appIdPrefix ID prefix of the requested window.
 * @param {function(string)} callback Completion callback with the new window's
 *     App ID.
 */
test.util.async.waitForWindow = function(appIdPrefix, callback) {
  test.util.repeatUntilTrue_(function() {
    for (var appId in background.appWindows) {
      if (appId.indexOf(appIdPrefix) == 0 &&
          background.appWindows[appId].contentWindow) {
        callback(appId);
        return true;
      }
    }
    return false;
  });
};

/**
 * Gets a document in the Files.app's window, including iframes.
 *
 * @param {Window} contentWindow Window to be used.
 * @param {string=} opt_iframeQuery Query for the iframe.
 * @return {Document=} Returns the found document or undefined if not found.
 * @private
 */
test.util.sync.getDocument_ = function(contentWindow, opt_iframeQuery) {
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
test.util.sync.getErrorCount = function() {
  var totalCount = JSErrorCount;
  for (var appId in background.appWindows) {
    var contentWindow = background.appWindows[appId].contentWindow;
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
test.util.sync.resizeWindow = function(contentWindow, width, height) {
  background.appWindows[contentWindow.appID].resizeTo(width, height);
  return true;
};

/**
 * Returns an array with the files currently selected in the file manager.
 *
 * @param {Window} contentWindow Window to be tested.
 * @return {Array.<string>} Array of selected files.
 */
test.util.sync.getSelectedFiles = function(contentWindow) {
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
test.util.sync.getFileList = function(contentWindow) {
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
  return fileList;
};

/**
 * Checkes if the given label and path of the volume are selected.
 * @param {Window} contentWindow Window to be tested.
 * @param {string} label Correct label the selected volume should have.
 * @param {string} path Correct path the selected volume should have.
 * @return {boolean} True for success.
 */
test.util.sync.checkSelectedVolume = function(contentWindow, label, path) {
  var list = contentWindow.document.querySelector('#navigation-list');
  var rows = list.querySelectorAll('li');
  var selected = [];
  for (var i = 0; i < rows.length; ++i) {
    if (rows[i].hasAttribute('selected'))
      selected.push(rows[i]);
  }
  // Selected item must be one.
  if (selected.length !== 1)
    return false;

  if (selected[0].modelItem.path !== path ||
      selected[0].querySelector('.root-label').textContent !== label) {
    return false;
  }

  return true;
};

/**
 * Waits until the window is set to the specified dimensions.
 *
 * @param {Window} contentWindow Window to be tested.
 * @param {number} width Requested width.
 * @param {number} height Requested height.
 * @param {function(Object)} callback Success callback with the dimensions.
 */
test.util.async.waitForWindowGeometry = function(
    contentWindow, width, height, callback) {
  test.util.repeatUntilTrue_(function() {
    if (contentWindow.innerWidth == width &&
        contentWindow.innerHeight == height) {
      callback({width: width, height: height});
      return true;
    }
    return false;
  });
};

/**
 * Waits for an element and returns it as an array of it's attributes.
 *
 * @param {Window} contentWindow Window to be tested.
 * @param {string} targetQuery Query to specify the element.
 * @param {?string} iframeQuery Iframe selector or null if no iframe.
 * @param {boolean=} opt_inverse True if the function should return if the
 *    element disappears, instead of appearing.
 * @param {function(Object)} callback Callback with a hash array of attributes
 *     and contents as text.
 */
test.util.async.waitForElement = function(
    contentWindow, targetQuery, iframeQuery, opt_inverse, callback) {
  test.util.repeatUntilTrue_(function() {
    var doc = test.util.sync.getDocument_(contentWindow, iframeQuery);
    if (!doc)
      return false;
    var element = doc.querySelector(targetQuery);
    if (!element)
      return !!opt_inverse;
    var attributes = {};
    for (var i = 0; i < element.attributes.length; i++) {
      attributes[element.attributes[i].nodeName] =
          element.attributes[i].nodeValue;
    }
    var text = element.textContent;
    callback({attributes: attributes, text: text});
    return !opt_inverse;
  });
};

/**
 * Calls getFileList until the number of displayed files is different from
 * lengthBefore.
 *
 * @param {Window} contentWindow Window to be tested.
 * @param {number} lengthBefore Number of items visible before.
 * @param {function(Array.<Array.<string>>)} callback Change callback.
 */
test.util.async.waitForFileListChange = function(
    contentWindow, lengthBefore, callback) {
  test.util.repeatUntilTrue_(function() {
    var files = test.util.sync.getFileList(contentWindow);
    files.sort();
    var notReadyRows = files.filter(function(row) {
      return row.filter(function(cell) { return cell == '...'; }).length;
    });
    if (notReadyRows.length === 0 &&
        files.length !== lengthBefore &&
        files.length !== 0) {
      callback(files);
      return true;
    } else {
      return false;
    }
  });
};

/**
 * Returns an array of items on the file manager's autocomplete list.
 *
 * @param {Window} contentWindow Window to be tested.
 * @return {Array.<string>} Array of items.
 */
test.util.sync.getAutocompleteList = function(contentWindow) {
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
test.util.async.performAutocompleteAndWait = function(
    contentWindow, query, numExpectedItems, callback) {
  // Dispatch a 'focus' event to the search box so that the autocomplete list
  // is attached to the search box. Note that calling searchBox.focus() won't
  // dispatch a 'focus' event.
  var searchBox = contentWindow.document.querySelector('#search-box input');
  var focusEvent = contentWindow.document.createEvent('Event');
  focusEvent.initEvent('focus', true /* bubbles */, true /* cancelable */);
  searchBox.dispatchEvent(focusEvent);

  // Change the value of the search box and dispatch an 'input' event so that
  // the autocomplete query is processed.
  searchBox.value = query;
  var inputEvent = contentWindow.document.createEvent('Event');
  inputEvent.initEvent('input', true /* bubbles */, true /* cancelable */);
  searchBox.dispatchEvent(inputEvent);

  test.util.repeatUntilTrue_(function() {
    var items = test.util.sync.getAutocompleteList(contentWindow);
    if (items.length >= numExpectedItems) {
      callback(items);
      return true;
    } else {
      return false;
    }
  });
};

/**
 * Waits until a dialog with an OK button is shown and accepts it.
 *
 * @param {Window} contentWindow Window to be tested.
 * @param {function()} callback Success callback.
 */
test.util.async.waitAndAcceptDialog = function(contentWindow, callback) {
  test.util.repeatUntilTrue_(function() {
    var button = contentWindow.document.querySelector('.cr-dialog-ok');
    if (!button)
      return false;
    button.click();
    // Wait until the dialog is removed from the DOM.
    test.util.repeatUntilTrue_(function() {
      if (contentWindow.document.querySelector('.cr-dialog-container'))
        return false;
      callback();
      return true;
    });
    return true;
  });
};

/**
 * Fakes pressing the down arrow until the given |filename| is selected.
 *
 * @param {Window} contentWindow Window to be tested.
 * @param {string} filename Name of the file to be selected.
 * @return {boolean} True if file got selected, false otherwise.
 */
test.util.sync.selectFile = function(contentWindow, filename) {
  var table = contentWindow.document.querySelector('#detail-table');
  var rows = table.querySelectorAll('li');
  for (var index = 0; index < rows.length; ++index) {
    test.util.sync.fakeKeyDown(contentWindow, '#file-list', 'Down', false);
    var selection = test.util.sync.getSelectedFiles(contentWindow);
    if (selection.length === 1 && selection[0] === filename)
      return true;
  }
  console.error('Failed to select file "' + filename + '"');
  return false;
};

/**
 * Open the file by selectFile and fakeMouseDoubleClick.
 *
 * @param {Window} contentWindow Window to be tested.
 * @param {string} filename Name of the file to be opened.
 * @return {boolean} True if file got selected and a double click message is
 *     sent, false otherwise.
 */
test.util.sync.openFile = function(contentWindow, filename) {
  var query = '#file-list li.table-row[selected] .filename-label span';
  return test.util.sync.selectFile(contentWindow, filename) &&
         test.util.sync.fakeMouseDoubleClick(contentWindow, query);
};

/**
 * Selects a volume specified by its icon name
 *
 * @param {Window} contentWindow Window to be tested.
 * @param {string} iconName Name of the volume icon.
 * @param {function(boolean)} callback Callback function to notify the caller
 *     whether the target is found and mousedown and click events are sent.
 */
test.util.async.selectVolume = function(contentWindow, iconName, callback) {
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
        if (!(test.util.sync.fakeMouseDown(contentWindow, driveQuery) &&
              test.util.sync.fakeMouseClick(contentWindow, driveQuery))) {
          callback(false);
          return;
        }
        preSelection = true;
      }
      setTimeout(steps.checkQuery, 50);
    },
    sendEvents: function() {
      // To change the selected volume, we have to send both events 'mousedown'
      // and 'click' to the navigation list.
      callback(test.util.sync.fakeMouseDown(contentWindow, query) &&
               test.util.sync.fakeMouseClick(contentWindow, query));
    }
  };
  steps.checkQuery();
};

/**
 * Waits the contents of file list becomes to equal to expected contents.
 *
 * @param {Window} contentWindow Window to be tested.
 * @param {Array.<Array.<string>>} expected Expected contents of file list.
 * @param {{orderCheck:boolean=, ignoreLastModifiedTime:boolean=}=} opt_options
 *     Options of the comparison. If orderCheck is true, it also compares the
 *     order of files. If ignoreLastModifiedTime is true, it compares the file
 *     without its last modified time.
 * @param {function()} callback Callback function to notify the caller that
 *     expected files turned up.
 */
test.util.async.waitForFiles = function(
    contentWindow, expected, opt_options, callback) {
  var options = opt_options || {};
  test.util.repeatUntilTrue_(function() {
    var files = test.util.sync.getFileList(contentWindow);
    if (!options.orderCheck) {
      files.sort();
      expected.sort();
    }
    if (options.ignoreLastModifiedTime) {
      for (var i = 0; i < Math.min(files.length, expected.length); i++) {
        files[i][3] = '';
        expected[i][3] = '';
      }
    }
    if (chrome.test.checkDeepEq(expected, files)) {
      callback(true);
      return true;
    }
    return false;
  });
};

/**
 * Executes Javascript code on a webview and returns the result.
 *
 * @param {Window} contentWindow Window to be tested.
 * @param {string} webViewQuery Selector for the web view.
 * @param {string} code Javascript code to be executed within the web view.
 * @param {function(*)} callback Callback function with results returned by the
 *     script.
 */
test.util.async.executeScriptInWebView = function(
    contentWindow, webViewQuery, code, callback) {
  var webView = contentWindow.document.querySelector(webViewQuery);
  webView.executeScript({code: code}, callback);
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
test.util.sync.sendEvent = function(
    contentWindow, targetQuery, event, opt_iframeQuery) {
  var doc = test.util.sync.getDocument_(contentWindow, opt_iframeQuery);
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
 * Sends an fake event having the specified type to the target query.
 *
 * @param {Window} contentWindow Window to be tested.
 * @param {string} targetQuery Query to specify the element.
 * @param {string} event Type of event.
 * @return {boolean} True if the event is sent to the target, false otherwise.
 */
test.util.sync.fakeEvent = function(contentWindow, targetQuery, event) {
  return test.util.sync.sendEvent(
      contentWindow, targetQuery, new Event(event));
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
test.util.sync.fakeKeyDown = function(
    contentWindow, targetQuery, keyIdentifier, ctrl, opt_iframeQuery) {
  var event = new KeyboardEvent(
      'keydown',
      { bubbles: true, keyIdentifier: keyIdentifier, ctrlKey: ctrl });
  return test.util.sync.sendEvent(
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
test.util.sync.fakeMouseClick = function(
    contentWindow, targetQuery, opt_iframeQuery) {
  var event = new MouseEvent('click', { bubbles: true, detail: 1 });
  return test.util.sync.sendEvent(
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
test.util.sync.fakeMouseDoubleClick = function(
    contentWindow, targetQuery, opt_iframeQuery) {
  // Double click is always preceded with a single click.
  if (!test.util.sync.fakeMouseClick(
      contentWindow, targetQuery, opt_iframeQuery)) {
    return false;
  }

  // Send the second click event, but with detail equal to 2 (number of clicks)
  // in a row.
  var event = new MouseEvent('click', { bubbles: true, detail: 2 });
  if (!test.util.sync.sendEvent(
      contentWindow, targetQuery, event, opt_iframeQuery)) {
    return false;
  }

  // Send the double click event.
  var event = new MouseEvent('dblclick', { bubbles: true });
  if (!test.util.sync.sendEvent(
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
test.util.sync.fakeMouseDown = function(
    contentWindow, targetQuery, opt_iframeQuery) {
  var event = new MouseEvent('mousedown', { bubbles: true });
  return test.util.sync.sendEvent(
      contentWindow, targetQuery, event, opt_iframeQuery);
};

/**
 * Sends a fake mouse up event to the element specified by |targetQuery|.
 *
 * @param {Window} contentWindow Window to be tested.
 * @param {string} targetQuery Query to specify the element.
 * @param {string=} opt_iframeQuery Optional iframe selector.
 * @return {boolean} True if the event is sent to the target, false otherwise.
 */
test.util.sync.fakeMouseUp = function(
    contentWindow, targetQuery, opt_iframeQuery) {
  var event = new MouseEvent('mouseup', { bubbles: true });
  return test.util.sync.sendEvent(
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
test.util.sync.copyFile = function(contentWindow, filename) {
  if (!test.util.sync.selectFile(contentWindow, filename))
    return false;
  // Ctrl+C and Ctrl+V
  test.util.sync.fakeKeyDown(contentWindow, '#file-list', 'U+0043', true);
  test.util.sync.fakeKeyDown(contentWindow, '#file-list', 'U+0056', true);
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
test.util.sync.deleteFile = function(contentWindow, filename) {
  if (!test.util.sync.selectFile(contentWindow, filename))
    return false;
  // Delete
  test.util.sync.fakeKeyDown(contentWindow, '#file-list', 'U+007F', false);
  return true;
};

/**
 * Wait for the elements' style to be changed as the expected values.  The
 * queries argument is a list of object that have the query property and the
 * styles property. The query property is a string query to specify the
 * element. The styles property is a string map of the style name and its
 * expected value.
 *
 * @param {Window} contentWindow Window to be tested.
 * @param {Array.<object>} queries Queries that specifies the elements and
 *   expected styles.
 * @param {function()} callback Callback function to be notified the change of
 *     the styles.
 */
test.util.async.waitForStyles = function(contentWindow, queries, callback) {
  test.util.repeatUntilTrue_(function() {
    for (var i = 0; i < queries.length; i++) {
      var element = contentWindow.document.querySelector(queries[i].query);
      var styles = queries[i].styles;
      for (var name in styles) {
        if (contentWindow.getComputedStyle(element)[name] != styles[name])
          return false;
      }
    }
    callback();
    return true;
  });
};

/**
 * Execute a command on the document in the specified window.
 *
 * @param {Window} contentWindow Window to be tested.
 * @param {string} command Command name.
 * @return {boolean} True if the command is executed successfully.
 */
test.util.sync.execCommand = function(contentWindow, command) {
  return contentWindow.document.execCommand(command);
};

/**
 * Registers message listener, which runs test utility functions.
 */
test.util.registerRemoteTestUtils = function() {
  // Register the message listener.
  var onMessage = chrome.runtime ? chrome.runtime.onMessageExternal :
      chrome.extension.onMessageExternal;
  // Return true for asynchronous functions and false for synchronous.
  onMessage.addListener(function(request, sender, sendResponse) {
    // Check the sender.
    if (sender.id != test.util.TESTING_EXTENSION_ID) {
      console.error('The testing extension must be white-listed.');
      return false;
    }
    // Set a global flag that we are in tests, so other components are aware
    // of it.
    window.IN_TEST = true;
    // Check the function name.
    if (!request.func || request.func[request.func.length - 1] == '_') {
      request.func = '';
    }
    // Prepare arguments.
    var args = request.args.slice();  // shallow copy
    if (request.appId) {
      if (!background.appWindows[request.appId]) {
        console.error('Specified window not found.');
        return false;
      }
      args.unshift(background.appWindows[request.appId].contentWindow);
    }
    // Call the test utility function and respond the result.
    if (test.util.async[request.func]) {
      args[test.util.async[request.func].length - 1] = function() {
        console.debug('Received the result of ' + request.func);
        sendResponse.apply(null, arguments);
      };
      console.debug('Waiting for the result of ' + request.func);
      test.util.async[request.func].apply(null, args);
      return true;
    } else if (test.util.sync[request.func]) {
      sendResponse(test.util.sync[request.func].apply(null, args));
      return false;
    } else {
      console.error('Invalid function name.');
      return false;
    }
  });
};

// Register the test utils.
test.util.registerRemoteTestUtils();
