// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  'use strict';

  /**
   * Component that renders a destination item in a destination list.
   * @param {!cr.EventTarget} eventTarget Event target to dispatch selection
   *     events to.
   * @param {!print_preview.Destination} destination Destination data object to
   *     render.
   * @constructor
   * @extends {print_preview.Component}
   */
  function DestinationListItem(eventTarget, destination) {
    print_preview.Component.call(this);

    /**
     * Event target to dispatch selection events to.
     * @type {!cr.EventTarget}
     * @private
     */
    this.eventTarget_ = eventTarget;

    /**
     * Destination that the list item renders.
     * @type {!print_preview.Destination}
     * @private
     */
    this.destination_ = destination;
  };

  /**
   * Event types dispatched by the destination list item.
   * @enum {string}
   */
  DestinationListItem.EventType = {
    // Dispatched when the list item is activated.
    SELECT: 'print_preview.DestinationListItem.SELECT'
  };

  /**
   * CSS classes used by the destination list item.
   * @enum {string}
   * @private
   */
  DestinationListItem.Classes_ = {
    ICON: 'destination-list-item-icon',
    NAME: 'destination-list-item-name'
  };

  /**
   * URLs of the various destination list item icons.
   * @enum {string}
   * @private
   */
  DestinationListItem.Icons_ = {
    CLOUD: 'images/cloud_printer_32.png',
    CLOUD_SHARED: 'images/cloud_printer_shared_32.png',
    LOCAL: 'images/classic_printer_32.png',
    MOBILE: 'images/mobile_32.png',
    MOBILE_SHARED: 'images/mobile_shared_32.png',
    GOOGLE_PROMOTED: 'images/google_promoted_printer_32.png'
  },

  DestinationListItem.prototype = {
    __proto__: print_preview.Component.prototype,

    /** @override */
    createDom: function() {
      this.setElementInternal(this.cloneTemplateInternal(
          'destination-list-item-template'));

      var iconUrl;
      if (this.destination_.isGooglePromoted) {
        iconUrl = DestinationListItem.Icons_.GOOGLE_PROMOTED;
      } else if (this.destination_.isLocal) {
        iconUrl = DestinationListItem.Icons_.LOCAL;
      } else if (this.destination_.type ==
          print_preview.Destination.Type.MOBILE && this.destination_.isOwned) {
        iconUrl = DestinationListItem.Icons_.MOBILE;
      } else if (this.destination_.type ==
          print_preview.Destination.Type.MOBILE && !this.destination_.isOwned) {
        iconUrl = DestinationListItem.Icons_.MOBILE_SHARED;
      } else if (this.destination_.type ==
          print_preview.Destination.Type.GOOGLE && this.destination_.isOwned) {
        iconUrl = DestinationListItem.Icons_.CLOUD;
      } else {
        iconUrl = DestinationListItem.Icons_.CLOUD_SHARED;
      }

      var iconImg = this.getElement().getElementsByClassName(
          print_preview.DestinationListItem.Classes_.ICON)[0];
      iconImg.src = iconUrl;
      var nameEl = this.getElement().getElementsByClassName(
          DestinationListItem.Classes_.NAME)[0];
      nameEl.textContent = this.destination_.displayName;
    },

    /** @override */
    enterDocument: function() {
      print_preview.Component.prototype.enterDocument.call(this);
      this.tracker.add(this.getElement(), 'click', this.onActivate_.bind(this));
    },

    /**
     * Called when the destination item is activated. Dispatches a SELECT event
     * on the given event target.
     * @private
     */
    onActivate_: function() {
      var selectEvt = new cr.Event(DestinationListItem.EventType.SELECT);
      selectEvt.destination = this.destination_;
      this.eventTarget_.dispatchEvent(selectEvt);
    }
  };

  // Export
  return {
    DestinationListItem: DestinationListItem
  };
});
