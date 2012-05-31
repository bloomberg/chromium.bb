// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview.ticket_items', function() {
  'use strict';

  /**
   * An object that represents a user modifiable item in a print ticket. Each
   * ticket item has a value which can be set by the user. Ticket items can also
   * be unavailable for modifying if the print destination doesn't support it or
   * if other ticket item constraints are not met.
   * @constructor
   */
  function TicketItem() {
    /**
     * Backing store of the print ticket item.
     * @type {Object}
     * @private
     */
    this.value_ = null;
  };

  TicketItem.prototype = {
    /**
     * Determines whether a given value is valid for the ticket item.
     * @param {Object} value The value to check for validity.
     * @return {boolean} Whether the given value is valid for the ticket item.
     */
    wouldValueBeValid: function(value) {
      throw Error('Abstract method not overridden');
    },

    /**
     * @return {boolean} Whether the print destination capability is available.
     */
    isCapabilityAvailable: function() {
      throw Error('Abstract method not overridden');
    },

    /** @return {!Object} The value of the ticket item. */
    getValue: function() {
      if (this.isCapabilityAvailable()) {
        if (this.value_ == null) {
          return this.getDefaultValueInternal();
        } else {
          return this.value_;
        }
      } else {
        return this.getCapabilityNotAvailableValueInternal();
      }
    },

    /** @return {boolean} Whether the ticket item was modified by the user. */
    isUserEdited: function() {
      return this.value_ != null;
    },

    /** @return {boolean} Whether the ticket item's value is valid. */
    isValid: function() {
      if (!this.isUserEdited()) {
        return true;
      }
      return this.wouldValueBeValid(this.value_);
    },

    /** @param {!Object} Value to set as the value of the ticket item. */
    updateValue: function(value) {
      this.value_ = value;
    },

    /**
     * @return {!Object} Default value of the ticket item if no value was set by
     *     the user.
     * @protected
     */
    getDefaultValueInternal: function() {
      throw Error('Abstract method not overridden');
    },

    /**
     * @return {!Object} Default value of the ticket item if the capability is
     *     not available.
     * @protected
     */
    getCapabilityNotAvailableValueInternal: function() {
      throw Error('Abstract method not overridden');
    }
  };

  // Export
  return {
    TicketItem: TicketItem
  };
});
