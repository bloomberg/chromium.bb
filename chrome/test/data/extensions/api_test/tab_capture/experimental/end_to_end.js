// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  // Opens two tabs: One that generates a test signal (i.e., cycling through
  // red-->green-->blue color fills), and one that receives a video stream
  // consisting of a capture of the first tab.  The "receiver" looks at the
  // video stream and calls back here once it has seen a video frame for each
  // of the three colors.
  function endToEndVideo() {
    chrome.tabs.create(
      { url: "sender.html", active: true },
      function (senderTab) {
        chrome.tabCapture.capture(
          { video: true, audio: true,
            videoConstraints: {
              mandatory: {
                minWidth: 64,
                minHeight: 64
              }
            }
          },
          function (stream) {
            chrome.test.assertTrue(stream != null);

            // Note: Receiver is allowed access to our window object for this
            // test.  It needs access to the stream object, and also notifies us
            // when the test is complete via a receivedAllColors() method
            // defined here:
            var receiverWindow = null;
            window.activeStream = stream;
            window.receivedAllColors = function() {
              window.activeStream.stop();
              chrome.tabs.remove(senderTab.id);
              receiverWindow.close();
              chrome.test.succeed();
            };
            receiverWindow = window.open("receiver.html");
            // TODO(miu): Once off-screen tab rendering for mirroring is
            // working, remove the following line:
            chrome.tabs.update(senderTab.id, { active: true });
          });
      });
  }

  // TODO(miu): Once the WebAudio API is finalized, we should add a test to emit
  // a tone from the sender page, and have the receiver page check for the audio
  // tone.

]);
