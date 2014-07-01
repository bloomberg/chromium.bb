// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This code uses the tab capture and Cast streaming APIs to capture the content
// and send it to a Cast receiver end-point controlled by
// CastStreamingApiTestcode.  It generates audio/video test patterns that rotate
// cyclicly, and these test patterns are checked for by an in-process Cast
// receiver to confirm the correct end-to-end functionality of the Cast
// streaming API.
//
// Once everything is set up and fully operational, chrome.test.succeed() is
// invoked as a signal for the end-to-end testing to proceed.  If any step in
// the setup process fails, chrome.test.fail() is invoked.

// The test pattern cycles as a color fill of red, then green, then blue; paired
// with successively higher-frequency tones.
var colors = [ [ 255, 0, 0 ], [ 0, 255, 0 ], [ 0, 0, 255 ] ];
var freqs = [ 200, 500, 1800 ];
var curTestIdx = 0;

function updateTestPattern() {
  if (document.body && document.body.style)  // Check that page is loaded.
    document.body.style.backgroundColor = "rgb(" + colors[curTestIdx] + ")";

  // Important: Blink the testing message so that the capture pipeline will
  // observe drawing updates and continue to produce video frames.
  var message = document.getElementById("message");
  if (message && !message.blinkInterval) {
    message.innerHTML = "Testing...";
    message.blinkInterval = setInterval(
        function toggleVisibility() {
          message.style.visibility =
              message.style.visibility == "hidden" ? "visible" : "hidden";
        },
        125);
  }

  if (!this.audioContext) {
    this.audioContext = new AudioContext();
    this.gainNode = this.audioContext.createGain();
    this.gainNode.gain.value = 0.5;
    this.gainNode.connect(this.audioContext.destination);
  } else {
    this.oscillator.stop();
    this.oscillator.disconnect();
  }
  // Note: We recreate the oscillator each time because this switches the audio
  // frequency immediately.  Re-using the same oscillator tends to take several
  // hundred milliseconds to ramp-up/down the frequency.
  this.oscillator = audioContext.createOscillator();
  this.oscillator.type = OscillatorNode.SINE;
  this.oscillator.frequency.value = freqs[curTestIdx];
  this.oscillator.connect(this.gainNode);
  this.oscillator.start();
}

// Calls updateTestPattern(), then waits and calls itself again to advance to
// the next one.
function runTestPatternLoop() {
  updateTestPattern();
  if (!this.curAdvanceWaitTimeMillis) {
    this.curAdvanceWaitTimeMillis = 750;
  }
  setTimeout(
      function advanceTestPattern() {
        ++curTestIdx;
        if (curTestIdx >= colors.length) {  // Completed a cycle.
          curTestIdx = 0;
          // Increase the wait time between switching test patterns for
          // overloaded bots that aren't capturing all the frames of video.
          this.curAdvanceWaitTimeMillis *= 1.25;
        }
        runTestPatternLoop();
      },
      this.curAdvanceWaitTimeMillis);
}

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

    // Set to true if you want to confirm the sender color/tone changes are
    // working, without starting tab capture and Cast sending.
    if (false) {
      setTimeout(runTestPatternLoop, 0);
      return;
    }

    var width = 400;
    var height = 400;
    var frameRate = 15;

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
                setTimeout(runTestPatternLoop, 0);
                if (window.innerWidth > 2 * width ||
                    window.innerHeight > 2 & height) {
                  console.warn("***TIMEOUT HAZARD***  Tab size is " +
                      window.innerWidth + "x" + window.innerHeight +
                      ", which is much larger than the expected " +
                      width + "x" + height);
                }
                chrome.test.succeed();
              });
        });
  }
]);
