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
    CAPABILITIES: 'capabilities',
    DISPLAY_NAME: 'displayName',
    FORMAT: 'capsFormat',
    ID: 'id',
    TAGS: 'tags'
  };

  /**
   * Special tag that denotes whether the destination has been recently used.
   * @type {string}
   * @private
   */
  CloudDestinationParser.RECENT_TAG_ = '^recent';

  /**
   * Parses a destination from JSON from a Google Cloud Print search or printer
   * response.
   * @param {object} json Object that represents a Google Cloud Print search or
   *     printer response.
   * @return {!print_preview.Destination} Parsed destination.
   */
  CloudDestinationParser.parse = function(json) {
    if (!json.hasOwnProperty(CloudDestinationParser.Field_.ID) ||
        !json.hasOwnProperty(CloudDestinationParser.Field_.DISPLAY_NAME)) {
      throw Error('Cloud destination does not have an ID or a display name');
    }
    var isRecent = arrayContains(
        json[CloudDestinationParser.Field_.TAGS] || [],
        CloudDestinationParser.RECENT_TAG_);
    var cloudDest = new print_preview.Destination(
        json[CloudDestinationParser.Field_.ID],
        json[CloudDestinationParser.Field_.DISPLAY_NAME],
        isRecent,
        false /*isLocal*/,
        json[CloudDestinationParser.Field_.TAGS] || []);
    if (json.hasOwnProperty(CloudDestinationParser.Field_.CAPABILITIES) &&
        json.hasOwnProperty(CloudDestinationParser.Field_.FORMAT)) {
      cloudDest.capabilities = CloudCapabilitiesParser.parse(
          json[CloudDestinationParser.Field_.FORMAT],
          json[CloudDestinationParser.Field_.CAPABILITIES]);
    }
    return cloudDest;
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
   * @param {object} json Object representing the cloud capabilities.
   * @return {!print_preview.CloudCapabilities} Parsed print capabilities.
   */
  CloudCapabilitiesParser.parse = function(capsFormat, json) {
    var colorCapability = null;
    var duplexCapability = null;
    var copiesCapability = null;
    var collateCapability = null;
    for (var cap, i = 0; cap = json[i]; i++) {
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
    }
    return new print_preview.CloudCapabilities(
        collateCapability, colorCapability, copiesCapability, duplexCapability);
  };

  /**
   * Parses a collate capability from the given object.
   * @param {string} capId Native ID of the given capability object.
   * @param {object} Object that represents the collate capability.
   * @return {print_preview.CollateCapability} Parsed collate capability or
   *     {@code null} if the given capability object was not a valid collate
   *     capability.
   */
  CloudCapabilitiesParser.parseCollate = function(capId, cap) {
    var options = cap[CloudCapabilitiesParser.Field_.OPTIONS];
    var collateOption = null;
    var noCollateOption = null;
    var isCollateDefault = false;
    for (var option, i = 0; option = options[i]; i++) {
      var optionId = option[CloudCapabilitiesParser.Field_.OPTION_ID];
      if (!collateOption &&
          print_preview.CollateCapability.COLLATE_REGEX.test(optionId)) {
        collateOption = optionId;
        isCollateDefault = !!option[CloudCapabilitiesParser.Field_.DEFAULT];
      } else if (!noCollateOption &&
          print_preview.CollateCapability.NO_COLLATE_REGEX.test(optionId)) {
        noCollateOption = optionId;
      }
    }
    if (!collateOption || !noCollateOption) {
      return null;
    }
    return new print_preview.CollateCapability(
        capId, collateOption, noCollateOption, isCollateDefault);
  };

  /**
   * Parses a color capability from the given object.
   * @param {string} capId Native ID of the given capability object.
   * @param {object} Object that represents the color capability.
   * @return {print_preview.ColorCapability} Parsed color capability or
   *     {@code null} if the given capability object was not a valid color
   *     capability.
   */
  CloudCapabilitiesParser.parseColor = function(capId, cap) {
    var options = cap[CloudCapabilitiesParser.Field_.OPTIONS];
    var colorOption = null;
    var bwOption = null;
    var isColorDefault = false;
    for (var option, i = 0; option = options[i]; i++) {
      var optionId = option[CloudCapabilitiesParser.Field_.OPTION_ID];
      if (!colorOption &&
          print_preview.ColorCapability.COLOR_REGEX.test(optionId)) {
        colorOption = optionId;
        isColorDefault = !!option[CloudCapabilitiesParser.Field_.DEFAULT];
      } else if (!bwOption &&
          print_preview.ColorCapability.BW_REGEX.test(optionId)) {
        bwOption = optionId;
      }
    }
    if (!colorOption || !bwOption) {
      return null;
    }
    return new print_preview.ColorCapability(
        capId, colorOption, bwOption, isColorDefault);
  };

  /**
   * Parses a duplex capability from the given object.
   * @param {string} capId Native ID of the given capability object.
   * @param {object} Object that represents the duplex capability.
   * @return {print_preview.DuplexCapability} Parsed duplex capability or
   *     {@code null} if the given capability object was not a valid duplex
   *     capability.
   */
  CloudCapabilitiesParser.parseDuplex = function(capId, cap) {
    var options = cap[CloudCapabilitiesParser.Field_.OPTIONS];
    var simplexOption = null;
    var longEdgeOption = null;
    var isDuplexDefault = false;
    for (var option, i = 0; option = options[i]; i++) {
      var optionId = option[CloudCapabilitiesParser.Field_.OPTION_ID];
      if (!simplexOption &&
          print_preview.DuplexCapability.SIMPLEX_REGEX.test(optionId)) {
        simplexOption = optionId;
      } else if (!longEdgeOption &&
          print_preview.DuplexCapability.LONG_EDGE_REGEX.test(optionId)) {
        longEdgeOption = optionId;
        isDuplexDefault = !!option[CloudCapabilitiesParser.Field_.DEFAULT];
      }
    }
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

