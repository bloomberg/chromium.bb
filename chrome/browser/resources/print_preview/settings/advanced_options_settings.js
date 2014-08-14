// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  'use strict';

  /**
   * Print options section to control printer advanced options.
   * @param {!print_preview.DestinationStore} destinationStore Used to determine
   *     the selected destination.
   * @constructor
   * @extends {print_preview.Component}
   */
  function AdvancedOptionsSettings(destinationStore) {
    print_preview.Component.call(this);

    /**
     * Used to determine the selected destination.
     * @private {!print_preview.DestinationStore}
     */
    this.destinationStore_ = destinationStore;
  };

  /**
   * Event types dispatched by the component.
   * @enum {string}
   */
  AdvancedOptionsSettings.EventType = {
    BUTTON_ACTIVATED: 'print_preview.AdvancedOptionsSettings.BUTTON_ACTIVATED'
  };

  AdvancedOptionsSettings.prototype = {
    __proto__: print_preview.Component.prototype,

    /** @param {boolean} Whether the component is enabled. */
    set isEnabled(isEnabled) {
      this.getButton_().disabled = !isEnabled;
    },

    /** @override */
    enterDocument: function() {
      print_preview.Component.prototype.enterDocument.call(this);

      fadeOutOption(this.getElement(), true);

      this.tracker.add(
          this.getButton_(), 'click', function() {
            cr.dispatchSimpleEvent(
                this, AdvancedOptionsSettings.EventType.BUTTON_ACTIVATED);
          }.bind(this));
      this.tracker.add(
          this.destinationStore_,
          print_preview.DestinationStore.EventType.DESTINATION_SELECT,
          this.onDestinationSelect_.bind(this));
      this.tracker.add(
          this.destinationStore_,
          print_preview.DestinationStore.EventType.
              SELECTED_DESTINATION_CAPABILITIES_READY,
          this.onDestinationSelect_.bind(this));
    },

    /**
     * @return {HTMLElement}
     * @private
     */
    getButton_: function() {
      return this.getChildElement('.advanced-options-settings-button');
    },

    /**
     * Called when the destination selection has changed. Updates UI elements.
     * @private
     */
    onDestinationSelect_: function() {
      var destination = this.destinationStore_.selectedDestination;
      var vendorCapabilities =
          destination &&
          destination.capabilities &&
          destination.capabilities.printer &&
          destination.capabilities.printer.vendor_capability;
      if (false && vendorCapabilities) {
        fadeInOption(this.getElement());
      } else {
        fadeOutOption(this.getElement());
      }
    }
  };

  // Export
  return {
    AdvancedOptionsSettings: AdvancedOptionsSettings
  };
});
