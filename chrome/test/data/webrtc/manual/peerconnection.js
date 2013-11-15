/**
 * Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * Used as a shortcut. Moved to the top of the page due to race conditions.
 * @param {string} id is a case-sensitive string representing the unique ID of
 *     the element being sought.
 * @return {object} id returns the element object specified as a parameter
 */
$ = function(id) {
  return document.getElementById(id);
};

/**
 * Sets the global variable gUseRtpDataChannel in message_handling.js
 * based on the check box element 'data-channel-type-rtp' checked status on
 * peerconnection.html. Also prints a help text to inform the user.
 */
function setPcDataChannelType() {
  var useRtpDataChannels = $('data-channel-type-rtp').checked;
  useRtpDataChannelsForNewPeerConnections(useRtpDataChannels);
  if (useRtpDataChannels) {
    debug('Make sure to tick the RTP check box on both the calling and ' +
          'answering side to ensure a data channel can be setup properly as ' +
          'it has to use the same transport protocol (RTP to RTP and SCTP to ' +
          'SCTP).');
  }
}

// The *Here functions are called from peerconnection.html and will make calls
// into our underlying JavaScript library with the values from the page
// (have to be named differently to avoid name clashes with existing functions).

function getUserMediaFromHere() {
  var constraints = $('getusermedia-constraints').value;
  try {
    doGetUserMedia(constraints);
  } catch (exception) {
    print_('getUserMedia says: ' + exception);
  }
}

function connectFromHere() {
  var server = $('server').value;
  if ($('peer-id').value == '') {
    // Generate a random name to distinguish us from other tabs:
    $('peer-id').value = 'peer_' + Math.floor(Math.random() * 10000);
    debug('Our name from now on will be ' + $('peer-id').value);
  }
  connect(server, $('peer-id').value);
}

function negotiateCallFromHere() {
  // Set the global variables used in jsep01_call.js with values from our UI.
  setCreateOfferConstraints(getEvaluatedJavaScript_(
      $('pc-createoffer-constraints').value));
  setCreateAnswerConstraints(getEvaluatedJavaScript_(
      $('pc-createanswer-constraints').value));

  ensureHasPeerConnection_();
  negotiateCall();
}

function addLocalStreamFromHere() {
  ensureHasPeerConnection_();
  addLocalStream();
}

function removeLocalStreamFromHere() {
  removeLocalStream();
}

function hangUpFromHere() {
  hangUp();
  acceptIncomingCallsAgain();
}

function toggleRemoteVideoFromHere() {
  toggleRemoteStream(function(remoteStream) {
    return remoteStream.getVideoTracks()[0];
  }, 'video');
}

function toggleRemoteAudioFromHere() {
  toggleRemoteStream(function(remoteStream) {
    return remoteStream.getAudioTracks()[0];
  }, 'audio');
}

function toggleLocalVideoFromHere() {
  toggleLocalStream(function(localStream) {
    return localStream.getVideoTracks()[0];
  }, 'video');
}

function toggleLocalAudioFromHere() {
  toggleLocalStream(function(localStream) {
    return localStream.getAudioTracks()[0];
  }, 'audio');
}

function stopLocalFromHere() {
  stopLocalStream();
}

function createDataChannelFromHere() {
  ensureHasPeerConnection_();
  createDataChannelOnPeerConnection();
}

function closeDataChannelFromHere() {
  ensureHasPeerConnection_();
  closeDataChannelOnPeerConnection();
}

function sendDataFromHere() {
  var data = $('data-channel-send').value;
  sendDataOnChannel(data);
}

function createDtmfSenderFromHere() {
  ensureHasPeerConnection_();
  createDtmfSenderOnPeerConnection();
}

function insertDtmfFromHere() {
  var tones = $('dtmf-tones').value;
  var duration = $('dtmf-tones-duration').value;
  var gap = $('dtmf-tones-gap').value;
  insertDtmfOnSender(tones, duration, gap);
}

function forceIsacChanged() {
  var forceIsac = $('force-isac').checked;
  if (forceIsac) {
    forceIsac_();
  } else {
    dontTouchSdp_();
  }
}

/**
 * Updates the constraints in the getusermedia-constraints text box with a
 * MediaStreamConstraints string. This string is created based on the status of
 * the checkboxes for audio and video. If device enumeration is supported and
 * device source id's are not null they will be added to the constraints string.
 * Fetches the screen size using "screen" in Chrome as we need to pass a max
 * resolution else it defaults to 640x480 in the constraints for screen
 * capturing.
 */
function updateGetUserMediaConstraints() {
  var audio_selected = $('audiosrc');
  var video_selected = $('videosrc');
  var constraints = {
    audio: $('audio').checked,
    video: $('video').checked
  };
  if (audio_selected.disabled == false && video_selected.disabled == false) {
    var devices = getSourcesFromField(audio_selected, video_selected);
    if ($('audio').checked == true) {
      if (devices.audio_id != null) {
        constraints.audio = {optional: [{sourceId: devices.audio_id}]};
      }
      else {
        constraints.audio = true;
      }
    }
    if ($('video').checked == true) {
      if (devices.video_id != null) {
        constraints.video = {optional: [{sourceId: devices.video_id},
                                        {minWidth: $('video-width').value},
                                        {minHeight: $('video-height').value}]
        };
      }
      else {
        constraints.video = {optional: [{minWidth: $('video-width').value},
                                        {minHeight: $('video-height').value}]
        };
      }
    }
  }
  if ($('screencapture').checked) {
    var constraints = {
      audio: $('audio').checked,
      video: {mandatory: {chromeMediaSource: 'screen',
                          maxWidth: screen.width,
                          maxHeight: screen.height}}
    };
    if ($('audio').checked == true)
      debug('Audio for screencapture is not implemented yet, please ' +
            'try to set audio = false prior requesting screencapture');
  }
  $('getusermedia-constraints').value =
      JSON.stringify(constraints, null, ' ');
}

function showServerHelp() {
  alert('You need to build and run a peerconnection_server on some ' +
        'suitable machine. To build it in chrome, just run make/ninja ' +
        'peerconnection_server. Otherwise, read in https://code.google' +
        '.com/searchframe#xSWYf0NTG_Q/trunk/peerconnection/README&q=REA' +
        'DME%20package:webrtc%5C.googlecode%5C.com.');
}

function toggleHelp() {
  var help = $('help');
  if (help.style.display == 'none')
    help.style.display = 'inline';
  else
    help.style.display = 'none';
}

function clearLog() {
  $('messages').innerHTML = '';
  $('debug').innerHTML = '';
}

/**
 * Prepopulate constraints from JS to the UI and setup callbacks in the scripts
 * shared with browser tests or automated tests. Enumerates devices available
 * via getUserMedia.
 */
window.onload = function() {
  $('pc-createoffer-constraints').value = JSON.stringify(
      gCreateOfferConstraints, null, ' ');
  $('pc-createanswer-constraints').value = JSON.stringify(
    gCreateAnswerConstraints, null, ' ');
  replaceReturnCallback(print_);
  replaceDebugCallback(debug_);
  doNotAutoAddLocalStreamWhenCalled();
  hookupDataChannelCallbacks_();
  hookupDtmfSenderCallback_();
  displayVideoSize_($('local-view'));
  displayVideoSize_($('remote-view'));
  getDevices();
  setPcDataChannelType();
};

/**
 * Checks if the 'audiosrc' and 'videosrc' drop down menu elements has had all
 * of its children appended in order to provide device ID's to the function
 * 'updateGetUserMediaConstraints()', used in turn to populate the getUserMedia
 * constraints text box when the page has loaded. If not the user is informed
 * and instructions on how to populate the field is provided.
 */
function checkIfDeviceDropdownsArePopulated() {
  if (document.addEventListener) {
    $('audiosrc').addEventListener('DOMNodeInserted',
         updateGetUserMediaConstraints, false);
    $('videosrc').addEventListener('DOMNodeInserted',
         updateGetUserMediaConstraints, false);
  }
  else {
    debug('addEventListener is not supported by your browser, cannot update ' +
          'device source ID\'s automatically. Select a device from the audio ' +
          'or video source drop down menu to update device source id\'s');
  }
}

/**
 * Disconnect before the tab is closed.
 */
window.onbeforeunload = function() {
  if (!isDisconnected())
    disconnect();
};

// Internals.

/**
 * Create the peer connection if none is up (this is just convenience to
 * avoid having a separate button for that).
 * @private
 */
function ensureHasPeerConnection_() {
  if (getReadyState() == 'no-peer-connection') {
    preparePeerConnection();
  }
}

/**
 * @private
 * @param {string} message Text to print.
 */
function print_(message) {
  // Filter out uninteresting noise.
  if (message == 'ok-no-errors')
    return;

  console.log(message);
  $('messages').innerHTML += message + '<br>';
}

/**
 * @private
 * @param {string} message Text to print.
 */
function debug_(message) {
  console.log(message);
  $('debug').innerHTML += message + '<br>';
}

/**
 * @private
 * @param {string} stringRepresentation JavaScript as a string.
 * @return {Object} The PeerConnection constraints as a JavaScript dictionary.
 */
function getEvaluatedJavaScript_(stringRepresentation) {
  try {
    var evaluatedJavaScript;
    eval('evaluatedJavaScript = ' + stringRepresentation);
  } catch (exception) {
    throw failTest('Not valid JavaScript expression: ' + stringRepresentation);
  }
  return evaluatedJavaScript;
}

/**
 * Swaps lines within a SDP message.
 * @private
 * @param {string} sdp The full SDP message.
 * @param {string} line The line to swap with swapWith.
 * @param {string} swapWith The other line.
 * @return {string} The altered SDP message.
 */
function swapSdpLines_(sdp, line, swapWith) {
  var lines = sdp.split('\r\n');
  var lineStart = lines.indexOf(line);
  var swapLineStart = lines.indexOf(swapWith);
  if (lineStart == -1 || swapLineStart == -1)
    return sdp;  // This generally happens on the first message.

  var tmp = lines[lineStart];
  lines[lineStart] = lines[swapLineStart];
  lines[swapLineStart] = tmp;

  return lines.join('\r\n');
}

// TODO(phoglund): Not currently used; delete once it clear we do not need to
// test opus prioritization.
/** @private */
function preferOpus_() {
  setOutgoingSdpTransform(function(sdp) {
    sdp = sdp.replace('103 104 111', '111 103 104');

    // TODO(phoglund): We need to swap the a= lines too. I don't think this
    // should be needed but it apparently is right now.
    return swapSdpLines_(sdp,
                         'a=rtpmap:103 ISAC/16000',
                         'a=rtpmap:111 opus/48000');
  });
}

/** @private */
function forceIsac_() {
  setOutgoingSdpTransform(function(sdp) {
    // Remove all other codecs (not the video codecs though).
    sdp = sdp.replace(/m=audio (\d+) RTP\/SAVPF.*\r\n/g,
                      'm=audio $1 RTP/SAVPF 104\r\n');
    sdp = sdp.replace('a=fmtp:111 minptime=10', 'a=fmtp:104 minptime=10');
    sdp = sdp.replace(/a=rtpmap:(?!104)\d{1,3} (?!VP8|red|ulpfec).*\r\n/g, '');
    return sdp;
  });
}

/** @private */
function dontTouchSdp_() {
  setOutgoingSdpTransform(function(sdp) { return sdp; });
}

/** @private */
function hookupDataChannelCallbacks_() {
  setDataCallbacks(function(status) {
    $('data-channel-status').value = status;
  },
  function(data_message) {
    debug('Received ' + data_message.data);
    $('data-channel-receive').value =
      data_message.data + '\n' + $('data-channel-receive').value;
  });
}

/** @private */
function hookupDtmfSenderCallback_() {
  setOnToneChange(function(tone) {
    debug('Sent DTMF tone: ' + tone.tone);
    $('dtmf-tones-sent').value =
      tone.tone + '\n' + $('dtmf-tones-sent').value;
  });
}
