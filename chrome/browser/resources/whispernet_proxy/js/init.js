// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

// Globals holding our encoder and decoder. We will never have more than one
// Global variable that will be used to access this Nacl bridge.
var whispernetNacl = null;

// copy of an encoder or a decoder at a time.
var whisperEncoder = null;
var whisperDecoder = null;

/**
 * Initialize the whispernet encoder and decoder.
 * @param {Object} audioParams Object containing the parameters needed for
 *     setting up audio config.
 */
function audioConfig(audioParams) {
  if (!whispernetNacl) {
    chrome.copresencePrivate.sendInitialized(false);
    return;
  }

  console.log('Configuring encoder!');
  whisperEncoder = new WhisperEncoder(audioParams.paramData, whispernetNacl);
  whisperEncoder.setAudioDataCallback(chrome.copresencePrivate.sendSamples);

  console.log('Configuring decoder!');
  whisperDecoder = new WhisperDecoder(audioParams.paramData, whispernetNacl);
  whisperDecoder.setReceiveCallback(chrome.copresencePrivate.sendFound);
  whisperDecoder.onDetectBroadcast(chrome.copresencePrivate.sendDetect);

  chrome.copresencePrivate.sendInitialized(true);
}

/**
 * Sends a request to whispernet to encode a token.
 * @param {Object} params Encode token parameters object.
 */
function encodeTokenRequest(params) {
  if (whisperEncoder) {
    whisperEncoder.encode(params);
  } else {
    console.error('encodeTokenRequest: Whisper not initialized!');
  }
}

/**
 * Sends a request to whispernet to decode samples.
 * @param {Object} params Process samples parameters object.
 */
function decodeSamplesRequest(params) {
  if (whisperDecoder) {
    whisperDecoder.processSamples(params);
  } else {
    console.error('decodeSamplesRequest: Whisper not initialized!');
  }
}

/**
 * Sends a request to whispernet to detect broadcast.
 */
function detectBroadcastRequest() {
  if (whisperDecoder) {
    whisperDecoder.detectBroadcast();
  } else {
    console.error('detectBroadcastRequest: Whisper not initialized!');
  }
}

/**
 * Initialize our listerners and signal that the extension is loaded.
 */
function onWhispernetLoaded() {
  console.log('init: Nacl ready!');

  // Setup all the listeners for the private API.
  chrome.copresencePrivate.onConfigAudio.addListener(audioConfig);
  chrome.copresencePrivate.onEncodeTokenRequest.addListener(encodeTokenRequest);
  chrome.copresencePrivate.onDecodeSamplesRequest.addListener(
      decodeSamplesRequest);
  chrome.copresencePrivate.onDetectBroadcastRequest.addListener(
      detectBroadcastRequest);

  // This first initialized is sent to indicate that the library is loaded.
  // Every other time, it will be sent only when Chrome wants to reinitialize
  // the encoder and decoder.
  chrome.copresencePrivate.sendInitialized(true);
}

/**
 * Initialize the whispernet Nacl bridge.
 */
function initWhispernet() {
  console.log('init: Starting Nacl bridge.');
  // TODO(rkc): Figure out how to embed the .nmf and the .pexe into component
  // resources without having to rename them to .js.
  whispernetNacl = new NaclBridge('whispernet_proxy.nmf.png',
                                  onWhispernetLoaded);
}

window.addEventListener('DOMContentLoaded', initWhispernet);
