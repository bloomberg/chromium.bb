// Copyright (c) 2009-2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * The local strings get injected into the page usig a variable named
 * {@code templateData}. This class provides a simpler interface to access those
 * strings.
 * @constructor
 */
function LocalStrings() {
}

LocalStrings.prototype = {
  /**
   * The template data object.
   * @type {Object}
   */
  templateData: null,

  /**
   * Gets a localized string by its id.
   * @param {string} s The ID of the string we want.
   * @return {string} The localized string.
   */
  getString: function(id) {
    // TODO(arv): We should not rely on a global variable here.
    return (this.templateData || window.templateData)[id] || '';
  },

  /**
   * Returns a formatted localized string where $1 to $9 are replaced by the
   * second to the tenth argument.
   * @param {string} id The ID of the string we want.
   * @param {...string} The extra values to include in the formatted output.
   * @return {string} The formatted string.
   */
  getStringF: function(id, var_args) {
    var s = this.getString(id);
    var args = arguments;
    return s.replace(/\$[$1-9]/g, function(m) {
      if (m == '$$')
        return '$';
      return args[m[1]];
    });
  }
};
