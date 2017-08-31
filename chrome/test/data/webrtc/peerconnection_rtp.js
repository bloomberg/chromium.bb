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
 * Verifies that the peer connection's getSenders() returns one sender per local
 * track, that there are no duplicates and that object identity is preserved.
 *
 * Returns "ok-senders-verified" on success.
 */
function verifyRtpSenders(expectedNumTracks = null) {
  if (expectedNumTracks != null &&
      peerConnection_().getSenders().length != expectedNumTracks) {
    throw failTest('getSenders().length != expectedNumTracks');
  }
  if (!arrayEquals_(peerConnection_().getSenders(),
                    peerConnection_().getSenders())) {
    throw failTest('One getSenders() call is not equal to the next.');
  }

  let senders = new Set();
  let senderTracks = new Set();
  peerConnection_().getSenders().forEach(function(sender) {
    if (sender == null)
      throw failTest('sender is null or undefined.');
    if (sender.track == null)
      throw failTest('sender.track is null or undefined.');
    senders.add(sender);
    senderTracks.add(sender.track);
  });
  if (senderTracks.size != senders.size)
    throw failTest('senderTracks.size != senders.size');

  returnToTest('ok-senders-verified');
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

  returnToTest('ok-receivers-verified');
}

/**
 * Creates an audio and video track and adds them to the peer connection using
 * |addTrack|. They are added with or without a stream in accordance with
 * |streamArgumentType|.
 *
 * Returns
 * "ok-<audio stream id> <audio track id> <video stream id> <video track id>" on
 * success. If no stream is backing up the track, <stream id> is "null".
 *
 * @param {string} streamArgumentType Must be one of the following values:
 * 'no-stream' - The tracks are added without an associated stream.
 * 'shared-stream' - The tracks are added with the same associated stream.
 * 'individual-streams' - A stream is created for each track.
 */
function createAndAddAudioAndVideoTrack(streamArgumentType) {
  if (streamArgumentType !== 'no-stream' &&
      streamArgumentType !== 'shared-stream' &&
      streamArgumentType !== 'individual-streams')
    throw failTest('Unsupported streamArgumentType.');
  getUserMedia({ audio: true, video: true },
      function(stream) {
        let audioStream = undefined;
        if (streamArgumentType !== 'no-stream')
          audioStream = new MediaStream();

        let audioTrack = stream.getAudioTracks()[0];
        let audioSender =
            audioStream ? peerConnection_().addTrack(audioTrack, audioStream)
                        : peerConnection_().addTrack(audioTrack);
        if (!audioSender || audioSender.track != audioTrack)
          throw failTest('addTrack did not return a sender with the track.');

        let videoStream = undefined;
        if (streamArgumentType === 'shared-stream') {
          videoStream = audioStream;
        } else if (streamArgumentType === 'individual-streams') {
          videoStream = new MediaStream();
        }

        let videoTrack = stream.getVideoTracks()[0];
        let videoSender =
            videoStream ? peerConnection_().addTrack(videoTrack, videoStream)
                        : peerConnection_().addTrack(videoTrack);
        if (!videoSender || videoSender.track != videoTrack)
          throw failTest('addTrack did not return a sender with the track.');

        let audioStreamId = audioStream ? audioStream.id : 'null';
        let videoStreamId = videoStream ? videoStream.id : 'null';
        returnToTest('ok-' + audioStreamId + ' ' + audioTrack.id
                     + ' ' + videoStreamId + ' ' + videoTrack.id);
      },
      function(error) {
        throw failTest('getUserMedia failed: ' + error);
      });
}

/**
 * Calls |removeTrack| with the first sender that has the track with |trackId|
 * and verifies the SDP is updated accordingly.
 *
 * Returns "ok-sender-removed" on success.
 */
function removeTrack(trackId) {
  let sender = null;
  let otherSenderHasTrack = false;
  peerConnection_().getSenders().forEach(function(s) {
    if (s.track && s.track.id == trackId) {
      if (!sender)
        sender = s;
      else
        otherSenderHasTrack = true;
    }
  });
  if (!sender)
    throw failTest('There is no sender for track ' + trackId);
  peerConnection_().removeTrack(sender);
  if (sender.track)
    throw failTest('sender.track was not nulled by removeTrack.');
  returnToTest('ok-sender-removed');
}

/**
 * Returns "ok-stream-with-track-found" or "ok-stream-with-track-not-found".
 * If |streamId| is null then any stream having a track with |trackId| will do.
 */
function hasLocalStreamWithTrack(streamId, trackId) {
  if (hasStreamWithTrack(
          peerConnection_().getLocalStreams(), streamId, trackId)) {
    returnToTest('ok-stream-with-track-found');
    return;
  }
  returnToTest('ok-stream-with-track-not-found');
}

/**
 * Returns "ok-stream-with-track-found" or "ok-stream-with-track-not-found".
 * If |streamId| is null then any stream having a track with |trackId| will do.
 */
function hasRemoteStreamWithTrack(streamId, trackId) {
  if (hasStreamWithTrack(
          peerConnection_().getRemoteStreams(), streamId, trackId)) {
    returnToTest('ok-stream-with-track-found');
    return;
  }
  returnToTest('ok-stream-with-track-not-found');
}

/**
 * Returns "ok-sender-with-track-found" or "ok-sender-with-track-not-found".
 */
function hasSenderWithTrack(trackId) {
  if (hasSenderOrReceiverWithTrack(peerConnection_().getSenders(), trackId)) {
    returnToTest('ok-sender-with-track-found');
    return;
  }
  returnToTest('ok-sender-with-track-not-found');
}

/**
 * Returns "ok-receiver-with-track-found" or "ok-receiver-with-track-not-found".
 */
function hasReceiverWithTrack(trackId) {
  if (hasSenderOrReceiverWithTrack(peerConnection_().getReceivers(), trackId)) {
    returnToTest('ok-receiver-with-track-found');
    return;
  }
  returnToTest('ok-receiver-with-track-not-found');
}

function createReceiverWithSetRemoteDescription() {
  var pc = new RTCPeerConnection();
  var receivers = null;
  pc.setRemoteDescription(createOffer("stream", "track1")).then(() => {
      receivers = pc.getReceivers();
      if (receivers.length != 1)
        throw failTest('getReceivers() should return 1 receiver: ' +
                       receivers.length)
      if (!receivers[0].track)
        throw failTest('getReceivers()[0].track should have a value')
      returnToTest('ok');
    });
  receivers = pc.getReceivers();
  if (receivers.length != 0)
    throw failTest('getReceivers() should return 0 receivers: ' +
                   receivers.length)
}

function switchRemoteStreamAndBackAgain() {
  let pc1 = new RTCPeerConnection();
  let firstStream0 = null;
  let firstTrack0 = null;
  pc1.setRemoteDescription(createOffer('stream0', 'track0'))
    .then(() => {
      firstStream0 = pc1.getRemoteStreams()[0];
      firstTrack0 = firstStream0.getTracks()[0];
      if (firstStream0.id != 'stream0')
        throw failTest('Unexpected firstStream0.id');
      if (firstTrack0.id != 'track0')
        throw failTest('Unexpected firstTrack0.id');
      return pc1.setRemoteDescription(createOffer('stream1', 'track1'));
    }).then(() => {
      return pc1.setRemoteDescription(createOffer('stream0', 'track0'));
    }).then(() => {
      let secondStream0 = pc1.getRemoteStreams()[0];
      let secondTrack0 = secondStream0.getTracks()[0];
      if (secondStream0.id != 'stream0')
        throw failTest('Unexpected secondStream0.id');
      if (secondTrack0.id != 'track0')
        throw failTest('Unexpected secondTrack0.id');
      if (secondTrack0 == firstTrack0)
        throw failTest('Expected a new track object with the same id');
      if (secondStream0 == firstStream0)
        throw failTest('Expected a new stream object with the same id');
      returnToTest('ok');
    });
}

/**
 * Invokes the GC and returns "ok-gc".
 */
function collectGarbage() {
  gc();
  returnToTest('ok-gc');
}

// Internals.

function createOffer(streamId, trackId) {
  return {
    type: "offer",
    sdp: "v=0\n" +
      "o=- 0 1 IN IP4 0.0.0.0\n" +
      "s=-\n" +
      "t=0 0\n" +
      "a=ice-ufrag:0000\n" +
      "a=ice-pwd:0000000000000000000000\n" +
      "a=fingerprint:sha-256 00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:" +
      "00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00\n" +
      "m=audio 9 UDP/TLS/RTP/SAVPF 0\n" +
      "c=IN IP4 0.0.0.0\n" +
      "a=sendonly\n" +
      "a=rtcp-mux\n" +
      "a=ssrc:1 cname:0\n" +
      "a=ssrc:1 msid:" + streamId + " " + trackId + "\n"
  };
}

/** @private */
function hasStreamWithTrack(streams, streamId, trackId) {
  for (let i = 0; i < streams.length; ++i) {
    let stream = streams[i];
    if (streamId && stream.id !== streamId)
      continue;
    let tracks = stream.getTracks();
    for (let j = 0; j < tracks.length; ++j) {
      let track = tracks[j];
      if (track.id == trackId) {
        return true;
      }
    }
  }
  return false;
}

/** @private */
function hasSenderOrReceiverWithTrack(sendersOrReceivers, trackId) {
  for (let i = 0; i < sendersOrReceivers.length; ++i) {
    if (sendersOrReceivers[i].track &&
        sendersOrReceivers[i].track.id === trackId) {
      return true;
    }
  }
  return false;
}

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
