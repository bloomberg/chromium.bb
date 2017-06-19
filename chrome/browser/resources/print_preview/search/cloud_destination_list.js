// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  'use strict';

  /**
   * Sub-class of a destination list that shows cloud-based destinations.
   * @param {!cr.EventTarget} eventTarget Event target to pass to destination
   *     items for dispatching SELECT events.
   * @constructor
   * @extends {print_preview.DestinationList}
   */
  function CloudDestinationList(eventTarget) {
    print_preview.DestinationList.call(
        this, eventTarget, loadTimeData.getString('cloudDestinationsTitle'),
        loadTimeData.getString('manage'));
  }

  CloudDestinationList.prototype = {
    __proto__: print_preview.DestinationList.prototype,

    /** @override */
    updateDestinations: function(destinations) {
      // Change the action link from "Manage..." to "Setup..." if user only has
      // the Docs printer.
      var docsId = print_preview.Destination.GooglePromotedId.DOCS;
      this.setActionLinkTextInternal(loadTimeData.getString(
          destinations.length == 1 && destinations[0].id == docsId ?
              'setupCloudPrinters' :
              'manage'));
      print_preview.DestinationList.prototype.updateDestinations.call(
          this, destinations);
    }
  };

  return {CloudDestinationList: CloudDestinationList};
});
