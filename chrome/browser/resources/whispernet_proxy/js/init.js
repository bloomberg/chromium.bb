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
 * @param {Object} audioParams Audio parameters used to initialize the encoder
 * and decoder.
 */
function initialize(audioParams) {
  if (!whispernetNacl) {
    chrome.copresencePrivate.sendInitialized(false);
    return;
  }

  console.log('init: creating encoder!');
  whisperEncoder = new WhisperEncoder(audioParams.play, whispernetNacl);
  whisperEncoder.setAudioDataCallback(chrome.copresencePrivate.sendSamples);

  console.log('init: creating decoder!');
  whisperDecoder = new WhisperDecoder(audioParams.record, whispernetNacl);
  whisperDecoder.setReceiveCallback(chrome.copresencePrivate.sendFound);
  whisperDecoder.onDetectBroadcast(chrome.copresencePrivate.sendDetect);

  chrome.copresencePrivate.sendInitialized(true);
}

/**
 * Sends a request to whispernet to encode a token.
 * @param {string} token Token to encode. This needs to be a base64 string.
 * @param {boolean} audible Whether we should use encode audible samples.
 */
function encodeTokenRequest(token, audible) {
  if (whisperEncoder) {
    whisperEncoder.encode(atob(token), audible, true);
  } else {
    console.error('encodeTokenRequest: Whisper not initialized!');
  }
}

/**
 * Sends a request to whispernet to decode samples.
 * @param {ArrayBuffer} samples Array of samples to decode.
 */
function decodeSamplesRequest(samples) {
  if (whisperDecoder) {
    whisperDecoder.processSamples(samples);
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
  chrome.copresencePrivate.onInitialize.addListener(initialize);
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
