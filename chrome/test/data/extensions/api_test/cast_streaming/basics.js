// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var rtpStream = chrome.cast.streaming.rtpStream;
var tabCapture = chrome.tabCapture;
var udpTransport = chrome.cast.streaming.udpTransport;
var createSession = chrome.cast.streaming.session.create;
var pass = chrome.test.callbackPass;

chrome.test.runTests([
  function rtpStreamStart() {
    console.log("[TEST] rtpStreamStart");
    tabCapture.capture({audio: true, video: true},
                       pass(function(stream) {
      console.log("Got MediaStream.");
      chrome.test.assertTrue(!!stream);
      createSession(stream.getAudioTracks()[0],
                    stream.getVideoTracks()[0],
                    pass(function(stream, audioId, videoId, udpId) {
        console.log("Starting.");
        var audioParams = rtpStream.getCaps(audioId);
        var videoParams = rtpStream.getCaps(videoId);
        rtpStream.start(audioId, audioParams);
        rtpStream.start(videoId, videoParams);
        udpTransport.start(udpId, {address: "127.0.0.1", port: 2344});
        window.setTimeout(pass(function() {
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
        }), 0);
      }.bind(null, stream)));
    }));
  },
  function invalidKey() {
    console.log("[TEST] invalidKey");
    tabCapture.capture({audio: true, video: true},
                       pass(function(stream) {
      chrome.test.assertTrue(!!stream);
      createSession(stream.getAudioTracks()[0],
                    stream.getVideoTracks()[0],
                    pass(function(stream, audioId, videoId, udpId) {
        // AES key is invalid and exception is expected.
        try {
          var audioParams = rtpStream.getCaps(audioId);
          var videoParams = rtpStream.getCaps(videoId);
          audioParams.payloads[0].aesKey = "google";
          videoParams.payloads[0].aesIvMask = "chrome";
          rtpStream.start(audioId, audioParams);
          rtpStream.start(videoId, videoParams);
        } catch (e) {
          rtpStream.destroy(audioId);
          rtpStream.destroy(videoId);
          udpTransport.destroy(udpId);
          stream.stop();
          chrome.test.succeed();
        }
      }.bind(null, stream)));
    }));
  },
]);
