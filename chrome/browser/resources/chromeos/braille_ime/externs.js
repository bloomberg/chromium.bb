// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Externs for the braille IME.
 * @externs
 */

// TODO: Remove the ChromeKeyboardEvent additions when they are included
// in third_party/closure_compiler/externs/chrome_extensions.js.

/** @type {string} */
ChromeKeyboardEvent.prototype.code;

/** @type {boolean|undefined} */
ChromeKeyboardEvent.prototype.capsLock;

/**
 * @type {Object}
 */
var localStorage;
