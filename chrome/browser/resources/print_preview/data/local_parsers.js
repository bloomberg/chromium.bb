// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  'use strict';

  /** Namespace that contains a method to parse local print destinations. */
  function LocalDestinationParser() {};

  /**
   * Parses a local print destination.
   * @param {!Object} destinationInfo Information describing a local print
   *     destination.
   * @return {!print_preview.Destination} Parsed local print destination.
   */
  LocalDestinationParser.parse = function(destinationInfo) {
    return new print_preview.Destination(
        destinationInfo.deviceName,
        print_preview.Destination.Type.LOCAL,
        destinationInfo.printerName,
        false /*isRecent*/,
        print_preview.Destination.ConnectionStatus.ONLINE);
  };

  /** Namespace that contains a method to parse local print capabilities. */
  function LocalCapabilitiesParser() {};

  /**
   * Parses local print capabilities.
   * @param {!Object} settingsInfo Object that describes local print
   *     capabilities.
   * @return {!print_preview.ChromiumCapabilities} Parsed local print
   *     capabilities.
   */
  LocalCapabilitiesParser.parse = function(settingsInfo) {
    var hasColorCapability = !settingsInfo['disableColorOption'] || false;
    var defaultIsColorEnabled = settingsInfo['setColorAsDefault'] || false;

    var hasDuplexCapability = false;
    var defaultIsDuplexEnabled = false;
    // On Windows, some printers don't specify their duplex values in the
    // printer schema. If the printer duplex value is UNKNOWN_DUPLEX_MODE,
    // hide the two sided option in preview tab UI.
    // Ref bug: http://crbug.com/89204
    if (hasDuplexCapability =
        settingsInfo['printerDefaultDuplexValue'] !=
        print_preview.NativeLayer.DuplexMode.UNKNOWN_DUPLEX_MODE) {
      defaultIsDuplexEnabled = settingsInfo['setDuplexAsDefault'] || false;
    }

    return new print_preview.ChromiumCapabilities(
        !settingsInfo['disableCopiesOption'] /*hasCopiesCapability*/,
        '1' /*defaultCopiesStr*/,
        true /*hasCollateCapability*/,
        true /*defaultIsCollateEnabled*/,
        hasDuplexCapability,
        defaultIsDuplexEnabled,
        !settingsInfo['disableLandscapeOption'] /*hasOrientationCapability*/,
        false /*defaultIsLandscapeEnabled*/,
        hasColorCapability,
        defaultIsColorEnabled);
  };

  // Export
  return {
    LocalCapabilitiesParser: LocalCapabilitiesParser,
    LocalDestinationParser: LocalDestinationParser
  };
});
