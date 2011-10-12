// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  'use strict';

  /**
   * Checks if |text| has a valid margin value format. A valid format is
   * parsable as a number and is greater than zero.
   * Example: "1.00", "1", ".5", "1.1" are valid values.
   * Example: "1.4dsf", "-1" are invalid.
   * Note: The inch symbol (") at the end of |text| is allowed.
   *
   * @param {string} text The text to check.
   * @return {number} The margin value represented by |text| or null if |text|
   *     does not represent a valid number.
   */
  function extractMarginValue(text) {
    // Remove whitespace anywhere in the string.
    text.replace(/\s*/g, '');
    if (text.length == 0)
      return -1;
    // Remove the inch(") symbol at end of string if present.
    if (text.charAt(text.length - 1) == '\"')
      text = text.slice(0, text.length - 1);
    var regex = /^\d*(\.\d+)?$/
    if (regex.test(text))
      return parseFloat(text);
    return -1;
  }

  /**
   * @param {sting} text The text to check (in inches).
   * @param {number} limit The upper bound of the valid margin range (in
   *     points).
   * @return {boolean} True of |text| can be parsed and it is within the allowed
   *     range.
   */
  function isMarginTextValid(text, limit) {
    var value = extractMarginValue(text);
    if (value ==  -1)
      return false;
    value = convertInchesToPoints(value);
    return value <= limit;
  }

  /**
   * Creates a Rect object. This object describes a rectangle in a 2D plane. The
   * units of |x|, |y|, |width|, |height| are chosen by clients of this class.
   * @constructor
   */
  function Rect(x, y, width, height) {
    // @type {number} Horizontal distance of the upper left corner from origin.
    this.x = x;
    // @type {number} Vertical distance of the upper left corner from origin.
    this.y = y;
    // @type {number} Width of |this| rectangle.
    this.width = width;
    // @type {number} Height of |this| rectangle.
    this.height = height;
  };

  Rect.prototype = {
    get right() {
      return this.x + this.width;
    },

    get bottom() {
      return this.y + this.height;
    },

    get middleX() {
      return this.x + this.width / 2;
    },

    get middleY() {
      return this.y + this.height / 2;
    }
  };

  return {
    extractMarginValue: extractMarginValue,
    isMarginTextValid: isMarginTextValid,
    Rect: Rect,
  };
});
