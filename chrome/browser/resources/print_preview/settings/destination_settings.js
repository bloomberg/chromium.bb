// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  'use strict';

  // TODO(rltoscano): This class needs a throbber while loading the destination
  // or another solution is persist the settings of the printer so that next
  // load is fast.

  /**
   * Component used to render the print destination.
   * @param {!print_preview.DestinationStore} destinationStore Used to determine
   *     the selected destination.
   * @constructor
   * @extends {print_preview.Component}
   */
  function DestinationSettings(destinationStore) {
    print_preview.Component.call(this);

    /**
     * Used to determine the selected destination.
     * @type {!print_preview.DestinationStore}
     * @private
     */
    this.destinationStore_ = destinationStore;
  };

  /**
   * Event types dispatched by the component.
   * @enum {string}
   */
  DestinationSettings.EventType = {
    MANAGE_PRINTERS_SELECT:
        'print_preview.DestinationSettings.MANAGE_PRINTERS_SELECT'
  };

  /**
   * CSS classes used by the component.
   * @enum {string}
   * @private
   */
  DestinationSettings.Classes_ = {
    SELECT: 'destination-settings-select'
  };

  /**
   * Option value of the "Manage Printers..." select option.
   * @type {string}
   * @const
   * @private
   */
  DestinationSettings.MANAGE_ID_ = '__manage';

  DestinationSettings.prototype = {
    __proto__: print_preview.Component.prototype,

    set isEnabled(isEnabled) {
      this.select_.disabled = !isEnabled;
    },

    /** @override */
    enterDocument: function() {
      print_preview.Component.prototype.enterDocument.call(this);
      this.tracker.add(
          this.select_, 'change', this.onSelectChange_.bind(this));
      this.tracker.add(
          this.destinationStore_,
          print_preview.DestinationStore.EventType.DESTINATION_SELECT,
          this.onDestinationSelect_.bind(this));
      this.tracker.add(
          this.destinationStore_,
          print_preview.DestinationStore.EventType.DESTINATIONS_INSERTED,
          this.onDestinationsInserted_.bind(this));
    },

    get select_() {
      return this.getElement().getElementsByClassName(
          DestinationSettings.Classes_.SELECT)[0];
    },

    renderDestinations_: function() {
      var select = this.select_;
      select.innerHTML = '';
      var destinations = this.destinationStore_.destinations;
      var selectedDestination = this.destinationStore_.selectedDestination;
      var saveToPdfDest = null;
      var printWithCloudPrintDest = null;
      for (var dest, i = 0; dest = destinations[i]; i++) {
        if (dest.id == print_preview.Destination.GooglePromotedId.SAVE_AS_PDF) {
          saveToPdfDest = dest;
          continue;
        }
        if (dest.isPrintWithCloudPrint) {
          printWithCloudPrintDest = dest;
          continue;
        }
        var optionEl = document.createElement('option');
        optionEl.value = dest.id;
        optionEl.selected =
            selectedDestination && selectedDestination.id == dest.id;
        optionEl.textContent = dest.displayName;
        select.appendChild(optionEl);
      }

      // Add special destinations.
      if (saveToPdfDest) {
        select.appendChild(this.createSeparatorOption_());
        var printToPdfOptionEl = document.createElement('option');
        printToPdfOptionEl.value = saveToPdfDest.id;
        printToPdfOptionEl.selected =
            selectedDestination && selectedDestination.id == saveToPdfDest.id;
        printToPdfOptionEl.textContent = saveToPdfDest.displayName;
        select.appendChild(printToPdfOptionEl);
      }
      if (printWithCloudPrintDest) {
        select.appendChild(this.createSeparatorOption_());
        var printWithCloudPrintOptionEl = document.createElement('option');
        printWithCloudPrintOptionEl.value = printWithCloudPrintDest.id;
        printWithCloudPrintOptionEl.selected =
            selectedDestination &&
            selectedDestination.id == printWithCloudPrintDest.id;
        printWithCloudPrintOptionEl.textContent =
            printWithCloudPrintDest.displayName;
        select.appendChild(printWithCloudPrintOptionEl);
      }
      select.appendChild(this.createSeparatorOption_());
      var manageOptionEl = document.createElement('option');
      manageOptionEl.value = DestinationSettings.MANAGE_ID_;
      manageOptionEl.textContent = localStrings.getString('managePrinters');
      select.appendChild(manageOptionEl);
    },

    createSeparatorOption_: function() {
      var sep = document.createElement('option');
      sep.disabled = true;
      sep.role = 'separator';
      return sep;
    },

    /**
     * Called when a destination is selected. Selects the corresponding option.
     * @private
     */
    onDestinationSelect_: function() {
      var select = this.select_;
      if (select.options.length > 0) {
        select.options[select.selectedIndex].selected = false;
      }
      var selectedDestination = this.destinationStore_.selectedDestination;
      for (var option, i = 0; option = select.options[i]; i++) {
        if (selectedDestination.id == option.value) {
          option.selected = true;
          break;
        }
      }
    },

    /**
     * Called when destinations are inserted into the destination store. Updates
     * the select element.
     * @private
     */
    onDestinationsInserted_: function() {
      this.renderDestinations_();
    },

    /**
     * Called when the select element changes options. Selects the corresponding
     * print destination.
     * @private
     */
    onSelectChange_: function() {
      var select = this.select_;
      var selectedDestId = select.options[select.selectedIndex].value;

      if (selectedDestId == DestinationSettings.MANAGE_ID_) {
        cr.dispatchSimpleEvent(
            this, DestinationSettings.EventType.MANAGE_PRINTERS_SELECT);
        // Select first in the list.
        this.destinationStore_.selectDestination(
            this.destinationStore_.destinations[0]);
      } else {
        var destinations = this.destinationStore_.destinations;
        for (var dest, i = 0; dest = destinations[i]; i++) {
          if (dest.id == selectedDestId) {
            this.destinationStore_.selectDestination(dest);
            break;
          }
        }
      }
    }
  };

  return {
    DestinationSettings: DestinationSettings
  };
});
