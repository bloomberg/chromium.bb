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

  var marginValidationStates = {
    TOO_SMALL: 0,
    WITHIN_RANGE: 1,
    TOO_BIG: 2,
    NOT_A_NUMBER: 3
  }

  /**
   * Checks  whether |value| is within range [0, limit].
   * @return {number} An appropriate value from enum |marginValidationStates|.
   */
  function validateMarginValue(value, limit) {
    if (value <= limit && value >= 0)
      return marginValidationStates.WITHIN_RANGE;
    else if (value < 0)
      return marginValidationStates.TOO_SMALL;
    else (value > limit)
      return marginValidationStates.TOO_BIG;
  }

  /**
   * @param {sting} text The text to check (in inches).
   * @param {number} limit The upper bound of the valid margin range (in
   *     points).
   * @return {number} An appropriate value from enum |marginValidationStates|.
   */
  function validateMarginText(text, limit) {
    var value = extractMarginValue(text);
    if (value == -1)
      return marginValidationStates.NOT_A_NUMBER;
    value = convertInchesToPoints(value);
    return validateMarginValue(value, limit);
  }

  /**
   * @param {number} toConvert The value to convert in points
   * @return {string} The equivalent text in inches.
   */
  function convertPointsToInchesText(toConvert) {
    var inInches = convertPointsToInches(toConvert);
    return convertInchesToInchesText(inInches);
  }

  /**
   * @param {number} toConvert The value to convert in inches.
   * @return {string} The equivalent text in inches with precision of two
   *     digits.
   */
  function convertInchesToInchesText(toConvert) {
    return toConvert.toFixed(2) + '"';
  }

  /**
   * Converts |value| to inches text (keeping 2 decimal digits) and then back to
   * points. Note: Because of the precision the return value might be different
   * than |value|.
   * @param {number} value The value in points to convert.
   * @return {number} The value in points after converting to inches with a
   *     certain precision and back.
   */
  function convertPointsToInchesTextAndBack(value) {
    var text = convertPointsToInchesText(value);
    var inches = extractMarginValue(text);
    return convertInchesToPoints(inches);
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
    /**
     * @type {number} The x coordinate of the right-most point.
     */
    get right() {
      return this.x + this.width;
    },

    /**
     * @type {number} The y coordinate of the lower-most point.
     */
    get bottom() {
      return this.y + this.height;
    },

    /**
     * Clones |this| and returns the cloned object.
     * @return {Rect} A copy of |this|.
     */
    clone: function() {
      return new Rect(this.x, this.y, this.width, this.height);
    }
  };

  return {
    convertInchesToInchesText: convertInchesToInchesText,
    convertPointsToInchesTextAndBack:convertPointsToInchesTextAndBack,
    convertPointsToInchesText: convertPointsToInchesText,
    extractMarginValue: extractMarginValue,
    marginValidationStates: marginValidationStates,
    Rect: Rect,
    validateMarginText: validateMarginText,
    validateMarginValue: validateMarginValue
  };
});
