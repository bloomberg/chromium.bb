// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  'use strict';

  /**
   * Print destination data object that holds data for both local and cloud
   * destinations.
   * @param {string} id ID of the destination.
   * @param {!print_preview.Destination.Type} type Type of the destination.
   * @param {string} displayName Display name of the destination.
   * @param {boolean} isRecent Whether the destination has been used recently.
   * @param {!print_preview.Destination.ConnectionStatus} connectionStatus
   *     Connection status of the print destination.
   * @param {{tags: Array.<string>,
   *          isOwned: ?boolean,
   *          lastAccessTime: ?number,
   *          isTosAccepted: ?boolean}=} opt_params Optional parameters for the
   *     destination.
   * @constructor
   */
  function Destination(id, type, displayName, isRecent, connectionStatus,
                       opt_params) {
    /**
     * ID of the destination.
     * @type {string}
     * @private
     */
    this.id_ = id;

    /**
     * Type of the destination.
     * @type {!print_preview.Destination.Type}
     * @private
     */
    this.type_ = type;

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
     * Tags associated with the destination.
     * @type {!Array.<string>}
     * @private
     */
    this.tags_ = (opt_params && opt_params.tags) || [];

    /**
     * Print capabilities of the destination.
     * @type {print_preview.ChromiumCapabilities}
     * @private
     */
    this.capabilities_ = null;

    /**
     * Whether the destination is owned by the user.
     * @type {boolean}
     * @private
     */
    this.isOwned_ = (opt_params && opt_params.isOwned) || false;

    /**
     * Cache of destination location fetched from tags.
     * @type {?string}
     * @private
     */
    this.location_ = null;

    /**
     * Connection status of the destination.
     * @type {!print_preview.Destination.ConnectionStatus}
     * @private
     */
    this.connectionStatus_ = connectionStatus;

    /**
     * Number of milliseconds since the epoch when the printer was last
     * accessed.
     * @type {number}
     * @private
     */
    this.lastAccessTime_ = (opt_params && opt_params.lastAccessTime) ||
                           Date.now();

    /**
     * Whether the user has accepted the terms-of-service for the print
     * destination. Only applies to the FedEx Office cloud-based printer.
     * {@code} null if terms-of-service does not apply to the print destination.
     * @type {?boolean}
     * @private
     */
    this.isTosAccepted_ = (opt_params && opt_params.isTosAccepted) || false;
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
    FEDEX: '__google__fedex',
    SAVE_AS_PDF: 'Save as PDF'
  };

  /**
   * Enumeration of the types of destinations.
   * @enum {string}
   */
  Destination.Type = {
    GOOGLE: 'google',
    LOCAL: 'local',
    MOBILE: 'mobile'
  };

  /**
   * Enumeration of the connection statuses of printer destinations.
   * @enum {string}
   */
  Destination.ConnectionStatus = {
    DORMANT: 'DORMANT',
    OFFLINE: 'OFFLINE',
    ONLINE: 'ONLINE',
    UNKNOWN: 'UNKNOWN'
  };

  /**
   * Enumeration of relative icon URLs for various types of destinations.
   * @enum {string}
   * @private
   */
  Destination.IconUrl_ = {
    CLOUD: 'images/printer.png',
    CLOUD_SHARED: 'images/printer_shared.png',
    LOCAL: 'images/printer.png',
    MOBILE: 'images/mobile.png',
    MOBILE_SHARED: 'images/mobile_shared.png',
    THIRD_PARTY: 'images/third_party.png',
    PDF: 'images/pdf.png',
    DOCS: 'images/google_doc.png',
    FEDEX: 'images/third_party_fedex.png'
  };

  Destination.prototype = {
    /** @return {string} ID of the destination. */
    get id() {
      return this.id_;
    },

    /** @return {!print_preview.Destination.Type} Type of the destination. */
    get type() {
      return this.type_;
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

    /**
     * @return {boolean} Whether the user owns the destination. Only applies to
     *     cloud-based destinations.
     */
    get isOwned() {
      return this.isOwned_;
    },

    /** @return {boolean} Whether the destination is local or cloud-based. */
    get isLocal() {
      return this.type_ == Destination.Type.LOCAL;
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
                Destination.LOCATION_TAG_PREFIX.length) || '';
            break;
          }
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
     * @return {!print_preview.Destination.ConnectionStatus} Connection status
     *     of the print destination.
     */
    get connectionStatus() {
      return this.connectionStatus_;
    },

    /**
     * @param {!print_preview.Destination.ConnectionStatus} status Connection
     *     status of the print destination.
     */
    set connectionStatus(status) {
      this.connectionStatus_ = status;
    },

    /**
     * @return {number} Number of milliseconds since the epoch when the printer
     *     was last accessed.
     */
    get lastAccessTime() {
      return this.lastAccessTime_;
    },

    /** @return {string} Relative URL of the destination's icon. */
    get iconUrl() {
      if (this.id_ == Destination.GooglePromotedId.DOCS) {
        return Destination.IconUrl_.DOCS;
      } else if (this.id_ == Destination.GooglePromotedId.FEDEX) {
        return Destination.IconUrl_.FEDEX;
      } else if (this.id_ == Destination.GooglePromotedId.SAVE_AS_PDF) {
        return Destination.IconUrl_.PDF;
      } else if (this.isLocal) {
        return Destination.IconUrl_.LOCAL;
      } else if (this.type_ == Destination.Type.MOBILE && this.isOwned_) {
        return Destination.IconUrl_.MOBILE;
      } else if (this.type_ == Destination.Type.MOBILE) {
        return Destination.IconUrl_.MOBILE_SHARED;
      } else if (this.isOwned_) {
        return Destination.IconUrl_.CLOUD;
      } else {
        return Destination.IconUrl_.CLOUD_SHARED;
      }
    },

    /**
     * @return {?boolean} Whether the user has accepted the terms-of-service of
     *     the print destination or {@code null} if a terms-of-service does not
     *     apply.
     */
    get isTosAccepted() {
      return this.isTosAccepted_;
    },

    /**
     * @param {?boolean} Whether the user has accepted the terms-of-service of
     *     the print destination or {@code null} if a terms-of-service does not
     *     apply.
     */
    set isTosAccepted(isTosAccepted) {
      this.isTosAccepted_ = isTosAccepted;
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
