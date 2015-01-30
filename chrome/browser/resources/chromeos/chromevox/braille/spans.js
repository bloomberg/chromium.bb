// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Objects used in spannables as annotations for ARIA values
 * and selections.
 */

goog.provide('cvox.ValueSelectionSpan');
goog.provide('cvox.ValueSpan');

goog.require('cvox.Spannable');

/**
 * Attached to the value region of a braille spannable.
 * @param {number} offset The offset of the span into the value.
 * @constructor
 */
cvox.ValueSpan = function(offset) {
  /**
   * The offset of the span into the value.
   * @type {number}
   */
  this.offset = offset;
};


/**
 * Creates a value span from a json serializable object.
 * @param {!Object} obj The json serializable object to convert.
 * @return {!cvox.ValueSpan} The value span.
 */
cvox.ValueSpan.fromJson = function(obj) {
  return new cvox.ValueSpan(obj.offset);
};


/**
 * Converts this object to a json serializable object.
 * @return {!Object} The JSON representation.
 */
cvox.ValueSpan.prototype.toJson = function() {
  return this;
};


cvox.Spannable.registerSerializableSpan(
    cvox.ValueSpan,
    'cvox.ValueSpan',
    cvox.ValueSpan.fromJson,
    cvox.ValueSpan.prototype.toJson);


/**
 * Attached to the selected text within a value.
 * @constructor
 */
cvox.ValueSelectionSpan = function() {
};


cvox.Spannable.registerStatelessSerializableSpan(
    cvox.ValueSelectionSpan,
    'cvox.ValueSelectionSpan');
