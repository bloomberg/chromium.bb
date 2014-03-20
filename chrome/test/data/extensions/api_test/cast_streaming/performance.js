// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Run a cast v2 mirroring session for 10 seconds.

chrome.test.runTests([
  function sendTestPatterns() {
    // The receive port changes between browser_test invocations, and is passed
    // as an query parameter in the URL.
    var recvPort;
    try {
      recvPort = parseInt(window.location.search.substring("?port=".length));
      chrome.test.assertTrue(recvPort > 0);
    } catch (err) {
      chrome.test.fail("Error parsing ?port=### -- " + err.message);
      return;
    }

    var width = 1920;
    var height = 1080;
    var frameRate = 30;

    chrome.tabCapture.capture(
        { video: true,
          audio: true,
          videoConstraints: {
            mandatory: {
              minWidth: width,
              minHeight: height,
              maxWidth: width,
              maxHeight: height,
              maxFrameRate: frameRate,
            }
          }
        },
        function startStreamingTestPatterns(captureStream) {
          chrome.test.assertTrue(!!captureStream);
          chrome.cast.streaming.session.create(
              captureStream.getAudioTracks()[0],
              captureStream.getVideoTracks()[0],
              function (audioId, videoId, udpId) {
                chrome.cast.streaming.udpTransport.setDestination(
                    udpId, { address: "127.0.0.1", port: recvPort } );
                var rtpStream = chrome.cast.streaming.rtpStream;
                rtpStream.start(audioId,
                                rtpStream.getSupportedParams(audioId)[0]);
                var videoParams = rtpStream.getSupportedParams(videoId)[0];
                videoParams.payload.width = width;
                videoParams.payload.height = height;
                videoParams.payload.clockRate = frameRate;
                rtpStream.start(videoId, videoParams);
                setTimeout(function () {
                  chrome.test.succeed();
                  rtpStream.stop(audioId);
                  rtpStream.stop(videoId);
                }, 20000);  // 20 seconds
              });
        });
  }
]);
