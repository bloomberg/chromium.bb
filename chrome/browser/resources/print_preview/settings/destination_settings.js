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

    /**
     * Current CSS class of the destination icon.
     * @type {?DestinationSettings.Classes_}
     * @private
     */
    this.iconClass_ = null;
  };

  /**
   * Event types dispatched by the component.
   * @enum {string}
   */
  DestinationSettings.EventType = {
    CHANGE_BUTTON_ACTIVATE:
        'print_preview.DestinationSettings.CHANGE_BUTTON_ACTIVATE'
  };

  /**
   * CSS classes used by the component.
   * @enum {string}
   * @private
   */
  DestinationSettings.Classes_ = {
    CHANGE_BUTTON: 'destination-settings-change-button',
    ICON: 'destination-settings-icon',
    ICON_CLOUD: 'destination-settings-icon-cloud',
    ICON_CLOUD_SHARED: 'destination-settings-icon-cloud-shared',
    ICON_GOOGLE_PROMOTED: 'destination-settings-icon-google-promoted',
    ICON_LOCAL: 'destination-settings-icon-local',
    ICON_MOBILE: 'destination-settings-icon-mobile',
    ICON_MOBILE_SHARED: 'destination-settings-icon-mobile-shared',
    LOCATION: 'destination-settings-location',
    NAME: 'destination-settings-name'
  };

  DestinationSettings.prototype = {
    __proto__: print_preview.Component.prototype,

    /** @param {boolean} Whether the component is enabled. */
    set isEnabled(isEnabled) {
      var changeButton = this.getElement().getElementsByClassName(
          DestinationSettings.Classes_.CHANGE_BUTTON)[0];
      changeButton.disabled = !isEnabled;
    },

    /** @override */
    enterDocument: function() {
      print_preview.Component.prototype.enterDocument.call(this);
      var changeButton = this.getElement().getElementsByClassName(
          DestinationSettings.Classes_.CHANGE_BUTTON)[0];
      this.tracker.add(
          changeButton, 'click', this.onChangeButtonClick_.bind(this));
      this.tracker.add(
          this.destinationStore_,
          print_preview.DestinationStore.EventType.DESTINATION_SELECT,
          this.onDestinationSelect_.bind(this));
    },

    /**
     * Called when the "Change" button is clicked. Dispatches the
     * CHANGE_BUTTON_ACTIVATE event.
     * @private
     */
    onChangeButtonClick_: function() {
      cr.dispatchSimpleEvent(
          this, DestinationSettings.EventType.CHANGE_BUTTON_ACTIVATE);
    },

    /**
     * Called when the destination selection has changed. Updates UI elements.
     * @private
     */
    onDestinationSelect_: function() {
      var destination = this.destinationStore_.selectedDestination;
      var nameEl = this.getElement().getElementsByClassName(
          DestinationSettings.Classes_.NAME)[0];
      nameEl.textContent = destination.displayName;

      var iconEl = this.getElement().getElementsByClassName(
          DestinationSettings.Classes_.ICON)[0];
      iconEl.src = destination.iconUrl;

      var locationEl = this.getElement().getElementsByClassName(
          DestinationSettings.Classes_.LOCATION)[0];
      locationEl.textContent = destination.location;

      setIsVisible(this.getElement().querySelector('.throbber'), false);
      setIsVisible(
          this.getElement().querySelector('.destination-settings-box'), true);
    }
  };

  // Export
  return {
    DestinationSettings: DestinationSettings
  };
});
