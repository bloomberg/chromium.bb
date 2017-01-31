// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ClearKeyPlayer responsible for playing media using Clear Key key system.
function ClearKeyPlayer(video, testConfig) {
  this.video = video;
  this.testConfig = testConfig;
}

ClearKeyPlayer.prototype.init = function() {
  // Returns a promise.
  return PlayerUtils.initEMEPlayer(this);
};

ClearKeyPlayer.prototype.registerEventListeners = function() {
  // Returns a promise.
  return PlayerUtils.registerEMEEventListeners(this);
};

ClearKeyPlayer.prototype.onMessage = function(message) {
  Utils.timeLog('MediaKeySession onMessage', message);

  var mediaKeySession = message.target;
  var keyId = Utils.extractFirstLicenseKeyId(message.message);
  var key = Utils.getDefaultKey(this.testConfig.forceInvalidResponse);
  var jwkSet = Utils.createJWKData(keyId, key);

  // Number of milliseconds in 100 years, which is approximately
  // 100 * 365 * 24 * 60 * 60 * 1000.
  // See clear_key_cdm.cc where this value is set.
  const ECK_RENEWAL_EXPIRATION = 3153600000000;

  Utils.timeLog('Calling update: ' + String.fromCharCode.apply(null, jwkSet));
  mediaKeySession.update(jwkSet).then(function() {
    // Check session expiration.
    // - For CLEARKEY, expiration is not set and is the default value NaN.
    // - For EXTERNAL_CLEARKEY_RENEWAL, expiration is set to
    //   ECK_RENEWAL_EXPIRATION milliseconds after 01 January 1970 UTC.
    // - For other EXTERNAL_CLEARKEY variants, expiration is explicitly set to
    //   NaN.
    var expiration = mediaKeySession.expiration;
    if (this.testConfig.keySystem == EXTERNAL_CLEARKEY_RENEWAL) {
      if (isNaN(expiration) || expiration != ECK_RENEWAL_EXPIRATION) {
        Utils.timeLog('Unexpected expiration: ', expiration);
        Utils.failTest(error, EME_UPDATE_FAILED);
      }
    } else {
      if (!isNaN(mediaKeySession.expiration)) {
        Utils.timeLog('Unexpected expiration: ', expiration);
        Utils.failTest(error, EME_UPDATE_FAILED);
      }
    }
  }).catch(function(error) {
    // Ignore the error if a crash is expected. This ensures that the decoder
    // actually detects and reports the error.
    if (this.testConfig.keySystem != CRASH_TEST_KEYSYSTEM) {
      Utils.failTest(error, EME_UPDATE_FAILED);
    }
  });
};
