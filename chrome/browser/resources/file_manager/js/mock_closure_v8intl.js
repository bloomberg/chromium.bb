// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Mock for |v8Intl| namespace.
 */
var v8Intl = window['v8Intl'] || {};

/**
 * Mock constructor of |v8Intl.Collator|.
 * @constructor
 * @param {Array.<*>} locales  //TODO(JSDOC).
 * @param {Object.<*, *>} options  //TODO(JSDOC).
 */
v8Intl.Collator = v8Intl.Collator || function(locales, options) {};

/**
 * Mock constructor of |v8Intl.DateTimeFormat|.
 * @constructor
 * @param {Array.<*>} locales  //TODO(JSDOC).
 * @param {Object.<*, *>} options  //TODO(JSDOC).
 */
v8Intl.DateTimeFormat = v8Intl.DateTimeFormat || function(locales, options) {};
