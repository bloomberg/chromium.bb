// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('cloudprint', function() {
  'use strict';

  /** Namespace which contains a method to parse cloud destinations directly. */
  function CloudDestinationParser() {};

  /**
   * Enumeration of cloud destination field names.
   * @enum {string}
   * @private
   */
  CloudDestinationParser.Field_ = {
    LAST_ACCESS: 'accessTime',
    CAPABILITIES: 'capabilities',
    CONNECTION_STATUS: 'connectionStatus',
    DISPLAY_NAME: 'displayName',
    FORMAT: 'capsFormat',
    ID: 'id',
    IS_TOS_ACCEPTED: 'isTosAccepted',
    TAGS: 'tags',
    TYPE: 'type'
  };

  /**
   * Special tag that denotes whether the destination has been recently used.
   * @type {string}
   * @const
   * @private
   */
  CloudDestinationParser.RECENT_TAG_ = '^recent';

  /**
   * Special tag that denotes whether the destination is owned by the user.
   * @type {string}
   * @const
   * @private
   */
  CloudDestinationParser.OWNED_TAG_ = '^own';

  /**
   * Enumeration of cloud destination types that are supported by print preview.
   * @enum {string}
   * @private
   */
  CloudDestinationParser.CloudType_ = {
    ANDROID: 'ANDROID_CHROME_SNAPSHOT',
    DOCS: 'DOCS',
    IOS: 'IOS_CHROME_SNAPSHOT'
  };

  /**
   * Parses a destination from JSON from a Google Cloud Print search or printer
   * response.
   * @param {!Object} json Object that represents a Google Cloud Print search or
   *     printer response.
   * @param {!print_preview.Destination.AuthType} authType The authentication
   *     type used to find printer.
   * @return {!print_preview.Destination} Parsed destination.
   */
  CloudDestinationParser.parse = function(json, authType) {
    if (!json.hasOwnProperty(CloudDestinationParser.Field_.ID) ||
        !json.hasOwnProperty(CloudDestinationParser.Field_.TYPE) ||
        !json.hasOwnProperty(CloudDestinationParser.Field_.DISPLAY_NAME)) {
      throw Error('Cloud destination does not have an ID or a display name');
    }
    var id = json[CloudDestinationParser.Field_.ID];
    var tags = json[CloudDestinationParser.Field_.TAGS] || [];
    var connectionStatus =
        json[CloudDestinationParser.Field_.CONNECTION_STATUS] ||
        print_preview.Destination.ConnectionStatus.UNKNOWN;
    var optionalParams = {
      tags: tags,
      isOwned: arrayContains(tags, CloudDestinationParser.OWNED_TAG_),
      lastAccessTime: parseInt(
          json[CloudDestinationParser.Field_.LAST_ACCESS], 10) || Date.now(),
      isTosAccepted: (id == print_preview.Destination.GooglePromotedId.FEDEX) ?
          json[CloudDestinationParser.Field_.IS_TOS_ACCEPTED] : null
    };
    var cloudDest = new print_preview.Destination(
        id,
        CloudDestinationParser.parseType_(
            json[CloudDestinationParser.Field_.TYPE]),
        authType,
        json[CloudDestinationParser.Field_.DISPLAY_NAME],
        arrayContains(tags, CloudDestinationParser.RECENT_TAG_) /*isRecent*/,
        connectionStatus,
        optionalParams);
    if (json.hasOwnProperty(CloudDestinationParser.Field_.CAPABILITIES) &&
        json.hasOwnProperty(CloudDestinationParser.Field_.FORMAT)) {
      cloudDest.capabilities = CloudCapabilitiesParser.parse(
          json[CloudDestinationParser.Field_.FORMAT],
          json[CloudDestinationParser.Field_.CAPABILITIES]);
    }
    return cloudDest;
  };

  /**
   * Parses the destination type.
   * @param {string} typeStr Destination type given by the Google Cloud Print
   *     server.
   * @return {!print_preview.Destination.Type} Destination type.
   * @private
   */
  CloudDestinationParser.parseType_ = function(typeStr) {
    if (typeStr == CloudDestinationParser.CloudType_.ANDROID ||
        typeStr == CloudDestinationParser.CloudType_.IOS) {
      return print_preview.Destination.Type.MOBILE;
    } else if (typeStr == CloudDestinationParser.CloudType_.DOCS) {
      return print_preview.Destination.Type.GOOGLE_PROMOTED;
    } else {
      return print_preview.Destination.Type.GOOGLE;
    }
  };

  /**
   * Namespace which contains a method to parse a cloud destination's print
   * capabilities.
   */
  function CloudCapabilitiesParser() {};

  /**
   * Enumeration of cloud destination print capabilities field names.
   * @enum {string}
   * @private
   */
  CloudCapabilitiesParser.Field_ = {
    CAP_ID: 'name',
    DEFAULT: 'default',
    IS_DEFAULT: 'default',
    OPTIONS: 'options',
    OPTION_ID: 'name'
  };

  /**
   * Parses print capabilities from an object in a given capabilities format.
   * @param {print_preview.CloudCapabilities.Format} capsFormat Format of the
   *     printer capabilities.
   * @param {!Array.<!Object>} json Object representing the cloud capabilities.
   * @return {!print_preview.CloudCapabilities} Parsed print capabilities.
   */
  CloudCapabilitiesParser.parse = function(capsFormat, json) {
    var colorCapability = null;
    var duplexCapability = null;
    var copiesCapability = null;
    var collateCapability = null;
    json.forEach(function(cap) {
      var capId = cap[CloudCapabilitiesParser.Field_.CAP_ID];
      if (capId == print_preview.CollateCapability.Id[capsFormat]) {
        collateCapability = CloudCapabilitiesParser.parseCollate(capId, cap);
      } else if (capId == print_preview.ColorCapability.Id[capsFormat]) {
        colorCapability = CloudCapabilitiesParser.parseColor(capId, cap);
      } else if (capId == print_preview.CopiesCapability.Id[capsFormat]) {
        copiesCapability = new print_preview.CopiesCapability(capId);
      } else if (capId == print_preview.DuplexCapability.Id[capsFormat]) {
        duplexCapability = CloudCapabilitiesParser.parseDuplex(capId, cap);
      }
    });
    return new print_preview.CloudCapabilities(
        collateCapability, colorCapability, copiesCapability, duplexCapability);
  };

  /**
   * Parses a collate capability from the given object.
   * @param {string} capId Native ID of the given capability object.
   * @param {!Object} Object that represents the collate capability.
   * @return {print_preview.CollateCapability} Parsed collate capability or
   *     {@code null} if the given capability object was not a valid collate
   *     capability.
   */
  CloudCapabilitiesParser.parseCollate = function(capId, cap) {
    var options = cap[CloudCapabilitiesParser.Field_.OPTIONS];
    var collateOption = null;
    var noCollateOption = null;
    var isCollateDefault = false;
    options.forEach(function(option) {
      var optionId = option[CloudCapabilitiesParser.Field_.OPTION_ID];
      if (!collateOption &&
          print_preview.CollateCapability.COLLATE_REGEX.test(optionId)) {
        collateOption = optionId;
        isCollateDefault = !!option[CloudCapabilitiesParser.Field_.DEFAULT];
      } else if (!noCollateOption &&
          print_preview.CollateCapability.NO_COLLATE_REGEX.test(optionId)) {
        noCollateOption = optionId;
      }
    });
    if (!collateOption || !noCollateOption) {
      return null;
    }
    return new print_preview.CollateCapability(
        capId, collateOption, noCollateOption, isCollateDefault);
  };

  /**
   * Parses a color capability from the given object.
   * @param {string} capId Native ID of the given capability object.
   * @param {!Object} Object that represents the color capability.
   * @return {print_preview.ColorCapability} Parsed color capability or
   *     {@code null} if the given capability object was not a valid color
   *     capability.
   */
  CloudCapabilitiesParser.parseColor = function(capId, cap) {
    var options = cap[CloudCapabilitiesParser.Field_.OPTIONS];
    var colorOption = null;
    var bwOption = null;
    var isColorDefault = false;
    options.forEach(function(option) {
      var optionId = option[CloudCapabilitiesParser.Field_.OPTION_ID];
      if (!colorOption &&
          print_preview.ColorCapability.COLOR_REGEX.test(optionId)) {
        colorOption = optionId;
        isColorDefault = !!option[CloudCapabilitiesParser.Field_.DEFAULT];
      } else if (!bwOption &&
          print_preview.ColorCapability.BW_REGEX.test(optionId)) {
        bwOption = optionId;
      }
    });
    if (!colorOption || !bwOption) {
      return null;
    }
    return new print_preview.ColorCapability(
        capId, colorOption, bwOption, isColorDefault);
  };

  /**
   * Parses a duplex capability from the given object.
   * @param {string} capId Native ID of the given capability object.
   * @param {!Object} Object that represents the duplex capability.
   * @return {print_preview.DuplexCapability} Parsed duplex capability or
   *     {@code null} if the given capability object was not a valid duplex
   *     capability.
   */
  CloudCapabilitiesParser.parseDuplex = function(capId, cap) {
    var options = cap[CloudCapabilitiesParser.Field_.OPTIONS];
    var simplexOption = null;
    var longEdgeOption = null;
    var isDuplexDefault = false;
    options.forEach(function(option) {
      var optionId = option[CloudCapabilitiesParser.Field_.OPTION_ID];
      if (!simplexOption &&
          print_preview.DuplexCapability.SIMPLEX_REGEX.test(optionId)) {
        simplexOption = optionId;
      } else if (!longEdgeOption &&
          print_preview.DuplexCapability.LONG_EDGE_REGEX.test(optionId)) {
        longEdgeOption = optionId;
        isDuplexDefault = !!option[CloudCapabilitiesParser.Field_.DEFAULT];
      }
    });
    if (!simplexOption || !longEdgeOption) {
      return null;
    }
    return new print_preview.DuplexCapability(
        capId, simplexOption, longEdgeOption, isDuplexDefault);
  };

  // Export
  return {
    CloudCapabilitiesParser: CloudCapabilitiesParser,
    CloudDestinationParser: CloudDestinationParser
  };
});

