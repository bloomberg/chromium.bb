/**
 * Copyright (c) 2013 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

var gContext = null;

function loadAudioAndAddToPeerConnection(url, peerconnection) {
  if (gContext == null)
    gContext = new webkitAudioContext();

  var inputSink = gContext.createMediaStreamDestination();
  peerconnection.addStream(inputSink.stream);

  loadAudioBuffer_(url, function(voiceSoundBuffer) {
    var bufferSource = gContext.createBufferSource();
    bufferSource.buffer = voiceSoundBuffer;
    bufferSource.connect(inputSink);
    bufferSource.start(gContext.currentTime);
  });
}

/** @private */
function loadAudioBuffer_(url, callback) {
  debug('loadAudioBuffer()');

  var request = new XMLHttpRequest();
  request.open('GET', url, true);
  request.responseType = 'arraybuffer';

  request.onload = function() {
    voiceSoundBuffer = gContext.createBuffer(request.response, false);
    callback(voiceSoundBuffer);
  }

  request.send();
}
