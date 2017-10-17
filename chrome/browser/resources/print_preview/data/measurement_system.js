// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.exportPath('print_preview');

/**
 * Enumeration of measurement unit types.
 * @enum {number}
 */
print_preview.MeasurementSystemUnitType = {
  METRIC: 0,   // millimeters
  IMPERIAL: 1  // inches
};

cr.define('print_preview', function() {
  'use strict';

  class MeasurementSystem {
    /**
     * Measurement system of the print preview. Used to parse and serialize
     * point measurements into the system's local units (e.g. millimeters,
     * inches).
     * @param {string} thousandsDelimeter Delimeter between thousands digits.
     * @param {string} decimalDelimeter Delimeter between integers and decimals.
     * @param {!print_preview.MeasurementSystemUnitType} unitType Measurement
     *     unit type of the system.
     */
    constructor(thousandsDelimeter, decimalDelimeter, unitType) {
      /**
       * The thousands delimeter to use when displaying numbers.
       * @private {string}
       */
      this.thousandsDelimeter_ = thousandsDelimeter || ',';

      /**
       * The decimal delimeter to use when displaying numbers.
       * @private {string}
       */
      this.decimalDelimeter_ = decimalDelimeter || '.';

      /**
       * The measurement unit type (metric or imperial).
       * @private {!print_preview.MeasurementSystemUnitType}
       */
      this.unitType_ = unitType;
    }

    /** @return {string} The unit type symbol of the measurement system. */
    get unitSymbol() {
      if (this.unitType_ == print_preview.MeasurementSystemUnitType.METRIC) {
        return 'mm';
      }
      if (this.unitType_ == print_preview.MeasurementSystemUnitType.IMPERIAL) {
        return '"';
      }
      throw Error('Unit type not supported: ' + this.unitType_);
    }

    /**
     * @return {string} The thousands delimeter character of the measurement
     *     system.
     */
    get thousandsDelimeter() {
      return this.thousandsDelimeter_;
    }

    /**
     * @return {string} The decimal delimeter character of the measurement
     *     system.
     */
    get decimalDelimeter() {
      return this.decimalDelimeter_;
    }

    setSystem(thousandsDelimeter, decimalDelimeter, unitType) {
      this.thousandsDelimeter_ = thousandsDelimeter;
      this.decimalDelimeter_ = decimalDelimeter;
      this.unitType_ = unitType;
    }

    /**
     * Rounds a value in the local system's units to the appropriate precision.
     * @param {number} value Value to round.
     * @return {number} Rounded value.
     */
    roundValue(value) {
      var precision = Precision_[this.unitType_];
      var roundedValue = Math.round(value / precision) * precision;
      // Truncate
      return +roundedValue.toFixed(DecimalPlaces_[this.unitType_]);
    }

    /**
     * @param {number} pts Value in points to convert to local units.
     * @return {number} Value in local units.
     */
    convertFromPoints(pts) {
      if (this.unitType_ == print_preview.MeasurementSystemUnitType.METRIC) {
        return pts / PTS_PER_MM_;
      }
      return pts / PTS_PER_INCH_;
    }

    /**
     * @param {number} localUnits Value in local units to convert to points.
     * @return {number} Value in points.
     */
    convertToPoints(localUnits) {
      if (this.unitType_ == print_preview.MeasurementSystemUnitType.METRIC) {
        return localUnits * PTS_PER_MM_;
      }
      return localUnits * PTS_PER_INCH_;
    }
  }

  /**
   * Maximum resolution of local unit values.
   * @private {!Object<!print_preview.MeasurementSystemUnitType, number>}
   */
  var Precision_ = {};
  Precision_[print_preview.MeasurementSystemUnitType.METRIC] = 0.5;
  Precision_[print_preview.MeasurementSystemUnitType.IMPERIAL] = 0.01;

  /**
   * Maximum number of decimal places to keep for local unit.
   * @private {!Object<!print_preview.MeasurementSystemUnitType, number>}
   */
  var DecimalPlaces_ = {};
  DecimalPlaces_[print_preview.MeasurementSystemUnitType.METRIC] = 1;
  DecimalPlaces_[print_preview.MeasurementSystemUnitType.IMPERIAL] = 2;

  /**
   * Number of points per inch.
   * @const {number}
   * @private
   */
  var PTS_PER_INCH_ = 72.0;

  /**
   * Number of points per millimeter.
   * @const {number}
   * @private
   */
  var PTS_PER_MM_ = PTS_PER_INCH_ / 25.4;

  // Export
  return {MeasurementSystem: MeasurementSystem};
});
