// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview usbGnubby methods related to U2F support.
 */
'use strict';

// Commands and flags of the Gnubby applet at
// //depot/google3/security/tools/gnubby/applet/gnubby/src/pkgGnubby/Gnubby.java
usbGnubby.U2F_ENROLL        = 0x01;
usbGnubby.U2F_SIGN          = 0x02;
usbGnubby.U2F_VERSION       = 0x03;

usbGnubby.APPLET_VERSION    = 0x11;  // First 3 bytes are applet version.

// APDU.P1 flags
usbGnubby.P1_TUP_REQUIRED   = 0x01;
usbGnubby.P1_TUP_CONSUME    = 0x02;
usbGnubby.P1_TUP_TESTONLY   = 0x04;
usbGnubby.P1_INDIVIDUAL_KEY = 0x80;

usbGnubby.prototype.enroll = function(challenge, appIdHash, cb) {
  var apdu = new Uint8Array(
      [0x00,
       usbGnubby.U2F_ENROLL,
       usbGnubby.P1_TUP_REQUIRED | usbGnubby.P1_TUP_CONSUME |
         usbGnubby.P1_INDIVIDUAL_KEY,
       0x00, 0x00, 0x00,
       challenge.length + appIdHash.length]);
  // TODO(mschilder): only use P1_INDIVIDUAL_KEY for corp appIdHashes.
  var u8 = new Uint8Array(apdu.length + challenge.length +
      appIdHash.length + 2);
  for (var i = 0; i < apdu.length; ++i) u8[i] = apdu[i];
  for (var i = 0; i < challenge.length; ++i) u8[i + apdu.length] =
    challenge[i];
  for (var i = 0; i < appIdHash.length; ++i) {
    u8[i + apdu.length + challenge.length] = appIdHash[i];
  }
  this.apduReply_(u8.buffer, cb);
};

usbGnubby.prototype.sign = function(challengeHash, appIdHash, keyHandle, cb,
                                    opt_nowink) {
  var apdu = new Uint8Array(
      [0x00,
       usbGnubby.U2F_SIGN,
       usbGnubby.P1_TUP_REQUIRED | usbGnubby.P1_TUP_CONSUME,
       0x00, 0x00, 0x00,
      challengeHash.length + appIdHash.length + keyHandle.length]);
  if (opt_nowink) {
    // A signature request that does not want winking.
    // These are used during enroll to figure out whether a gnubby was already
    // enrolled.
    // Tell applet to not actually produce a signature, even
    // if already touched.
    apdu[2] |= usbGnubby.P1_TUP_TESTONLY;
  }
  var u8 = new Uint8Array(apdu.length + challengeHash.length +
      appIdHash.length + keyHandle.length + 2);
  for (var i = 0; i < apdu.length; ++i) u8[i] = apdu[i];
  for (var i = 0; i < challengeHash.length; ++i) u8[i + apdu.length] =
    challengeHash[i];
  for (var i = 0; i < appIdHash.length; ++i) {
    u8[i + apdu.length + challengeHash.length] = appIdHash[i];
  }
  for (var i = 0; i < keyHandle.length; ++i) {
    u8[i + apdu.length + challengeHash.length + appIdHash.length] =
        keyHandle[i];
  }
  this.apduReply_(u8.buffer, cb, opt_nowink);
};

usbGnubby.prototype.version = function(cb) {
  if (!cb) cb = usbGnubby.defaultCallback;
  var apdu = new Uint8Array([0x00, usbGnubby.U2F_VERSION, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00]);
  this.apduReply_(apdu.buffer, function(rc, data) {
    if (rc == 0x6d00) {
      // Command not implemented. Pretend this is v1.
      var v1 = new Uint8Array(UTIL_StringToBytes('U2F_V1'));
      cb(-llGnubby.OK, v1.buffer);
    } else {
      cb(rc, data);
    }
  });
};
