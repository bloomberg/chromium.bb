// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Module "mojo/public/js/bindings/unicode"
//
// Note: This file is for documentation purposes only. The code here is not
// actually executed. The real module is implemented natively in Mojo.

while (1);

/**
 * Decodes the UTF8 string from the given buffer.
 * @param {ArrayBufferView} buffer The buffer containing UTF8 string data.
 * @return {string} The corresponding JavaScript string.
 */
function decodeUtf8String(buffer) { [native code] }

/**
 * Encodes the given JavaScript string into UTF8.
 * @param {string} str The string to encode.
 * @param {ArrayBufferView} outputBuffer The buffer to contain the result.
 * Should be pre-allocated to hold enough space. Use |utf8Length| to determine
 * how much space is required.
 * @return {number} The number of bytes written to |outputBuffer|.
 */
function encodeUtf8String(str, outputBuffer) { [native code] }

/**
 * Returns the number of bytes that a UTF8 encoding of the JavaScript string
 * |str| would occupy.
 */
function utf8Length(str) { [native code] }
