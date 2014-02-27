// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function TestStateMachine(stream, audioId, videoId, udpId) {
  this.stream = stream;
  this.audioId = audioId;
  this.videoId = videoId;
  this.udpId = udpId;
  this.audioStarted = false;
  this.videoStarted = false;
  this.audioStopped = false;
  this.videoStopped = false;
  this.gotAudioRawEvents = false;
  this.gotVideoRawEvents = false;
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

TestStateMachine.prototype.onGotRawEvents = function(id, rawEvents) {
  chrome.test.assertTrue(rawEvents.length > 0);
  if (id == this.audioId) {
    this.gotAudioRawEvents = true;
  }
  if (id == this.videoId) {
    this.gotVideoRawEvents = true;
  }
  if (this.gotAudioRawEvents && this.gotVideoRawEvents)
    this.onGotAllRawEvents();
}

