// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The tests here cover the end-to-end functionality of tab capturing and
// playback as video.  The page generates a test signal (a full color fill), and
// the rendering output of the tab is captured into a LocalMediaStream.  Then,
// the LocalMediaStream is plugged into a video element for playback, and a
// canvas is used to examine the frames of the video for expected content.
//
// A previous version of this test used a polling scheme and two separate tabs
// with very little control logic.  This setup resulted in flakiness, as there
// were numerous issues that could cause the test to time out.  This new version
// uses an entirely event-based scheme, which ensures everything is synchronized
// as the test advances through its stages.


// Video needs to be global, or the big bad garbage collector will come and
// huff and puff it all away.
var video = null;

function TestStream(stream) {
  chrome.test.assertTrue(stream != null);

  // The test source is a color fill of red, then green, then blue.
  var colors = [ [ 255, 0, 0 ], [ 0, 255, 0 ], [ 0, 0, 255 ] ];
  var curColorIdx = 0;

  // Create video and canvas elements, but no need to append them to the
  // DOM.
  video = document.createElement("video");
  video.width = 64;
  video.height = 48;
  video.addEventListener("error", chrome.test.fail);
  var canvas = document.createElement("canvas");

  function updateTestDocument() {
    document.body.style.backgroundColor =
        "rgb(" + colors[curColorIdx] + ")";

    // Important: Blink the testing message so that the capture pipeline
    // will observe drawing updates and continue to produce video frames.
    var message = document.getElementById("message");
    if (!message.blinkInterval) {
      message.innerHTML = "Testing...";
      message.blinkInterval = setInterval(function toggleVisibility() {
        message.style.visibility =
            message.style.visibility == "hidden" ? "visible" : "hidden";
      }, 500);
    }
  }

  function checkVideoForFillColor(event) {
    var curColor = colors[curColorIdx];
    var width = video.videoWidth;
    var height = video.videoHeight;

    // Grab a snapshot of the center pixel of the video.
    canvas.width = width;
    canvas.height = height;
    var ctx = canvas.getContext("2d");
    ctx.drawImage(video, 0, 0, width, height);
    var imageData = ctx.getImageData(width / 2, height / 2, 1, 1);
    var pixel = [ imageData.data[0], imageData.data[1], imageData.data[2] ];

    // Check whether the pixel is of the expected color value, and proceed
    // to the next test stage when a match is encountered.  Note: The video
    // encode/decode process loses color accuracy, which is accounted for
    // here.
    if (Math.abs(pixel[0] - curColor[0]) < 10 &&
        Math.abs(pixel[1] - curColor[1]) < 10 &&
        Math.abs(pixel[2] - curColor[2]) < 10) {
      console.debug("Observed expected color RGB(" + curColor +
          ") in the video as RGB(" + pixel + ")");
      // Continue with the next color; or, if there are no more colors,
      // consider the test successful.
      if (curColorIdx + 1 < colors.length) {
        ++curColorIdx;
        updateTestDocument();
      } else {
        video.removeEventListener("timeupdate", checkVideoForFillColor);
        stream.stop();
        chrome.test.succeed();
      }
    }
  }
  // Play the LocalMediaStream in the video element.
  video.src = URL.createObjectURL(stream);
  video.play();

  // Kick it off.
  updateTestDocument();
  video.addEventListener("timeupdate", checkVideoForFillColor);
}

// Set up a WebRTC connection and pipe |stream| through it.
// Call TestStream with the new stream when done.
function testThroughWebRTC(stream) {
  var sender = new webkitRTCPeerConnection(null);
  var receiver = new webkitRTCPeerConnection(null);
  sender.onicecandidate = function (event) {
    if (event.candidate) {
      receiver.addIceCandidate(new RTCIceCandidate(event.candidate));
    }
  };
  receiver.onicecandidate = function (event) {
    if (event.candidate) {
      sender.addIceCandidate(new RTCIceCandidate(event.candidate));
    }
  };
  receiver.onaddstream = function (event) {
    TestStream(event.stream);
  };
  sender.addStream(stream);
  sender.createOffer(function (sender_description) {
    sender.setLocalDescription(sender_description);
    receiver.setRemoteDescription(sender_description);
    receiver.createAnswer(function (receiver_description) {
      receiver.setLocalDescription(receiver_description);
      sender.setRemoteDescription(receiver_description);
    });
  });
}

function endToEndVideoTest() {
  chrome.tabCapture.capture(
    { video: true,
      audio: false,
      videoConstraints: {
        mandatory: {
          minWidth: 64,
          minHeight: 48
        }
      }
    },
    TestStream);
}

function endToEndVideoTestWithWebRTC() {
  chrome.tabCapture.capture(
    { video: true,
      audio: false,
      videoConstraints: {
        mandatory: {
          minWidth: 64,
          minHeight: 48
        }
      }
    },
    testThroughWebRTC);
}

chrome.test.runTests([
  endToEndVideoTest,
  endToEndVideoTestWithWebRTC
]);

// TODO(miu): Once the WebAudio API is finalized, we should add a test to emit a
// tone from the sender page, and have the receiver page check for the audio
// tone.
