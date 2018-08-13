// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview API used for secure communication with frames.
 */

goog.provide('__crWeb.frameMessaging');

goog.require('__crWeb.base');

/**
 * Namespace for this module.
 */
__gCrWeb.frameMessaging = {};

// Store message namespace object in a global __gCrWeb object referenced by a
// string, so it does not get renamed by closure compiler during the
// minification.
__gCrWeb['frameMessaging'] = __gCrWeb.frameMessaging;

/** Beginning of anonymous object */
(function() {

/**
 * Unique identifier for this frame.
 * @type {?string}
 * @private
 */
var frameId_ = null;

/**
 * The encryption key to decrypt messages received from native code.
 * @type {?webCrypto.CryptoKey}
 * @private
 */
var frameSymmetricKey_ = null;

/**
 * Returns the frameId associated with this frame. A new value will be created
 * for this frame the first time it is called. The frameId will persist as long
 * as this JavaScript context lives. For example, the frameId will be the same
 * when navigating 'back' to this frame.
 * @return {string} A string representing a unique identifier for this frame.
 */
__gCrWeb.frameMessaging['getFrameId'] = function() {
  if (!frameId_) {
    // Generate 128 bit unique identifier.
    var components = new Uint32Array(4);
    window.crypto.getRandomValues(components);
    frameId_ = '';
    for (var i = 0; i < components.length; i++) {
      // Convert value to base16 string and append to the |frameId_|.
      frameId_ += components[i].toString(16);
    }
  }
  return /** @type {string} */ (frameId_);
};

/**
 * Exports |frameSymmetricKey_| as a base64 string. Key will be created if it
 * does not already exist.
 * @param {function(string)} callback A callback to be run with the exported
 *                           base64 key.
 */
var exportKey_ = function(callback) {
  // Early return with an empty key string if encryption is not supported in
  // this frame.
  // - Insecure contexts do not support the crypto.subtle API.
  // - Even though iOS 10 supports window.crypto.webkitSubtle instead of
  //   window.crypto.subtle, the AES-GCM cipher suite is not supported, so
  //   support will only be used from the official WebCrypto API.
  //   TODO(crbug.com/872818): Remove comment once only iOS 11+ is supported.
  if (!window.isSecureContext || !window.crypto.subtle) {
    callback("");
    return;
  }
  getFrameSymmetricKey_(function(key) {
    window.crypto.subtle.exportKey('raw', key)
        .then(function(/** @type {ArrayBuffer} */ k) {
      var keyBytes = new Uint8Array(k);
      var key64 = btoa(String.fromCharCode.apply(null, keyBytes));
      callback(key64);
    });
  });
};

/**
 * Runs |callback| with the key associated with this frame. The key will be
 * created if necessary. The key will persist as long as this JavaScript context
 * lives. For example, the key will be the same when navigating 'back' to this
 * frame.
 * @param {function(!webCrypto.CryptoKey)} callback A callback to be run with
 *                                         the key.
 */
var getFrameSymmetricKey_ = function(callback) {
  if (frameSymmetricKey_) {
    callback(frameSymmetricKey_);
    return;
  }
  window.crypto.subtle.generateKey(
    {'name': 'AES-GCM', 'length': 256},
    true,
    ['decrypt', 'encrypt']
  ).then(function(/** @type {!webCrypto.CryptoKey} */ key) {
    frameSymmetricKey_ = key;
    callback(frameSymmetricKey_);
  });
};

/**
 * Creates (or gets the existing) encryption key and sends it to the native
 * application.
 */
__gCrWeb.frameMessaging['registerFrame'] = function() {
  exportKey_(function(frameKey) {
    __gCrWeb.common.sendWebKitMessage('FrameBecameAvailable', {
      'crwFrameId': __gCrWeb.frameMessaging['getFrameId'](),
      'crwFrameKey': frameKey
    });
  });
};

/**
 * Registers this frame with the native code and forwards the message to any
 * child frames.
 * This needs to be called by the native application on each navigation
 * because no JavaScript events are fired reliably when a page is displayed and
 * hidden. This is especially important when a page remains alive and is re-used
 * from the WebKit page cache.
 * TODO(crbug.com/872134): In iOS 12, the JavaScript pageshow and pagehide
 *                         events seem reliable, so replace this exposed
 *                         function with a pageshow event listener.
 */
__gCrWeb.frameMessaging['getExistingFrames'] = function() {
  __gCrWeb.frameMessaging['registerFrame']();

  var framecount = window['frames']['length'];
  for (var i = 0; i < framecount; i++) {
    window.frames[i].postMessage(
        {type: 'org.chromium.registerForFrameMessaging',},
        '*'
    );
  }
};

}());  // End of anonymous object
