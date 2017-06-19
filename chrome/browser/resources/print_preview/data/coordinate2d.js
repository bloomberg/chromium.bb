// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  'use strict';

  /**
   * Immutable two dimensional point in space. The units of the dimensions are
   * undefined.
   * @param {number} x X-dimension of the point.
   * @param {number} y Y-dimension of the point.
   * @constructor
   */
  function Coordinate2d(x, y) {
    /**
     * X-dimension of the point.
     * @type {number}
     * @private
     */
    this.x_ = x;

    /**
     * Y-dimension of the point.
     * @type {number}
     * @private
     */
    this.y_ = y;
  }

  Coordinate2d.prototype = {
    /** @return {number} X-dimension of the point. */
    get x() {
      return this.x_;
    },

    /** @return {number} Y-dimension of the point. */
    get y() {
      return this.y_;
    },

    /**
     * @param {number} x Amount to translate in the X dimension.
     * @param {number} y Amount to translate in the Y dimension.
     * @return {!print_preview.Coordinate2d} A new two-dimensional point
     *     translated along the X and Y dimensions.
     */
    translate: function(x, y) {
      return new Coordinate2d(this.x_ + x, this.y_ + y);
    },

    /**
     * @param {number} factor Amount to scale the X and Y dimensions.
     * @return {!print_preview.Coordinate2d} A new two-dimensional point scaled
     *     by the given factor.
     */
    scale: function(factor) {
      return new Coordinate2d(this.x_ * factor, this.y_ * factor);
    },

    /**
     * @param {print_preview.Coordinate2d} other The point to compare against.
     * @return {boolean} Whether another point is equal to this one.
     */
    equals: function(other) {
      return other != null && this.x_ == other.x_ && this.y_ == other.y_;
    }
  };

  // Export
  return {Coordinate2d: Coordinate2d};
});
