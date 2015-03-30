// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Utilities for strings.
 */

goog.provide('StringUtil');

/**
 * @constructor
 */
StringUtil = function() {};

/**
 * Returns the length of the longest common prefix of two strings.
 * @param {string} first The first string.
 * @param {string} second The second string.
 * @return {number} The length of the longest common prefix, which may be 0
 *     for an empty common prefix.
 */
StringUtil.longestCommonPrefixLength = function(first, second) {
  var limit = Math.min(first.length, second.length);
  var i;
  for (i = 0; i < limit; ++i) {
    if (first.charAt(i) != second.charAt(i)) {
      break;
    }
  }
  return i;
};
