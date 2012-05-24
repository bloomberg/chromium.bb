// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  'use strict';

  /**
   * Print destination data object that holds data for both local and cloud
   * destinations.
   * @param {string} id ID of the destination.
   * @param {string} displayName Display name of the destination.
   * @param {boolean} isRecent Whether the destination has been used recently.
   * @param {boolean} isLocal Whether the destination is local or cloud-based.
   * @param {Array.<string>=} opt_tags Tags associated with the destination.
   * @constructor
   */
  function Destination(id, displayName, isRecent, isLocal, opt_tags) {
    /**
     * ID of the destination.
     * @type {string}
     * @private
     */
    this.id_ = id;

    /**
     * Display name of the destination.
     * @type {string}
     * @private
     */
    this.displayName_ = displayName;

    /**
     * Whether the destination has been used recently.
     * @type {boolean}
     * @private
     */
    this.isRecent_ = isRecent;

    /**
     * Whether the destination is local or cloud-based.
     * @type {boolean}
     * @private
     */
    this.isLocal_ = isLocal;

    /**
     * Tags associated with the destination.
     * @type {!Array.<string>}
     * @private
     */
    this.tags_ = opt_tags || [];

    /**
     * Print capabilities of the destination.
     * @type {print_preview.ChromiumCapabilities}
     * @private
     */
    this.capabilities_ = null;

    /**
     * Cache of destination location fetched from tags.
     * @type {string}
     * @private
     */
    this.location_ = null;
  };

  /**
   * Prefix of the location destination tag.
   * @type {string}
   * @const
   */
  Destination.LOCATION_TAG_PREFIX = '__cp__printer-location=';

  /**
   * Enumeration of Google-promoted destination IDs.
   * @enum {string}
   */
  Destination.GooglePromotedId = {
    DOCS: '__google__docs',
    SAVE_AS_PDF: 'Save as PDF',
    PRINT_WITH_CLOUD_PRINT: 'printWithCloudPrint'
  };

  Destination.prototype = {
    /** @return {string} ID of the destination. */
    get id() {
      return this.id_;
    },

    /** @return {string} Display name of the destination. */
    get displayName() {
      return this.displayName_;
    },

    /** @return {boolean} Whether the destination has been used recently. */
    get isRecent() {
      return this.isRecent_;
    },

    /**
     * @param {boolean} isRecent Whether the destination has been used recently.
     */
    set isRecent(isRecent) {
      this.isRecent_ = isRecent;
    },

    /** @return {boolean} Whether the destination is local or cloud-based. */
    get isLocal() {
      return this.isLocal_;
    },

    /** @return {boolean} Whether the destination is promoted by Google. */
    get isGooglePromoted() {
      for (var key in Destination.GooglePromotedId) {
        if (Destination.GooglePromotedId[key] == this.id_) {
          return true;
        }
      }
      return false;
    },

    /**
     * @return {boolean} Whether the destination is the "Print with Cloud Print"
     *     destination.
     */
    get isPrintWithCloudPrint() {
      return this.id_ == Destination.GooglePromotedId.PRINT_WITH_CLOUD_PRINT;
    },

    /**
     * @return {string} The location of the destination, or an empty string if
     *     the location is unknown.
     */
    get location() {
      if (this.location_ == null) {
        for (var tag, i = 0; tag = this.tags_[i]; i++) {
          if (tag.indexOf(Destination.LOCATION_TAG_PREFIX) == 0) {
            this.location_ = tag.substring(
                Destination.LOCATION_TAG_PREFIX.length);
          }
        }
        if (this.location_ == null) {
          this.location_ = '';
        }
      }
      return this.location_;
    },

    /** @return {!Array.<string>} Tags associated with the destination. */
    get tags() {
      return this.tags_.slice(0);
    },

    /**
     * @return {print_preview.ChromiumCapabilities} Print capabilities of the
     *     destination.
     */
    get capabilities() {
      return this.capabilities_;
    },

    /**
     * @param {!print_preview.ChromiumCapabilities} capabilities Print
     *     capabilities of the destination.
     */
    set capabilities(capabilities) {
      this.capabilities_ = capabilities;
    },

    /**
     * Matches a query against the destination.
     * @param {string} query Query to match against the destination.
     * @return {boolean} {@code true} if the query matches this destination,
     *     {@code false} otherwise.
     */
    matches: function(query) {
      return this.displayName_.toLowerCase().indexOf(
          query.toLowerCase().trim()) != -1;
    }
  };

  // Export
  return {
    Destination: Destination
  };
});
