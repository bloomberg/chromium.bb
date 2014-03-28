// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

(function() {

/**
 * Extension ID of Video Player.
 * @type {string}
 * @const
 */
var VIDEO_PLAYER_EXTENSIONS_ID = 'jcgeabjmjgoblfofpppfkcoakmfobdko';

/**
 * Calls a remote test util in video player. See: test_util.js.
 *
 * @param {string} func Function name.
 * @param {?string} filename Target window's file name,
 * @param {Array.<*>} args Array of arguments.
 * @param {function(*)=} opt_callback Callback handling the function's result.
 * @return {Promise} Promise to be fulfilled with the result of the remote
 *     utility.
 */
function callRemoteTestUtilInVideoPlayer(func, filename, args, opt_callback) {
  return new Promise(function(onFulfilled) {
    chrome.runtime.sendMessage(
        VIDEO_PLAYER_EXTENSIONS_ID, {
          func: func,
          file: filename,
          args: args
        },
        function() {
          if (opt_callback)
            opt_callback.apply(null, arguments);
          onFulfilled(arguments[0]);
        });
  }).catch(function(err) {
    console.log('Error in calling sendMessage: ' + (err.message || err));
    return Promise.resolve(false);
  });
}

/**
 * Waits until a window having the given filename appears.
 * @param {string} filename Filename of the requested window.
 * @param {Promise} promise Promise to be fulfilled with a found window's ID.
 */
function waitForPlaying(filename) {
  return repeatUntil(function() {
    return callRemoteTestUtilInVideoPlayer('isPlaying',
                                           filename,
                                           []).
        then(function(result) {
          if (result)
            return true;
          return pending('Window with the prefix %s is not found.', filename);
        });
  });
}

/**
 * Tests if the video player shows up for the selected movie and that it is
 * loaded successfully.
 *
 * @param {string} path Directory path to be tested.
 */
function videoOpen(path) {
  var appId;
  StepsRunner.run([
    function() {
      setupAndWaitUntilReady(null, path, this.next);
    },
    function(inAppId) {
      appId = inAppId;
      // Select the song.
      callRemoteTestUtil(
          'openFile', appId, ['world.ogv'], this.next);
    },
    function(result) {
      chrome.test.assertTrue(result);
      // Wait for the video player.
      waitForPlaying('world.ogv').then(this.next);
    },
    function(result) {
      chrome.test.assertTrue(result);
      callRemoteTestUtilInVideoPlayer(
          'getErrorCount', null, [], this.next);
    },
    function(errorCount) {
      chrome.test.assertEq(errorCount, 0);
      checkIfNoErrorsOccured(this.next);
    }
  ]);
}

// Exports test functions.
testcase.videoOpenDrive = function() {
  videoOpen(RootPath.DRIVE);
};

testcase.videoOpenDownloads = function() {
  videoOpen(RootPath.DOWNLOADS);
};

})();
