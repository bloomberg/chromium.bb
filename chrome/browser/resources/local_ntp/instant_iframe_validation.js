// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


/**
 * @fileoverview Helpers for validating parameters to chrome-search:// iframes.
 */


/**
 * Converts an RGB color number to a hex color string if valid.
 * @param {number} color A 6-digit hex RGB color code as a number.
 * @return {?string} A CSS representation of the color or null if invalid.
 */
function convertColor(color) {
  // Color must be a number, finite, with no fractional part, in the correct
  // range for an RGB hex color.
  if (isFinite(color) && Math.floor(color) == color &&
      color >= 0 && color <= 0xffffff) {
    var hexColor = color.toString(16);
    // Pads with initial zeros and # (e.g. for 'ff' yields '#0000ff').
    return '#000000'.substr(0, 7 - hexColor.length) + hexColor;
  }
  return null;
}
