/**
 * Copyright 2017 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * Aggregate target bits per second for encoding of the Audio track.
 * @private
 */
const AUDIO_BITS_PER_SECOND = 3000000;

var doneCapturing = false;

/**
 * Starts the audio capturing.
 *
 * @param {Number} The duration of the capture in seconds.
 */
function startAudioCapture(capture_duration_in_seconds, output_file) {
  console.log('Started capturing for ' + capture_duration_in_seconds + 's to '
      + output_file);
  var inputElement = document.getElementById('remote-view');

  // |audioBitsPerSecond| must set to a large number to throw as little
  // information away as possible.
  var mediaRecorderOptions = {
    audioBitsPerSecond: AUDIO_BITS_PER_SECOND,
    mimeType: 'audio/webm',
  };
  var stream = inputElement.srcObject;
  var mediaRecorder = new MediaRecorder(stream, mediaRecorderOptions);

  var audio_chunks = [];

  mediaRecorder.ondataavailable = function(recording) {
    audio_chunks.push(recording.data);
  }
  mediaRecorder.onstop = function() {
    var audioBlob = new Blob(audio_chunks, {type: 'audio/webm'});

    var url = window.URL.createObjectURL(audioBlob);
    var a = document.createElement('a');
    document.body.appendChild(a);

    a.href = url;
    a.download = output_file;
    a.click();

    doneCapturing = true;
  }

  mediaRecorder.start();
  setTimeout(function() { mediaRecorder.stop(); },
             capture_duration_in_seconds * 1000);

  returnToTest('ok-capturing');
}

function testIsDoneCapturing() {
  returnToTest(doneCapturing.toString());
}
