/**
 * Copyright 2017 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

// Public interface to tests. These are expected to be called with
// ExecuteJavascript invocations from the browser tests and will return answers
// through the DOM automation controller.

/**
 * Adds |count| streams to the peer connection, with one audio and one video
 * track per stream.
 *
 * Returns "ok-streams-created-and-added" on success.
 */
function createAndAddStreams(count) {
  if (count > 0) {
    getUserMedia({ audio: true, video: true },
        function(stream) {
          peerConnection_().addStream(stream);
          createAndAddStreams(count - 1);
        },
        function(error) {
          throw failTest('getUserMedia failed: ' + error);
        });
  } else {
    returnToTest('ok-streams-created-and-added');
  }
}

/**
 * Verifies that the peer connection's getReceivers() returns one receiver per
 * remote track, that there are no duplicates and that object identity is
 * preserved.
 *
 * Returns "ok-receivers-verified" on success.
 */
function verifyRtpReceivers(expectedNumTracks = null) {
  if (peerConnection_().getReceivers() == null)
    throw failTest('getReceivers() returns null or undefined.');
  if (expectedNumTracks != null &&
      peerConnection_().getReceivers().length != expectedNumTracks) {
    throw failTest('getReceivers().length != expectedNumTracks');
  }
  if (!arrayEquals_(peerConnection_().getReceivers(),
                    peerConnection_().getReceivers())) {
    throw failTest('One getReceivers() call is not equal to the next.');
  }

  let remoteTracks = new Set();
  peerConnection_().getRemoteStreams().forEach(function(stream) {
    stream.getTracks().forEach(function(track) {
      remoteTracks.add(track);
    });
  });
  if (peerConnection_().getReceivers().length != remoteTracks.size)
    throw failTest('The number of receivers and tracks are not the same.');

  let receivers = new Set();
  let receiverTracks = new Set();
  peerConnection_().getReceivers().forEach(function(receiver) {
    if (receiver == null)
      throw failTest('receiver is null or undefined.');
    if (receiver.track == null)
      throw failTest('receiver.track is null or undefined.');
    if (receiver.getContributingSources().length != 0)
      throw failTest('receiver.getContributingSources() is not empty.');
    receivers.add(receiver);
    receiverTracks.add(receiver.track);
  });
  if (receiverTracks.size != receivers.size)
    throw failTest('receiverTracks.size != receivers.size');

  if (!setEquals_(receiverTracks, remoteTracks)) {
    throw failTest('The set of receiver tracks is not equal to the set of ' +
                   'stream tracks.');
  }
  returnToTest('ok-receivers-verified');
}

// Internals.

/** @private */
function arrayEquals_(a, b) {
  if (a == null)
    return b == null;
  if (a.length != b.length)
    return false;
  for (let i = 0; i < a.length; ++i) {
    if (a[i] !== b[i])
      return false;
  }
  return true;
}

/** @private */
function setEquals_(a, b) {
  if (a == null)
    return b == null;
  if (a.size != b.size)
    return false;
  a.forEach(function(value) {
    if (!b.has(value))
      return false;
  });
  return true;
}
