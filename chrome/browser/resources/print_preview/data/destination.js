// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.exportPath('print_preview');

/**
 * Enumeration of the types of destinations.
 * @enum {string}
 */
print_preview.DestinationType = {
  GOOGLE: 'google',
  GOOGLE_PROMOTED: 'google_promoted',
  LOCAL: 'local',
  MOBILE: 'mobile'
};

/**
 * Enumeration of the origin types for cloud destinations.
 * @enum {string}
 */
print_preview.DestinationOrigin = {
  LOCAL: 'local',
  COOKIES: 'cookies',
  DEVICE: 'device',
  PRIVET: 'privet',
  EXTENSION: 'extension',
  CROS: 'chrome_os',
};

/**
 * Enumeration of the connection statuses of printer destinations.
 * @enum {string}
 */
print_preview.DestinationConnectionStatus = {
  DORMANT: 'DORMANT',
  OFFLINE: 'OFFLINE',
  ONLINE: 'ONLINE',
  UNKNOWN: 'UNKNOWN',
  UNREGISTERED: 'UNREGISTERED'
};

/**
 * Enumeration specifying whether a destination is provisional and the reason
 * the destination is provisional.
 * @enum {string}
 */
print_preview.DestinationProvisionalType = {
  /** Destination is not provisional. */
  NONE: 'NONE',
  /**
   * User has to grant USB access for the destination to its provider.
   * Used for destinations with extension origin.
   */
  NEEDS_USB_PERMISSION: 'NEEDS_USB_PERMISSION'
};

/**
 * The CDD (Cloud Device Description) describes the capabilities of a print
 * destination.
 *
 * @typedef {{
 *   version: string,
 *   printer: {
 *     vendor_capability: !Array<{Object}>,
 *     collate: ({default: (boolean|undefined)}|undefined),
 *     color: ({
 *       option: !Array<{
 *         type: (string|undefined),
 *         vendor_id: (string|undefined),
 *         custom_display_name: (string|undefined),
 *         is_default: (boolean|undefined)
 *       }>
 *     }|undefined),
 *     copies: ({default: (number|undefined),
 *               max: (number|undefined)}|undefined),
 *     duplex: ({option: !Array<{type: (string|undefined),
 *                               is_default: (boolean|undefined)}>}|undefined),
 *     page_orientation: ({
 *       option: !Array<{type: (string|undefined),
 *                        is_default: (boolean|undefined)}>
 *     }|undefined),
 *     media_size: ({
 *       option: !Array<{
 *         type: (string|undefined),
 *         vendor_id: (string|undefined),
 *         custom_display_name: (string|undefined),
 *         is_default: (boolean|undefined)
 *       }>
 *     }|undefined)
 *   }
 * }}
 */
print_preview.Cdd;

cr.define('print_preview', function() {
  'use strict';

  /**
   * Print destination data object that holds data for both local and cloud
   * destinations.
   * @param {string} id ID of the destination.
   * @param {!print_preview.DestinationType} type Type of the destination.
   * @param {!print_preview.DestinationOrigin} origin Origin of the
   *     destination.
   * @param {string} displayName Display name of the destination.
   * @param {boolean} isRecent Whether the destination has been used recently.
   * @param {!print_preview.DestinationConnectionStatus} connectionStatus
   *     Connection status of the print destination.
   * @param {{tags: (Array<string>|undefined),
   *          isOwned: (boolean|undefined),
   *          isEnterprisePrinter: (boolean|undefined),
   *          account: (string|undefined),
   *          lastAccessTime: (number|undefined),
   *          cloudID: (string|undefined),
   *          provisionalType:
   *              (print_preview.DestinationProvisionalType|undefined),
   *          extensionId: (string|undefined),
   *          extensionName: (string|undefined),
   *          description: (string|undefined)}=} opt_params Optional parameters
   *     for the destination.
   * @constructor
   */
  function Destination(
      id, type, origin, displayName, isRecent, connectionStatus, opt_params) {
    /**
     * ID of the destination.
     * @private {string}
     */
    this.id_ = id;

    /**
     * Type of the destination.
     * @private {!print_preview.DestinationType}
     */
    this.type_ = type;

    /**
     * Origin of the destination.
     * @private {!print_preview.DestinationOrigin}
     */
    this.origin_ = origin;

    /**
     * Display name of the destination.
     * @private {string}
     */
    this.displayName_ = displayName || '';

    /**
     * Whether the destination has been used recently.
     * @private {boolean}
     */
    this.isRecent_ = isRecent;

    /**
     * Tags associated with the destination.
     * @private {!Array<string>}
     */
    this.tags_ = (opt_params && opt_params.tags) || [];

    /**
     * Print capabilities of the destination.
     * @private {?print_preview.Cdd}
     */
    this.capabilities_ = null;

    /**
     * Whether the destination is owned by the user.
     * @private {boolean}
     */
    this.isOwned_ = (opt_params && opt_params.isOwned) || false;

    /**
     * Whether the destination is an enterprise policy controlled printer.
     * @private {boolean}
     */
    this.isEnterprisePrinter_ =
        (opt_params && opt_params.isEnterprisePrinter) || false;

    /**
     * Account this destination is registered for, if known.
     * @private {string}
     */
    this.account_ = (opt_params && opt_params.account) || '';

    /**
     * Cache of destination location fetched from tags.
     * @private {?string}
     */
    this.location_ = null;

    /**
     * Printer description.
     * @private {string}
     */
    this.description_ = (opt_params && opt_params.description) || '';

    /**
     * Connection status of the destination.
     * @private {!print_preview.DestinationConnectionStatus}
     */
    this.connectionStatus_ = connectionStatus;

    /**
     * Number of milliseconds since the epoch when the printer was last
     * accessed.
     * @private {number}
     */
    this.lastAccessTime_ =
        (opt_params && opt_params.lastAccessTime) || Date.now();

    /**
     * Cloud ID for Privet printers.
     * @private {string}
     */
    this.cloudID_ = (opt_params && opt_params.cloudID) || '';

    /**
     * Extension ID for extension managed printers.
     * @private {string}
     */
    this.extensionId_ = (opt_params && opt_params.extensionId) || '';

    /**
     * Extension name for extension managed printers.
     * @private {string}
     */
    this.extensionName_ = (opt_params && opt_params.extensionName) || '';

    /**
     * Different from {@code print_preview.DestinationProvisionalType.NONE} if
     * the destination is provisional. Provisional destinations cannot be
     * selected as they are, but have to be resolved first (i.e. extra steps
     * have to be taken to get actual destination properties, which should
     * replace the provisional ones). Provisional destination resolvment flow
     * will be started when the user attempts to select the destination in
     * search UI.
     * @private {print_preview.DestinationProvisionalType}
     */
    this.provisionalType_ = (opt_params && opt_params.provisionalType) ||
        print_preview.DestinationProvisionalType.NONE;

    assert(
        this.provisionalType_ !=
                print_preview.DestinationProvisionalType.NEEDS_USB_PERMISSION ||
            this.isExtension,
        'Provisional USB destination only supprted with extension origin.');
  }

  /**
   * Prefix of the location destination tag.
   * @type {!Array<string>}
   * @const
   */
  Destination.LOCATION_TAG_PREFIXES =
      ['__cp__location=', '__cp__printer-location='];

  /**
   * Enumeration of Google-promoted destination IDs.
   * @enum {string}
   */
  Destination.GooglePromotedId = {
    DOCS: '__google__docs',
    SAVE_AS_PDF: 'Save as PDF'
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
    ENTERPRISE: 'images/business.svg'
  };

  Destination.prototype = {
    /** @return {string} ID of the destination. */
    get id() {
      return this.id_;
    },

    /** @return {!print_preview.DestinationType} Type of the destination. */
    get type() {
      return this.type_;
    },

    /**
     * @return {!print_preview.DestinationOrigin} Origin of the destination.
     */
    get origin() {
      return this.origin_;
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

    /**
     * @return {string} Account this destination is registered for, if known.
     */
    get account() {
      return this.account_;
    },

    /** @return {boolean} Whether the destination is local or cloud-based. */
    get isLocal() {
      return this.origin_ == print_preview.DestinationOrigin.LOCAL ||
          this.origin_ == print_preview.DestinationOrigin.EXTENSION ||
          this.origin_ == print_preview.DestinationOrigin.CROS ||
          (this.origin_ == print_preview.DestinationOrigin.PRIVET &&
           this.connectionStatus_ !=
               print_preview.DestinationConnectionStatus.UNREGISTERED);
    },

    /** @return {boolean} Whether the destination is a Privet local printer */
    get isPrivet() {
      return this.origin_ == print_preview.DestinationOrigin.PRIVET;
    },

    /**
     * @return {boolean} Whether the destination is an extension managed
     *     printer.
     */
    get isExtension() {
      return this.origin_ == print_preview.DestinationOrigin.EXTENSION;
    },

    /**
     * @return {string} The location of the destination, or an empty string if
     *     the location is unknown.
     */
    get location() {
      if (this.location_ == null) {
        this.location_ = '';
        this.tags_.some(function(tag) {
          return Destination.LOCATION_TAG_PREFIXES.some(function(prefix) {
            if (tag.startsWith(prefix)) {
              this.location_ = tag.substring(prefix.length) || '';
              return true;
            }
          }, this);
        }, this);
      }
      return this.location_;
    },

    /**
     * @return {string} The description of the destination, or an empty string,
     *     if it was not provided.
     */
    get description() {
      return this.description_;
    },

    /**
     * @return {string} Most relevant string to help user to identify this
     *     destination.
     */
    get hint() {
      if (this.id_ == Destination.GooglePromotedId.DOCS) {
        return this.account_;
      }
      return this.location || this.extensionName || this.description;
    },

    /** @return {!Array<string>} Tags associated with the destination. */
    get tags() {
      return this.tags_.slice(0);
    },

    /** @return {string} Cloud ID associated with the destination */
    get cloudID() {
      return this.cloudID_;
    },

    /**
     * @return {string} Extension ID associated with the destination. Non-empty
     *     only for extension managed printers.
     */
    get extensionId() {
      return this.extensionId_;
    },

    /**
     * @return {string} Extension name associated with the destination.
     *     Non-empty only for extension managed printers.
     */
    get extensionName() {
      return this.extensionName_;
    },

    /** @return {?print_preview.Cdd} Print capabilities of the destination. */
    get capabilities() {
      return this.capabilities_;
    },

    /**
     * @param {!print_preview.Cdd} capabilities Print capabilities of the
     *     destination.
     */
    set capabilities(capabilities) {
      this.capabilities_ = capabilities;
    },

    /**
     * @return {!print_preview.DestinationConnectionStatus} Connection status
     *     of the print destination.
     */
    get connectionStatus() {
      return this.connectionStatus_;
    },

    /**
     * @param {!print_preview.DestinationConnectionStatus} status Connection
     *     status of the print destination.
     */
    set connectionStatus(status) {
      this.connectionStatus_ = status;
    },

    /** @return {boolean} Whether the destination is considered offline. */
    get isOffline() {
      return arrayContains(
          [
            print_preview.DestinationConnectionStatus.OFFLINE,
            print_preview.DestinationConnectionStatus.DORMANT
          ],
          this.connectionStatus_);
    },

    /** @return {string} Human readable status for offline destination. */
    get offlineStatusText() {
      if (!this.isOffline) {
        return '';
      }
      var offlineDurationMs = Date.now() - this.lastAccessTime_;
      var offlineMessageId;
      if (offlineDurationMs > 31622400000.0) {  // One year.
        offlineMessageId = 'offlineForYear';
      } else if (offlineDurationMs > 2678400000.0) {  // One month.
        offlineMessageId = 'offlineForMonth';
      } else if (offlineDurationMs > 604800000.0) {  // One week.
        offlineMessageId = 'offlineForWeek';
      } else {
        offlineMessageId = 'offline';
      }
      return loadTimeData.getString(offlineMessageId);
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
      }
      if (this.id_ == Destination.GooglePromotedId.SAVE_AS_PDF) {
        return Destination.IconUrl_.PDF;
      }
      if (this.isEnterprisePrinter) {
        return Destination.IconUrl_.ENTERPRISE;
      }
      if (this.isLocal) {
        return Destination.IconUrl_.LOCAL;
      }
      if (this.type_ == print_preview.DestinationType.MOBILE && this.isOwned_) {
        return Destination.IconUrl_.MOBILE;
      }
      if (this.type_ == print_preview.DestinationType.MOBILE) {
        return Destination.IconUrl_.MOBILE_SHARED;
      }
      if (this.isOwned_) {
        return Destination.IconUrl_.CLOUD;
      }
      return Destination.IconUrl_.CLOUD_SHARED;
    },

    /**
     * @return {!Array<string>} Properties (besides display name) to match
     *     search queries against.
     */
    get extraPropertiesToMatch() {
      return [this.location, this.description];
    },

    /**
     * Matches a query against the destination.
     * @param {!RegExp} query Query to match against the destination.
     * @return {boolean} {@code true} if the query matches this destination,
     *     {@code false} otherwise.
     */
    matches: function(query) {
      return !!this.displayName_.match(query) ||
          !!this.extensionName_.match(query) ||
          this.extraPropertiesToMatch.some(function(property) {
            return property.match(query);
          });
    },

    /**
     * Gets the destination's provisional type.
     * @return {print_preview.DestinationProvisionalType}
     */
    get provisionalType() {
      return this.provisionalType_;
    },

    /**
     * Whether the destinaion is provisional.
     * @return {boolean}
     */
    get isProvisional() {
      return this.provisionalType_ !=
          print_preview.DestinationProvisionalType.NONE;
    },

    /**
     * Whether the printer is enterprise policy controlled printer.
     * @return {boolean}
     */
    get isEnterprisePrinter() {
      return this.isEnterprisePrinter_;
    },
  };

  // Export
  return {
    Destination: Destination,
  };
});
