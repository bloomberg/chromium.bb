// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var errorReported = false;

var testStartSessionErrorReport = function() {
  function onSessionError(errorSinkId, errorMessage){
    chrome.test.assertEq(1, errorSinkId);
    chrome.test.assertEq('timeout_error', errorMessage.type);
    chrome.test.assertEq('Sink became unresponsive', errorMessage.description);
    errorReported = true;
  };
  chrome.displaySource.onSessionErrorOccured.addListener(onSessionError);

  function onSessionTerminated(terminatedSink) {
    chrome.test.assertEq(1, terminatedSink);
    chrome.test.assertTrue(errorReported);
    chrome.test.succeed("SessionTerminated Received");
  };
  chrome.displaySource.onSessionTerminated.addListener(onSessionTerminated);

  chrome.tabs.getCurrent(function(tab) {
    var sink_id = 1;
    var constraints = {
        audio: false,
        video: true,
    };

    function onStream(stream) {
      var session_info = {
          sinkId: sink_id,
          videoTrack: stream.getVideoTracks()[0],
          audioTrack: stream.getAudioTracks()[0],
          authenticationInfo: { method: "PIN", data: "1234" }
      };

      function onStarted() {
        if (chrome.runtime.error) {
          console.log('The Session to sink ' + sink_id
            + 'could not start, error: '
            + chrome.runtime.lastError.message);
          chrome.test.fail();
        } else {
          chrome.test.assertNoLastError();
        }
      }
      chrome.displaySource.startSession(session_info, onStarted);
    }
    chrome.tabCapture.capture(constraints, onStream);
  });
};

chrome.test.runTests([testStartSessionErrorReport]);
