// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var rtpStream = chrome.cast.streaming.rtpStream;
var tabCapture = chrome.tabCapture;
var udpTransport = chrome.cast.streaming.udpTransport;
var createSession = chrome.cast.streaming.session.create;
var pass = chrome.test.callbackPass;

function TestStateMachine(stream, audioId, videoId, udpId) {
  this.stream = stream;
  this.audioId = audioId;
  this.videoId = videoId;
  this.udpId = udpId;
  this.audioStarted = false;
  this.videoStarted = false;
  this.audioStopped = false;
  this.videoStopped = false;
}

TestStateMachine.prototype.onStarted = function(id) {
  if (id == this.audioId)
    this.audioStarted = true;
  if (id == this.videoId)
    this.videoStarted = true;
  if (this.audioStarted && this.videoStarted)
    this.onAllStarted();
}

TestStateMachine.prototype.onStopped = function(id) {
  if (id == this.audioId)
    this.audioStopped = true;
  if (id == this.videoId)
    this.videoStopped = true;
  if (this.audioStopped && this.videoStopped)
    this.onAllStopped();
}

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
        var stateMachine = new TestStateMachine(stream,
                                                audioId,
                                                videoId,
                                                udpId);
        var audioParams = rtpStream.getSupportedParams(audioId)[0];
        var videoParams = rtpStream.getSupportedParams(videoId)[0];
        chrome.test.assertEq(audioParams.payload.codecName, "OPUS");
        chrome.test.assertEq(videoParams.payload.codecName, "VP8");
        udpTransport.setDestination(udpId,
                                    {address: "127.0.0.1", port: 2344});
        rtpStream.onStarted.addListener(
            stateMachine.onStarted.bind(stateMachine));
        stateMachine.onAllStarted =
            pass(function(audioId, videoId) {
          console.log("Stopping.");
          rtpStream.stop(audioId);
          rtpStream.stop(videoId);
        }.bind(null, audioId, videoId));
        rtpStream.onStopped.addListener(
            stateMachine.onStopped.bind(stateMachine));
        stateMachine.onAllStopped =
            pass(function(stream, audioId, videoId, udpId) {
          console.log("Destroying.");
          rtpStream.destroy(audioId);
          rtpStream.destroy(videoId);
          udpTransport.destroy(udpId);
          chrome.test.assertEq(audioParams.payload.codecName, "OPUS");
          chrome.test.assertEq(videoParams.payload.codecName, "VP8");
          chrome.test.succeed();
        }.bind(null, stream, audioId, videoId, udpId));
        rtpStream.start(audioId, audioParams);
        rtpStream.start(videoId, videoParams);
      }.bind(null, stream)));
    }));
  },
]);
