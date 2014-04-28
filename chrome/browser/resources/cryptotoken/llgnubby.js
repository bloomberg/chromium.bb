// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Interface for representing a low-level gnubby device.
 */
'use strict';

/**
 * Low level gnubby 'driver'. One per physical USB device.
 * @interface
 */
function llGnubby() {}

// Commands of the USB interface.
// //depot/google3/security/tools/gnubby/gnubbyd/gnubby_if.h
llGnubby.CMD_PING =      0x81;
llGnubby.CMD_ATR =       0x82;
llGnubby.CMD_APDU =      0x83;
llGnubby.CMD_LOCK =      0x84;
llGnubby.CMD_SYSINFO =   0x85;
llGnubby.CMD_PROMPT =    0x87;
llGnubby.CMD_WINK =      0x88;
llGnubby.CMD_USB_TEST =  0xb9;
llGnubby.CMD_DFU =       0xba;
llGnubby.CMD_SYNC =      0xbc;
llGnubby.CMD_ERROR =     0xbf;

// Low-level error codes.
// //depot/google3/security/tools/gnubby/gnubbyd/gnubby_if.h
// //depot/google3/security/tools/gnubby/client/gnubby_error_codes.h
llGnubby.OK =            0;
llGnubby.INVALID_CMD =   1;
llGnubby.INVALID_PAR =   2;
llGnubby.INVALID_LEN =   3;
llGnubby.INVALID_SEQ =   4;
llGnubby.TIMEOUT =       5;
llGnubby.BUSY =          6;
llGnubby.ACCESS_DENIED = 7;
llGnubby.GONE =          8;
llGnubby.VERIFY_ERROR =  9;
llGnubby.LOCK_REQUIRED = 10;
llGnubby.SYNC_FAIL =     11;
llGnubby.OTHER =         127;

// Remote helper errors.
llGnubby.NOTREMOTE =     263;
llGnubby.COULDNOTDIAL =  264;

// chrome.usb-related errors.
llGnubby.NODEVICE =      512;
llGnubby.NOPERMISSION =  666;

/** Destroys this low-level device instance. */
llGnubby.prototype.destroy = function() {};

/**
 * Register a client for this gnubby.
 * @param {*} who The client.
 */
llGnubby.prototype.registerClient = function(who) {};

/**
 * De-register a client.
 * @param {*} who The client.
 * @return {number} The number of remaining listeners for this device, or -1
 *     if this had no clients to start with.
 */
llGnubby.prototype.deregisterClient = function(who) {};

/**
 * @param {*} who The client.
 * @return {boolean} Whether this device has who as a client.
 */
llGnubby.prototype.hasClient = function(who) {};

/**
 * Queue command to be sent.
 * If queue was empty, initiate the write.
 * @param {number} cid The client's channel ID.
 * @param {number} cmd The command to send.
 * @param {ArrayBuffer} data
 */
llGnubby.prototype.queueCommand = function(cid, cmd, data) {};
