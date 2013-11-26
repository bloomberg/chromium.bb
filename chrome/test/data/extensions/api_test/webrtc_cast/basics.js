// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var sendTransport = chrome.webrtc.castSendTransport;
var tabCapture = chrome.tabCapture;
var udpTransport = chrome.webrtc.castUdpTransport;

chrome.test.runTests([
  function udpTransportCreate() {
    udpTransport.create(function(udpId) {
      udpTransport.start(
          udpId,
          {address: "127.0.0.1", port: 60613});
      udpTransport.destroy(udpId);
      chrome.test.succeed();
    });
  },
  function sendTransportCreate() {
    tabCapture.capture({audio: true}, function(stream) {
      chrome.test.assertTrue(!!stream);
      udpTransport.create(function(stream, udpId) {
        sendTransport.create(
            udpId,
            stream.getAudioTracks()[0],
            function(stream, udpId, sendTransportId) {
          sendTransport.destroy(sendTransportId);
          udpTransport.destroy(udpId);
          stream.stop();
          chrome.test.succeed();
        }.bind(null, stream, udpId));
      }.bind(null, stream));
    });
  },
  function sendTransportGetCapsAudio() {
    tabCapture.capture({audio: true}, function(stream) {
      chrome.test.assertTrue(!!stream);
      udpTransport.create(function(stream, udpId) {
        sendTransport.create(
            udpId,
            stream.getAudioTracks()[0],
            function(stream, udpId, sendTransportId) {
          var caps = sendTransport.getCaps(sendTransportId);
          sendTransport.destroy(sendTransportId);
          udpTransport.destroy(udpId);
          stream.stop();
          chrome.test.assertEq(caps.payloads[0].codecName, "OPUS");
          chrome.test.succeed();
        }.bind(null, stream, udpId));
      }.bind(null, stream));
    });
  },
  function sendTransportStart() {
    function startId(id) {
      sendTransport.start(id,sendTransport.getCaps(id));
    }
    function cleanupId(id) {
      sendTransport.stop(id);
      sendTransport.destroy(id);
    }

    console.log("Caputre MediaStream...");
    tabCapture.capture({audio: true, video: true}, function(stream) {
      console.log("Got MediaStream.");

      chrome.test.assertTrue(!!stream);
      udpTransport.create(function(stream, udpId) {
        console.log("Got udp transport.");

        // Create a transport for audio.
        sendTransport.create(
            udpId,
            stream.getAudioTracks()[0],
            function(stream, udpId, audioId) {
          console.log("Got audio stream.");

          // Create a transport for video.
          sendTransport.create(
              udpId,
              stream.getVideoTracks()[0],
              function(stream, udpId, audioId, videoId) {
            console.log("Got video stream.");
            startId(audioId);
            cleanupId(audioId);
            startId(videoId);
            cleanupId(videoId);
            udpTransport.destroy(udpId);
            stream.stop();
            chrome.test.succeed();
          }.bind(null, stream, udpId, audioId));
        }.bind(null, stream, udpId));
      }.bind(null, stream));
    });
  },
]);
