// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview.ticket_items', function() {
  'use strict';

  /**
   * Landscape ticket item whose value is a {@code boolean} that indicates
   * whether the document should be printed in landscape orientation.
   * @param {!print_preview.CapabilitiesHolder} capabilitiesHolder Capabilities
   *     holder used to determine the default landscape value and if landscape
   *     printing is available.
   * @param {!print_preview.DocumentInfo} documentInfo Information about the
   *     document to print.
   * @constructor
   * @extends {print_preview.ticket_items.TicketItem}
   */
  function Landscape(capabilitiesHolder, documentInfo) {
    print_preview.ticket_items.TicketItem.call(this);

    /**
     * Capabilities holder used to determine the default landscape value and if
     * landscape printing is available.
     * @type {!print_preview.CapabilitiesHolder}
     * @private
     */
    this.capabilitiesHolder_ = capabilitiesHolder;

    /**
     * Information about the document to print.
     * @type {!print_preview.DocumentInfo}
     * @private
     */
    this.documentInfo_ = documentInfo;
  };

  Landscape.prototype = {
    __proto__: print_preview.ticket_items.TicketItem.prototype,

    /** @override */
    wouldValueBeValid: function(value) {
      return true;
    },

    /** @override */
    isCapabilityAvailable: function() {
      // TODO(rltoscano): Technically, the print destination can still change
      // the orientation of the print out (at least for cloud printers) if the
      // document is not modifiable. But the preview wouldn't update in this
      // case so it would be a bad user experience.
      return this.documentInfo_.isModifiable &&
          !this.documentInfo_.hasCssMediaStyles &&
          this.capabilitiesHolder_.get().hasOrientationCapability;
    },

    /** @override */
    getDefaultValueInternal: function() {
      return this.capabilitiesHolder_.get().defaultIsLandscapeEnabled;
    },

    /** @override */
    getCapabilityNotAvailableValueInternal: function() {
      return this.documentInfo_.hasCssMediaStyles ?
          (this.documentInfo_.pageSize.width >
              this.documentInfo_.pageSize.height) :
          false;
    }
  };

  // Export
  return {
    Landscape: Landscape
  };
});
