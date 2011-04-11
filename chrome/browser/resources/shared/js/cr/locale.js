// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(rginda): Fill out formatTime, add testcases.

cr.define('cr', function() {

  /**
   * Lookup tables used by bytesToSi.
   */
  var units = ['B', 'k', 'M', 'G', 'T', 'P'];
  var scale = [1, 1e3, 1e6, 1e9, 1e12, 1e15];

  /**
   * Construct a new Locale object with a set of strings.
   *
   * The strings object maps symbolic string names to translated strings
   * for the locale.  Lists of translated strings are delimited with the caret
   * ('^') character.
   *
   *  LOCALE_DAYS_SHORT: List of abbreviated day names.
   *  LOCALE_MONTHS_SHORT: List of abbreviated month names.
   *  LOCALE_FMT_SHORT_DATE: A Locale.prototype.formatTime format specifier
   *      representing the short date format (e.g. Apr 1, 2011) for the
   *      locale.
   */
  function Locale(strings) {
    this.dateStrings_ = {
      dayShort: strings.LOCALE_DAYS_SHORT.split('^'),
      monthShort: strings.LOCALE_MONTHS_SHORT.split('^'),
      shortDateFormat: strings.LOCALE_FMT_DATE_SHORT
    };
  }

  Locale.prototype = {
    /**
     * Convert a number of bytes into an appropriate International System of
     * Units (SI) representation, using the correct number separators.
     *
     * The first time this function is called it computes a lookup table which
     * is cached for subsequent calls.
     *
     * @param {number} bytes The number of bytes.
     */
    bytesToSi: function(bytes) {
      function fmt(s, u) {
        var rounded = Math.round(bytes / s * 10) / 10;
        return rounded.toLocaleString() + u;
      }

      // This loop index is used outside the loop if it turns out |bytes|
      // requires the largest unit.
      var i;

      for (i = 0; i < units.length - 1; i++) {
        if (bytes < scale[i + 1])
          return fmt(scale[i], units[i]);
      }

      return fmt(scale[i], units[i]);
    },

    /**
     * Format a date as a string using the given format specifier.
     *
     * This function is similar to strftime() from the C standard library, with
     * the GNU extensions for controlling padding.
     *
     * The following conversion specifiers are defined:
     *
     *  %% - A literal '%'
     *  %a - The localized abbreviated weekday name.
     *  %b - The localized abbreviated month name.
     *  %d - The day of the month, zero padded (01-31).
     *  %Y - The four digit year.
     *
     * Between  the  '%'  character and the conversion specifier character, an
     * optional flag and field width may be specified.
     *
     *  The following flag characters are permitted:
     *    _  (underscore) Pad a numeric result string with spaces.
     *    -  (dash) Do not pad a numeric result string.
     *    ^  Convert alphabetic characters in result string to upper case.
     *
     * TODO(rginda): Implement more conversion specifiers.
     *
     * @param {Date} date The date to be formatted.
     * @param {string} spec The format specification.
     */
    formatDate: function(date, spec) {
      var self = this;
      var strings = this.dateStrings_;

      // Called back once for each conversion specifier.
      function replaceSpecifier(m, flag, width, code) {

        // Left pad utility.
        function lpad(value, ch) {
          value = String(value);

          while (width && value.length < width) {
            value = ch + value;
          }

          return value;
        }

        // Format a value according to the selected flag and field width.
        function fmt(value, defaultWidth) {
          if (flag == '-')  // No padding.
            return value;

          if (flag == '^')  // Convert to uppercase.
            value = String(value).toUpperCase();

          if (typeof width == 'undefined')
            width = defaultWidth;

          // If there is no width specifier, there's nothing to pad.
          if (!width)
            return value;

          if (flag == '_')  // Pad with spaces.
            return lpad(value, ' ');

          // Autodetect padding character.
          if (typeof value == 'number')
            return lpad(value, '0');

          return lpad(value, ' ');
        }

        switch (code) {
          case '%': return '%';
          case 'a': return fmt(strings.dayShort[date.getDay()]);
          case 'b': return fmt(strings.monthShort[date.getMonth()]);
          case 'd': return fmt(date.getDate(), 2);
          case 'Y': return date.getFullYear();
          default:
            console.log('Unknown format specifier: ' + code);
            return m;
        }
      }

      // Conversion specifiers start with a '%', optionally contain a
      // flag and/or field width, followed by a single letter.
      // e.g. %a, %-d, %2l.
      return spec.replace(/%([\^\-_])?(\d+)?([%a-z])?/gi, replaceSpecifier);
    }
  };

  /**
   * Storage for the current cr.locale.
   */
  var locale = null;

  return {
    Locale: Locale,
    get locale() {
      return locale;
    },
    initLocale: function(strings) {
      locale = new Locale(strings);
    }
  };
});
