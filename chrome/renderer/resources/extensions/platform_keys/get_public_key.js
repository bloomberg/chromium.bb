// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var internalAPI = require('platformKeys.internalAPI');

var normalizeAlgorithm =
    requireNative('platform_keys_natives').NormalizeAlgorithm;

function combineAlgorithms(algorithm, importParams) {
  if (importParams.name === undefined) {
    importParams.name = algorithm.name;
  }

  // Verify whether importParams.hash equals
  //   { name: 'none' }
  if (importParams.hash &&
      importParams.hash.name.toLowerCase() === 'none') {
    if (Object.keys(importParams.hash).length != 1 ||
        Object.keys(importParams).length != 2) {
      // 'name' must be the only hash property in this case.
      throw new Error('A required parameter was missing or out-of-range');
    }
    importParams.hash.name = 'none';
  } else {
    // Otherwise apply WebCrypto's algorithm normalization.
    importParams = normalizeAlgorithm(importParams, 'ImportKey');
    if (!importParams) {
      //    throw CreateSyntaxError();
      throw new Error('A required parameter was missing or out-of-range');
    }
  }

  // internalAPI.getPublicKey returns publicExponent as ArrayBuffer, but it
  // should be a Uint8Array.
  if (algorithm.publicExponent) {
    algorithm.publicExponent = new Uint8Array(algorithm.publicExponent);
  }

  for (var key in importParams) {
    algorithm[key] = importParams[key];
  }

  return algorithm;
}

function getPublicKey(cert, importParams, callback) {
  internalAPI.getPublicKey(cert, function(publicKey, algorithm) {
    var combinedAlgorithm = combineAlgorithms(algorithm, importParams);
    callback(publicKey, combinedAlgorithm);
  });
}

exports.getPublicKey = getPublicKey;
