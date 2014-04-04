/**
 * Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * See http://dev.w3.org/2011/webrtc/editor/getusermedia.html for more
 * information on getUserMedia.
 */

/**
 * Keeps track of our local stream (e.g. what our local webcam is streaming).
 * @private
 */
var gLocalStream = null;

/**
 * The MediaConstraints to use when connecting the local stream with a peer
 * connection.
 * @private
 */
var gAddStreamConstraints = {};

/**
 * String which keeps track of what happened when we requested user media.
 * @private
 */
var gRequestWebcamAndMicrophoneResult = 'not-called-yet';

/**
 * Used as a shortcut. Moved to the top of the page due to race conditions.
 * @param {string} id is a case-sensitive string representing the unique ID of
 *     the element being sought.
 * @return {string} id returns the element object specified as a parameter
 */
$ = function(id) {
  return document.getElementById(id);
};

/**
 * This function asks permission to use the webcam and mic from the browser. It
 * will return ok-requested to the test. This does not mean the request was
 * approved though. The test will then have to click past the dialog that
 * appears in Chrome, which will run either the OK or failed callback as a
 * a result. To see which callback was called, use obtainGetUserMediaResult().
 *
 * @param {string} constraints Defines what to be requested, with mandatory
 *     and optional constraints defined. The contents of this parameter depends
 *     on the WebRTC version. This should be JavaScript code that we eval().
 */
function doGetUserMedia(constraints) {
  if (!getUserMedia) {
    returnToTest('Browser does not support WebRTC.');
    return;
  }
  try {
    var evaluatedConstraints;
    eval('evaluatedConstraints = ' + constraints);
  } catch (exception) {
    throw failTest('Not valid JavaScript expression: ' + constraints);
  }
  debug('Requesting doGetUserMedia: constraints: ' + constraints);
  getUserMedia(evaluatedConstraints,
               function(stream) {
                 ensureGotAllExpectedStreams_(stream, constraints);
                 getUserMediaOkCallback_(stream);
               },
               getUserMediaFailedCallback_);
  returnToTest('ok-requested');
}

/**
 * Must be called after calling doGetUserMedia.
 * @return {string} Returns not-called-yet if we have not yet been called back
 *     by WebRTC. Otherwise it returns either ok-got-stream or
 *     failed-with-error-x (where x is the error code from the error
 *     callback) depending on which callback got called by WebRTC.
 */
function obtainGetUserMediaResult() {
  returnToTest(gRequestWebcamAndMicrophoneResult);
  return gRequestWebcamAndMicrophoneResult;
}

/**
 * Stops all tracks of the last acquired local stream.
 */
function stopLocalStream() {
  if (gLocalStream == null)
    throw failTest('Tried to stop local stream, ' +
                   'but media access is not granted.');

  gLocalStream.getVideoTracks().forEach(function(track) {
    track.stop();
  });
  gLocalStream.getAudioTracks().forEach(function(track) {
    track.stop();
  });
  gLocalStream = null;
  gRequestWebcamAndMicrophoneResult = 'not-called-yet';
  returnToTest('ok-stopped');
}

// Functions callable from other JavaScript modules.

/**
 * Adds the current local media stream to a peer connection.
 * @param {RTCPeerConnection} peerConnection
 */
function addLocalStreamToPeerConnection(peerConnection) {
  if (gLocalStream == null)
    throw failTest('Tried to add local stream to peer connection, ' +
                   'but there is no stream yet.');
  try {
    peerConnection.addStream(gLocalStream, gAddStreamConstraints);
  } catch (exception) {
    throw failTest('Failed to add stream with constraints ' +
                   gAddStreamConstraints + ': ' + exception);
  }
  debug('Added local stream.');
}

/**
 * Removes the local stream from the peer connection.
 * @param {rtcpeerconnection} peerConnection
 */
function removeLocalStreamFromPeerConnection(peerConnection) {
  if (gLocalStream == null)
    throw failTest('Tried to remove local stream from peer connection, ' +
                   'but there is no stream yet.');
  try {
    peerConnection.removeStream(gLocalStream);
  } catch (exception) {
    throw failTest('Could not remove stream: ' + exception);
  }
  debug('Removed local stream.');
}

/**
 * @return {string} Returns the current local stream - |gLocalStream|.
 */
function getLocalStream() {
  return gLocalStream;
}

// Internals.

/**
 * @private
 * @param {MediaStream} stream Media stream from getUserMedia.
 * @param {String} constraints The constraints passed
 */
function ensureGotAllExpectedStreams_(stream, constraints) {
  var requestedVideo = /video\s*:\s*true/i;
  if (requestedVideo.test(constraints) && stream.getVideoTracks().length == 0) {
    gRequestWebcamAndMicrophoneResult = 'failed-to-get-video';
    throw ('Requested video, but did not receive a video stream from ' +
           'getUserMedia. Perhaps the machine you are running on ' +
           'does not have a webcam.');
  }
  var requestedAudio = /audio\s*:\s*true/i;
  if (requestedAudio.test(constraints) && stream.getAudioTracks().length == 0) {
    gRequestWebcamAndMicrophoneResult = 'failed-to-get-audio';
    throw ('Requested audio, but did not receive an audio stream ' +
           'from getUserMedia. Perhaps the machine you are running ' +
           'on does not have audio devices.');
  }
}

/**
 * @private
 * @param {MediaStream} stream Media stream.
 */
function getUserMediaOkCallback_(stream) {
  gLocalStream = stream;
  gRequestWebcamAndMicrophoneResult = 'ok-got-stream';

  attachMediaStream($('local-view'), stream);
}

/**
 * Enumerates the audio and video devices available in Chrome and adds the
 * devices to the HTML elements with Id 'audiosrc' and 'videosrc'.
 * Checks if device enumeration is supported and if the 'audiosrc' + 'videosrc'
 * elements exists, if not a debug printout will be displayed.
 * If the device label is empty, audio/video + sequence number will be used to
 * populate the name. Also makes sure the children has been loaded in order
 * to update the constraints.
 */
function getDevices() {
  if ($('audiosrc') && $('videosrc') && $('get-devices')) {
    var audio_select = $('audiosrc');
    var video_select = $('videosrc');
    var get_devices = $('get-devices');
    audio_select.innerHTML = '';
    video_select.innerHTML = '';
    try {
      eval(MediaStreamTrack.getSources(function() {}));
    } catch (exception) {
      audio_select.disabled = true;
      video_select.disabled = true;
      get_devices.disabled = true;
      updateGetUserMediaConstraints();
      debug('Device enumeration not supported. ' + exception);
      return;
    }
    MediaStreamTrack.getSources(function(devices) {
      for (var i = 0; i < devices.length; i++) {
        var option = document.createElement('option');
        option.value = devices[i].id;
        option.text = devices[i].label;
        if (devices[i].kind == 'audio') {
          if (option.text == '') {
            option.text = devices[i].id;
          }
          audio_select.appendChild(option);
        }
        else if (devices[i].kind == 'video') {
          if (option.text == '') {
            option.text = devices[i].id;
          }
          video_select.appendChild(option);
        }
        else {
          debug('Device type ' + devices[i].kind + ' not recognized, cannot ' +
                'enumerate device. Currently only device types \'audio\' and ' +
                '\'video\' are supported');
          updateGetUserMediaConstraints();
          return;
        }
      }
    });
    checkIfDeviceDropdownsArePopulated();
  }
  else {
    debug('Device DOM elements cannot be found, cannot display devices');
    updateGetUserMediaConstraints();
  }
}

/**
 * This provides the selected source id from the objects in the parameters
 * provided to this function. If the audio_select or video_select objects does
 * not have any HTMLOptions children it will return null in the source object.
 * @param {object} audio_select HTML drop down element with audio devices added
 *     as HTMLOptionsCollection children.
 * @param {object} video_select HTML drop down element with audio devices added
 *     as HTMLPptionsCollection children.
 * @return {object} audio_id video_id Containing audio and video source ID from
 *     the selected devices in the drop down menus provided as parameters to
 *     this function.
 */
function getSourcesFromField(audio_select, video_select) {
  var source = {
    audio_id: null,
    video_id: null
  };
  if (audio_select.options.length > 0) {
    source.audio_id = audio_select.options[audio_select.selectedIndex].value;
  }
  if (video_select.options.length > 0) {
    source.video_id = video_select.options[video_select.selectedIndex].value;
  }
  return source;
}

/**
 * @private
 * @param {NavigatorUserMediaError} error Error containing details.
 */
function getUserMediaFailedCallback_(error) {
  // Translate from the old error to the new. Remove when rename fully deployed.
  var errorName = error.name;

  debug('GetUserMedia FAILED: Maybe the camera is in use by another process?');
  gRequestWebcamAndMicrophoneResult = 'failed-with-error-' + errorName;
  debug(gRequestWebcamAndMicrophoneResult);
}
