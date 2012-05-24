// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  'use strict';

  /**
   * Capabilities of a print destination not including the capabilities of the
   * document renderer.
   * @param {boolean} hasCopiesCapability Whether the print destination has a
   *     copies capability.
   * @param {string} defaultCopiesStr Default string representation of the
   *     copies value.
   * @param {boolean} hasCollateCapability Whether the print destination has
   *     collation capability.
   * @param {boolean} defaultIsCollateEnabled Whether collate is enabled by
   *     default.
   * @param {boolean} hasDuplexCapability Whether the print destination has
   *     duplexing capability.
   * @param {boolean} defaultIsDuplexEnabled Whether duplexing is enabled by
   *     default.
   * @param {boolean} hasOrientationCapability Whether the print destination has
   *     orientation capability.
   * @param {boolean} defaultIsLandscapeEnabled Whether the document should be
   *     printed in landscape by default.
   * @param {boolean} hasColorCapability Whether the print destination has
   *     color printing capability.
   * @param {boolean} defaultIsColorEnabled Whether the document should be
   *     printed in color by default.
   * @constructor
   */
  function ChromiumCapabilities(
      hasCopiesCapability,
      defaultCopiesStr,
      hasCollateCapability,
      defaultIsCollateEnabled,
      hasDuplexCapability,
      defaultIsDuplexEnabled,
      hasOrientationCapability,
      defaultIsLandscapeEnabled,
      hasColorCapability,
      defaultIsColorEnabled) {
    /**
     * Whether the print destination has a copies capability.
     * @type {boolean}
     * @private
     */
    this.hasCopiesCapability_ = hasCopiesCapability;

    /**
     * Default string representation of the copies value.
     * @type {string}
     * @private
     */
    this.defaultCopiesStr_ = defaultCopiesStr;

    /**
     * Whether the print destination has collation capability.
     * @type {boolean}
     * @private
     */
    this.hasCollateCapability_ = hasCollateCapability;

    /**
     * Whether collate is enabled by default.
     * @type {boolean}
     * @private
     */
    this.defaultIsCollateEnabled_ = defaultIsCollateEnabled;

    /**
     * Whether the print destination has duplexing capability.
     * @type {boolean}
     * @private
     */
    this.hasDuplexCapability_ = hasDuplexCapability;

    /**
     * Whether duplex is enabled by default.
     * @type {boolean}
     * @private
     */
    this.defaultIsDuplexEnabled_ = defaultIsDuplexEnabled;

    /**
     * Whether the print destination has orientation capability.
     * @type {boolean}
     * @private
     */
    this.hasOrientationCapability_ = hasOrientationCapability;

    /**
     * Whether the document should be printed in landscape by default.
     * @type {boolean}
     * @private
     */
    this.defaultIsLandscapeEnabled_ = defaultIsLandscapeEnabled;

    /**
     * Whether the print destination has color printing capability.
     * @type {boolean}
     * @private
     */
    this.hasColorCapability_ = hasColorCapability;

    /**
     * Whether the document should be printed in color.
     * @type {boolean}
     * @private
     */
    this.defaultIsColorEnabled_ = defaultIsColorEnabled;
  };

  ChromiumCapabilities.prototype = {
    /** @return {boolean} Whether the destination has the copies capability. */
    get hasCopiesCapability() {
      return this.hasCopiesCapability_;
    },

    /** @return {string} Default number of copies in string format. */
    get defaultCopiesStr() {
      return this.defaultCopiesStr_;
    },

    /** @return {boolean} Whether the destination has collation capability. */
    get hasCollateCapability() {
      return this.hasCollateCapability_;
    },

    /** @return {boolean} Whether collation is enabled by default. */
    get defaultIsCollateEnabled() {
      return this.defaultIsCollateEnabled_;
    },

    /** @return {boolean} Whether the destination has the duplex capability. */
    get hasDuplexCapability() {
      return this.hasDuplexCapability_;
    },

    /** @return {boolean} Whether duplexing is enabled by default. */
    get defaultIsDuplexEnabled() {
      return this.defaultIsDuplexEnabled_;
    },

    /**
     * @return {boolean} Whether the destination has the orientation capability.
     */
    get hasOrientationCapability() {
      return this.hasOrientationCapability_;
    },

    /**
     * @return {boolean} Whether document should be printed in landscape by
     *     default.
     */
    get defaultIsLandscapeEnabled() {
      return this.defaultIsLandscapeEnabled_;
    },

    /**
     * @return {boolean} Whether the destination has color printing capability.
     */
    get hasColorCapability() {
      return this.hasColorCapability_;
    },

    /**
     * @return {boolean} Whether document should be printed in color by default.
     */
    get defaultIsColorEnabled() {
      return this.defaultIsColorEnabled_;
    }
  };

  // Export
  return {
    ChromiumCapabilities: ChromiumCapabilities
  };
});
