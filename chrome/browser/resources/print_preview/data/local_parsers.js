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
        print_preview.Destination.Origin.LOCAL,
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
   * @return {!print_preview.Cdd} Parsed local print capabilities.
   */
  LocalCapabilitiesParser.parse = function(settingsInfo) {
    var cdd = {
      version: '1.0',
      printer: {
        collate: {default: true}
      }
    };

    if (!settingsInfo['disableColorOption']) {
      cdd.printer.color = {
        option: [
          {
            type: 'STANDARD_COLOR',
            is_default: !!settingsInfo['setColorAsDefault']
          },
          {
            type: 'STANDARD_MONOCHROME',
            is_default: !settingsInfo['setColorAsDefault']
          }
        ]
      }
    }

    if (!settingsInfo['disableCopiesOption']) {
      cdd.printer.copies = {default: 1};
    }

    if (settingsInfo['printerDefaultDuplexValue'] !=
        print_preview.NativeLayer.DuplexMode.UNKNOWN_DUPLEX_MODE) {
      cdd.printer.duplex = {
        option: [
          {type: 'NO_DUPLEX', is_default: !settingsInfo['setDuplexAsDefault']},
          {type: 'LONG_EDGE', is_default: !!settingsInfo['setDuplexAsDefault']}
        ]
      };
    }

    if (!settingsInfo['disableLandscapeOption']) {
      cdd.printer.page_orientation = {
        option: [
          {type: 'PORTRAIT', is_default: true},
          {type: 'LANDSCAPE'}
        ]
      };
    }

    return cdd;
  };

  // Export
  return {
    LocalCapabilitiesParser: LocalCapabilitiesParser,
    LocalDestinationParser: LocalDestinationParser
  };
});
