// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var rtpStream = chrome.cast.streaming.rtpStream;
var tabCapture = chrome.tabCapture;
var udpTransport = chrome.cast.streaming.udpTransport;
var createSession = chrome.cast.streaming.session.create;

chrome.test.runTests([
  function rtpStreamStart() {
    tabCapture.capture({audio: true, video: true}, function(stream) {
      console.log("Got MediaStream.");
      chrome.test.assertTrue(!!stream);
      createSession(stream.getAudioTracks()[0],
                    stream.getVideoTracks()[0],
            function(stream, audioId, videoId, udpId) {
        console.log("Starting.");
        var audioParams = rtpStream.getCaps(audioId);
        var videoParams = rtpStream.getCaps(videoId);
        rtpStream.start(audioId, audioParams);
        rtpStream.start(videoId, videoParams);
        window.setTimeout(function() {
          console.log("Stopping.");
          rtpStream.stop(audioId);
          rtpStream.stop(videoId);
          rtpStream.destroy(audioId);
          rtpStream.destroy(videoId);
          udpTransport.destroy(udpId);
          stream.stop();
          chrome.test.assertEq(audioParams.payloads[0].codecName, "OPUS");
          chrome.test.assertEq(videoParams.payloads[0].codecName, "VP8");
          chrome.test.succeed();
        }, 0);
      }.bind(null, stream));
    });
  },
]);
