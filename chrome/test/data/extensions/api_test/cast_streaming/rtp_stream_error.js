// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var rtpStream = chrome.cast.streaming.rtpStream;
var tabCapture = chrome.tabCapture;
var udpTransport = chrome.cast.streaming.udpTransport;
var createSession = chrome.cast.streaming.session.create;
var pass = chrome.test.callbackPass;

chrome.test.runTests([
  function rtpStreamError() {
    console.log("[TEST] rtpStreamError");
    var constraints = {
      video: true,
      audio: true,
      videoConstraints: {
        mandatory: {
          minWidth: 640,
          minHeight: 480,
          maxWidth: 640,
          maxHeight: 480,
        }
      }
    };
    tabCapture.capture(constraints,
                       pass(function(stream) {
      chrome.test.assertTrue(!!stream);
      createSession(stream.getAudioTracks()[0],
                    stream.getVideoTracks()[0],
                    pass(function(stream, audioId, videoId, udpId) {
        var audioParams = rtpStream.getSupportedParams(audioId)[0];
        var videoParams = rtpStream.getSupportedParams(videoId)[0];
        rtpStream.onError.addListener(
            pass(function(audioId, videoId, id, msg) {
          chrome.test.assertEq(videoId, id);
          rtpStream.destroy(audioId);
          rtpStream.destroy(videoId);
          udpTransport.destroy(udpId);
          console.log(msg);
        }.bind(null, audioId, videoId)));
        // Invalid width and height.
        videoParams.payload.width = 100;
        videoParams.payload.height = 100;
        udpTransport.setDestination(udpId,
                                    {address: "127.0.0.1", port: 2344});
        rtpStream.start(videoId, videoParams);
      }.bind(null, stream)));
    }));
  },
]);
