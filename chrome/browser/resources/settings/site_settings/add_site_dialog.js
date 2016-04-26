// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'add-site-dialog' provides a dialog to add exceptions for a given Content
 * Settings category.
 */
Polymer({
  is: 'add-site-dialog',

  behaviors: [SiteSettingsBehavior],

  properties: {
    /**
     * The site to add an exception for.
     * @private
     */
    site_: String,
  },

  /** Opens the dialog. */
  open: function() {
    this.$.dialog.open();
  },

  /**
   * Validates that the pattern entered is valid.
   * @private
   */
  validate_: function() {
    var pattern = this.addPatternWildcard_(this.site_);
    this.browserProxy.isPatternValid(pattern).then(function(isValid) {
      this.$.add.disabled = !isValid;
    }.bind(this));
  },

  /**
   * Adds the wildcard prefix to a pattern string.
   * @param {string} pattern The pattern to add the wildcard to.
   * @return {string} The resulting pattern.
   * @private
   */
  addPatternWildcard_: function(pattern) {
    if (pattern.startsWith('http://'))
      return pattern.replace('http://', 'http://[*.]');
    else if (pattern.startsWith('https://'))
      return pattern.replace('https://', 'https://[*.]');
    else
      return '[*.]' + pattern;
  },

  /**
   * The tap handler for the Add [Site] button (adds the pattern and closes
   * the dialog).
   * @private
   */
  onAddTap_: function() {
    var pattern = this.addPatternWildcard_(this.site_);
    this.setCategoryPermissionForOrigin(
        pattern, pattern, this.category, settings.PermissionValues.ALLOW);
    this.$.dialog.close();
  },
});
