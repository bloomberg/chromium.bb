// Copyright (c) 2009-2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(arv): Namespace and share code with DOMUI

/**
 * The local strings get injected into the page usig a varaible named
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
   * @param {string} s The id of the string we want.
   * @return {string} The localized string.
   */
  getString: function(id) {
    return this.templateData[id] || '';
  },

  /**
   * Returns a formatted localized string where all %s contents are replaced
   * by the second argument and where $1 to $9 are replaced by the second to
   * tenths arguments.
   * @param {string} id The ID of the string we want.
   * @param {string} v The string to include in the formatted string.
   * @param {...string} The extra values to include in the fomatted output.
   * @return {string} The formatted string.
   */
  getStringF: function(id, v, var_args) {
    // The localized messages should contain $n but they also use %s from time
    // to time so we support both until all the messages have been unified.
    var s = this.getString(id);
    var args = arguments;
    return s.replace(/%s|\$[$1-9]/g, function(m) {
      if (m == '%s')
        return v;
      if (m == '$$')
        return '$';
      return args[m[1]];
    });
  }
};
