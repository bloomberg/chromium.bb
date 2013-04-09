// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  'use strict';

  /**
   * Capabilities of a cloud-based print destination.
   * @param {!Object} cdd Cloud Device Description of print destination.
   * @constructor
   * @extends {print_preview.ChromiumCapabilities}
   */
  function CloudCapabilities(cdd) {
    print_preview.ChromiumCapabilities.call(
        this,
        cdd.printer && cdd.printer.copies,
        cdd.printer && cdd.printer.copies && cdd.printer.copies['default'] + '',
        cdd.printer && cdd.printer.collate,
        cdd.printer && cdd.printer.collate && cdd.printer.collate['default'],
        CloudCapabilities.hasDuplexCapability_(cdd),
        CloudCapabilities.hasDuplexCapability_(cdd) &&
            CloudCapabilities.isLongEdgeDefault_(cdd.printer.duplex),
        true /*hasOrientationCapability*/,
        false /*defaultIsLandscapeEnabled*/,
        CloudCapabilities.hasColorCapability_(cdd),
        CloudCapabilities.hasColorCapability_(cdd) &&
            CloudCapabilities.isColorDefault_(cdd.printer.color));

    /**
     * Cloud Device Description of the print destination.
     * @type {!Object}
     * @private
     */
    this.cdd_ = cdd;
  };

  CloudCapabilities.ColorType_ = {
    STANDARD_COLOR: 'STANDARD_COLOR',
    STANDARD_MONOCHROME: 'STANDARD_MONOCHROME'
  };

  CloudCapabilities.DuplexType_ = {
    NO_DUPLEX: 'NO_DUPLEX',
    LONG_EDGE: 'LONG_EDGE'
  };

  CloudCapabilities.PageOrientationType_ = {
    PORTRAIT: 'PORTRAIT',
    LANDSCAPE: 'LANDSCAPE'
  };

  CloudCapabilities.hasColorCapability_ = function(cdd) {
    if (!cdd.printer || !cdd.printer.color) {
      return false;
    }
    var hasStandardColor = false;
    var hasStandardMonochrome = false;
    cdd.printer.color.option.forEach(function(option) {
      if (option.type == CloudCapabilities.ColorType_.STANDARD_COLOR) {
        hasStandardColor = true;
      } else if (option.type ==
          CloudCapabilities.ColorType_.STANDARD_MONOCHROME) {
        hasStandardMonochrome = true;
      }
    });
    return hasStandardColor && hasStandardMonochrome;
  };

  CloudCapabilities.hasDuplexCapability_ = function(cdd) {
    if (!cdd.printer || !cdd.printer.duplex) {
      return false;
    }
    var hasNoDuplexOption = false;
    var hasLongEdgeDuplexOption = false;
    cdd.printer.duplex.option.forEach(function(option) {
      if (option.type == CloudCapabilities.DuplexType_.NO_DUPLEX) {
        hasNoDuplexOption = true;
      } else if (option.type == CloudCapabilities.DuplexType_.LONG_EDGE) {
        hasLongEdgeDuplexOption = true;
      }
    });
    return hasNoDuplexOption && hasLongEdgeDuplexOption;
  };

  CloudCapabilities.hasPageOrientationCapability_ = function(cdd) {
    if (!cdd.printer || !cdd.printer.page_orientation) {
      return false;
    }
    var hasPortraitOption = false;
    var hasLandscapeOption = false;
    cdd.printer.page_orientation.option.forEach(function(option) {
      if (option.type == CloudCapabilities.PageOrientationType_.PORTRAIT) {
        hasPortraitOption = true;
      } else if (option.type ==
          CloudCapabilities.PageOrientationType_.LANDSCAPE) {
        hasLandscapeOption = true;
      }
    });
    return hasPortraitOption && hasLandscapeOption;
  };

  CloudCapabilities.isColorDefault_ = function(color) {
    return color.option.some(function(option) {
      return option.is_default &&
          option.type == CloudCapabilities.ColorType_.STANDARD_COLOR;
    });
  };

  CloudCapabilities.isLongEdgeDefault_ = function(duplex) {
    return duplex.option.some(function(option) {
      return option.is_default &&
          option.type == CloudCapabilities.DuplexType_.LONG_EDGE;
    });
  };

  CloudCapabilities.prototype = {
    __proto__: print_preview.ChromiumCapabilities.prototype,

    /**
     * Creates a Cloud Job Ticket (CJT) from the given PrintTicketStore.
     * @param {!print_preview.PrintTicketStore} printTicketStore Print ticket
     *     store that contains the values to put into the CJT.
     * @return {!Object} CJT for the print job.
     */
    createCjt: function(printTicketStore) {
      var cjt = {
        version: '1.0',
        print: {}
      };
      if (this.cdd_.printer && this.cdd_.printer.collate) {
        cjt.print.collate =
            this.createCollateTicketItem_(printTicketStore.isCollateEnabled());
      }
      if (CloudCapabilities.hasColorCapability_(this.cdd_)) {
        cjt.print.color =
            this.createColorTicketItem_(printTicketStore.isColorEnabled());
      }
      if (this.cdd_.printer && this.cdd_.printer.copies) {
        cjt.print.copies =
            this.createCopiesTicketItem_(printTicketStore.getCopies());
      }
      if (CloudCapabilities.hasDuplexCapability_(this.cdd_)) {
        cjt.print.duplex =
            this.createDuplexTicketItem_(printTicketStore.isDuplexEnabled());
      }
      if (CloudCapabilities.hasPageOrientationCapability_(this.cdd_)) {
        cjt.print.page_orientation = this.createPageOrientationTicketItem_(
            printTicketStore.isLandscapeEnabled());
      }
      return cjt;
    },

    createCollateTicketItem_: function(isCollateEnabled) {
      return {collate: isCollateEnabled};
    },

    createColorTicketItem_: function(isColorEnabled) {
      var selectedColorType = isColorEnabled ?
          CloudCapabilities.ColorType_.STANDARD_COLOR :
          CloudCapabilities.ColorType_.STANDARD_MONOCHROME;
      var selectedColorOption =
          this.cdd_.printer.color.option.filter(function(option) {
            return option.type == selectedColorType;
          });

      assert(selectedColorOption.length > 0,
             'Color capability missing either the STANDARD_COLOR or ' +
             'STANDARD_MONOCHROME option');

      var colorTicketItem = {};
      if (selectedColorOption[0].hasOwnProperty('vendor_id')) {
        colorTicketItem.vendor_id = selectedColorOption[0].vendor_id;
      }
      if (selectedColorOption[0].hasOwnProperty('type')) {
        colorTicketItem.type = selectedColorOption[0].type;
      }
      return colorTicketItem;
    },

    createCopiesTicketItem_: function(copies) {
      return {copies: copies};
    },

    createDuplexTicketItem_: function(isDuplexEnabled) {
      var selectedDuplexType = isDuplexEnabled ?
          CloudCapabilities.DuplexType_.LONG_EDGE :
          CloudCapabilities.DuplexType_.NO_DUPLEX;
      return {type: selectedDuplexType};
    },

    createPageOrientationTicketItem_: function(isLandscapeEnabled) {
      var selectedPageOrientationType = isLandscapeEnabled ?
          CloudCapabilities.PageOrientationType_.LANDSCAPE :
          CloudCapabilities.PageOrientationType_.PORTRAIT;
      return {type: selectedPageOrientationType};
    }
  };

  // Export
  return {
    CloudCapabilities: CloudCapabilities
  };
});
