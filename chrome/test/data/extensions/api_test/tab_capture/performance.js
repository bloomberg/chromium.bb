// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The tests here cover the end-to-end functionality of tab capturing and
// playback as video.  The page generates a test patter (moving balls) and
// the rendering output of the tab is captured into a LocalMediaStream.  Then,
// the LocalMediaStream is plugged into a video element for playback.
//

// Global to prevent gc from eating the video tag.
var video = null;

function TestStream(stream) {
  // Create video and canvas elements, but no need to append them to the
  // DOM.
  video = document.createElement("video");
  video.width = 1920;
  video.height = 1080;
  video.addEventListener("error", chrome.test.fail);

  var canvas = document.createElement("canvas");
  canvas.width = video.width;
  canvas.height = video.height;
  var context = canvas.getContext("2d");
  document.body.appendChild(canvas);
  var start_time = new Date().getTime();

  // Play the LocalMediaStream in the video element.
  video.src = URL.createObjectURL(stream);
  video.play();

  var frame = 0;
  function draw() {
    // Run for 10 seconds.
    if (new Date().getTime() - start_time > 10000) {
      chrome.test.succeed();
      // Note that the API testing framework might not terminate if we keep
      // animating and capturing, so we have to make sure that we stop doing
      // that here.
      stream.stop();
      return;
    }
    requestAnimationFrame(draw);
    frame = frame + 1;
    context.fillStyle = 'rgb(255,255,255)';
    context.fillRect( 0, 0, canvas.width, canvas.height );
    for (var j = 0; j < 200; j++) {
      var i = (j + frame) % 200;
      var t = (frame + 3000) * (0.01 + i / 8000.0);
      var x = (Math.sin( t ) * 0.45 + 0.5) * canvas.width;
      var y = (Math.cos( t * 0.9 )  * 0.45 + 0.5) * canvas.height;
      context.fillStyle = 'rgb(' + (255 - i) + ',' + (155 +i) + ', ' + i + ')';
      context.beginPath();
      context.arc(x, y, 50, 0, Math.PI * 2, true);
      context.closePath();
      context.fill();
    }
  }

  // Kick it off.
  draw();
}

// Set up a WebRTC connection and pipe |stream| through it.
function testThroughWebRTC(stream) {
  console.log("Testing through webrtc.");
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

function tabCapturePerformanceTest() {
  var f = TestStream;
  if (parseInt(window.location.href.split('?WebRTC=')[1])) {
    f = testThroughWebRTC;
  }
  var fps = parseInt(window.location.href.split('&fps=')[1]);
  chrome.tabCapture.capture(
    { video: true, audio: false,
      videoConstraints: {
        mandatory: {
          minWidth: 1920,
          minHeight: 1080,
          maxWidth: 1920,
          maxHeight: 1080,
          maxFrameRate: fps,
        }
      }
    },
    f);
}

chrome.test.runTests([ tabCapturePerformanceTest ]);

// TODO(hubbe): Consider capturing audio as well, need to figure out how to
// capture relevant statistics for that though.
