// Copyright (c) 2013 The Chromium Authors. All rights reserved.
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

function endToEndVideoTest() {
  chrome.tabCapture.capture(
    { video: true, audio: false,
      videoConstraints: {
        mandatory: {
          minWidth: 64,
          minHeight: 48
        }
      }
    },
    function onTabCaptureSuccess(stream) {
      chrome.test.assertTrue(stream != null);

      // The test source is a color fill of red, then green, then blue.
      var colors = [ [ 255, 0, 0 ], [ 0, 255, 0 ], [ 0, 0, 255 ] ];
      var curColorIdx = 0;

      // Create video and canvas elements, but no need to append them to the
      // DOM.
      var video = document.createElement("video");
      video.width = 64;
      video.height = 48;
      video.addEventListener("error", chrome.test.fail);
      var canvas = document.createElement("canvas");

      function updateFillColor() {
        document.body.style.backgroundColor =
            "rgb(" + colors[curColorIdx] + ")";
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
            updateFillColor();
          } else {
            video.removeEventListener("timeupdate", checkVideoForFillColor);
            chrome.test.succeed();
          }
        }
      }

      // Play the LocalMediaStream in the video element.
      video.src = webkitURL.createObjectURL(stream);
      video.play();

      // Kick it off.
      updateFillColor();
      video.addEventListener("timeupdate", checkVideoForFillColor);
    }
  );
}

chrome.test.runTests([ endToEndVideoTest ]);

// TODO(miu): Once the WebAudio API is finalized, we should add a test to emit a
// tone from the sender page, and have the receiver page check for the audio
// tone.
