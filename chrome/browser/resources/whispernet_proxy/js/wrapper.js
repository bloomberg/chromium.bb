// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Function to convert an array of bytes to a base64 string
 * TODO(rkc): Change this to use a Uint8array instead of a string.
 * @param {string} bytes String containing the bytes we want to convert.
 * @return {string} String containing the base64 representation.
 */
function bytesToBase64(bytes) {
  var bstr = '';
  for (var i = 0; i < bytes.length; ++i)
    bstr += String.fromCharCode(bytes[i]);
  return btoa(bstr);
}

/**
 * Function to convert a string to an array of bytes.
 * @param {string} str String to convert.
 * @return {Array} Array containing the string.
 */
function stringToArray(str) {
  var buffer = [];
  for (var i = 0; i < str.length; ++i)
    buffer[i] = str.charCodeAt(i);
  return buffer;
}

/**
 * Creates a whispernet encoder.
 * @constructor
 * @param {Object} params Dictionary of parameters used to initialize the
 * whispernet encoder.
 * @param {Object} whisperNacl The NaclBridge object to use to communicate with
 * the whispernet wrapper.
 */
function WhisperEncoder(params, whisperNacl) {
  params = params || {};
  this.repetitions_ = params.repetitions || 3;

  this.whisperNacl_ = whisperNacl;
  this.whisperNacl_.addListener(this.onNaclMessage_.bind(this));

  var symbolCoder = {};
  symbolCoder.sample_rate = params.sampleRate || 48000.0;
  symbolCoder.upsampling_factor = params.bitsPerSample || 16;
  symbolCoder.desired_carrier_frequency = params.carrierFrequency || 18500.0;
  symbolCoder.bits_per_symbol = 4;
  symbolCoder.min_cycles_per_frame = 4;
  symbolCoder.baseband_decimation_factor = 4;

  var msg = {
    type: 'initialize_encoder',
    symbol_coder: symbolCoder,
    encoder_params: {
      bytes_per_token: 6,
      include_parity_symbol: true,
      single_sideband: true
    }
  };
  this.whisperNacl_.send(JSON.stringify(msg));
}

/**
 * Method to encode a token.
 * @param {string} token Token to encode.
 * @param {boolean} raw Whether we should return the encoded samples in raw
 * format or as a Wave file.
 */
WhisperEncoder.prototype.encode = function(token, raw) {
  var msg = {
    type: 'encode_token',
    // Trying to send the token in binary form to Nacl doesn't work correctly.
    // We end up with the correct string + a bunch of extra characters. This is
    // true of returning a binary string too; hence we communicate back and
    // forth by converting the bytes into an array of integers.
    token: stringToArray(token),
    repetitions: this.repetitions_,
    return_raw_samples: raw
  };
  this.whisperNacl_.send(JSON.stringify(msg));
};

/**
 * Method to set the callback for encoded audio data received from the encoder
 * when we finish encoding a token.
 * @param {function(string, ArrayBuffer)} callback Callback which will receive
 * the audio samples.
 */
WhisperEncoder.prototype.setAudioDataCallback = function(callback) {
  this.audioDataCallback_ = callback;
};

/**
 * Method to handle messages from the whispernet NaCl wrapper.
 * @param {Event} e Event from the whispernet wrapper.
 * @private
 */
WhisperEncoder.prototype.onNaclMessage_ = function(e) {
  var msg = e.data;
  if (msg.type == 'encode_token_response') {
    this.audioDataCallback_(bytesToBase64(msg.token), msg.samples);
  }
};

/**
 * Creates a whispernet decoder.
 * @constructor
 * @param {Object} params Dictionary of parameters used to initialize the
 * whispernet decoder.
 * @param {Object} whisperNacl The NaclBridge object to use to communicate with
 * the whispernet wrapper.
 */
function WhisperDecoder(params, whisperNacl) {
  params = params || {};

  this.whisperNacl_ = whisperNacl;
  this.whisperNacl_.addListener(this.onNaclMessage_.bind(this));

  var msg = {
    type: 'initialize_decoder',
    num_channels: params.channels,
    symbol_coder: {
      sample_rate: params.sampleRate || 48000.0,
      upsampling_factor: params.bitsPerSample || 16,
      desired_carrier_frequency: params.carrierFrequency || 18500.0,
      bits_per_symbol: 4,
      min_cycles_per_frame: 4,
      baseband_decimation_factor: 4
    },
    decoder_params: {
      bytes_per_token: 6,
      include_parity_symbol: true,
      max_candidates: 1,
      broadcaster_stopped_threshold_in_seconds: 10
    },
    acquisition_params: {
      max_buffer_duration_in_seconds: 3
    }
  };
  this.whisperNacl_.send(JSON.stringify(msg));
}

/**
 * Method to request the decoder to wipe its internal buffer.
 */
WhisperDecoder.prototype.wipeDecoder = function() {
  var msg = {
    type: 'wipe_decode_buffer'
  };
  this.whisperNacl_.send(JSON.stringify(msg));
};

/**
 * Method to request the decoder to detect a broadcast.
 */
WhisperDecoder.prototype.detectBroadcast = function() {
  var msg = {
    type: 'detect_broadcast'
  };
  this.whisperNacl_.send(JSON.stringify(msg));
};

/**
 * Method to request the decoder to process samples.
 * @param {ArrayBuffer} samples Array of samples to process.
 */
WhisperDecoder.prototype.processSamples = function(samples) {
  // For sample processing, the Nacl module doesn't expect any frills in the
  // message, just send the samples directly.
  this.whisperNacl_.send(samples);
};

/**
 * Method to set the callback for decoded tokens received from the decoder.
 * @param {function(!Array.string)} callback Callback to receive the list of
 * decoded tokens.
 */
WhisperDecoder.prototype.setReceiveCallback = function(callback) {
  this.tokenCallback_ = callback;
};

/**
 * Method to set the callback for receiving the detect callback status received
 * from the decoder.
 * @param {function()} callback Callback to set to receive the detect broadcast
 * status.
 */
WhisperDecoder.prototype.onDetectBroadcast = function(callback) {
  this.detectBroadcastCallback_ = callback;
};

/**
 * Method to handle messages from the whispernet NaCl wrapper.
 * @param {Event} e Event from the whispernet wrapper.
 * @private
 */
WhisperDecoder.prototype.onNaclMessage_ = function(e) {
  var msg = e.data;
  if (msg.type == 'decode_tokens_response') {
    this.handleCandidates_(JSON.parse(msg.tokens));
  } else if (msg.type == 'detect_broadcast_response') {
    this.detectBroadcastCallback_(msg.detected);
  }
};

/**
 * Method to receive tokens from the decoder and process and forward them to the
 * token callback registered with us.
 * @param {!Array.string} candidates Array of token candidates.
 * @private
 */
WhisperDecoder.prototype.handleCandidates_ = function(candidates) {
  if (!this.tokenCallback_ || !candidates || candidates.length == 0)
    return;

  var returnCandidates = [];
  for (var i = 0; i < candidates.length; ++i)
    returnCandidates[i] = bytesToBase64(candidates[i]);
  this.tokenCallback_(returnCandidates);
};
