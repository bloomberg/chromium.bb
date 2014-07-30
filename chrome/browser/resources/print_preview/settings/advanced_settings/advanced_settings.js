// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  'use strict';

  /**
   * Modal dialog for print destination's advanced settings.
   * @constructor
   * @extends {print_preview.Overlay}
   */
  function AdvancedSettings() {
    print_preview.Overlay.call(this);

    /**
     * Used to record usage statistics.
     * @private {!print_preview.PrintSettingsUiMetricsContext}
     */
    this.metrics_ = new print_preview.PrintSettingsUiMetricsContext();

    /** @private {!print_preview.SearchBox} */
    this.searchBox_ = new print_preview.SearchBox(
        localStrings.getString('advancedSettingsSearchBoxPlaceholder'));
    this.addChild(this.searchBox_);

    /** @private {print_preview.Destination} */
    this.destination_ = null;
  };

  AdvancedSettings.prototype = {
    __proto__: print_preview.Overlay.prototype,

    /**
     * @param {!print_preview.Destination} destination Destination to show
     *     advanced settings for.
     */
    showForDestination: function(destination) {
      //assert(!this.destination_);
      this.destination_ = destination;
      this.getChildElement('.advanced-settings-title').textContent =
          localStrings.getStringF('advancedSettingsDialogTitle',
                                  this.destination_.displayName);
      this.setIsVisible(true);
    },

    /** @override */
    enterDocument: function() {
      print_preview.Overlay.prototype.enterDocument.call(this);

      this.tracker.add(
          this.getChildElement('#cancel-button'),
          'click',
          this.cancel.bind(this));

      this.tracker.add(
          this.searchBox_,
          print_preview.SearchBox.EventType.SEARCH,
          this.onSearch_.bind(this));

      this.renderSettings_();
    },

    /** @override */
    decorateInternal: function() {
      this.searchBox_.render(this.getChildElement('.search-box-container'));
    },

    /** @override */
    onSetVisibleInternal: function(isVisible) {
      if (isVisible) {
        this.searchBox_.focus();
        this.metrics_.record(print_preview.Metrics.PrintSettingsUiBucket.
            ADVANCED_SETTINGS_DIALOG_SHOWN);
      } else {
        this.searchBox_.setQuery(null);
        this.filterLists_(null);
        this.destination_ = null;
      }
    },

    /** @override */
    onCancelInternal: function() {
      this.metrics_.record(print_preview.Metrics.PrintSettingsUiBucket.
          ADVANCED_SETTINGS_DIALOG_CANCELED);
    },

    /**
     * @return {number} Height available for settings lists, in pixels.
     * @private
     */
    getAvailableListsHeight_: function() {
      var elStyle = window.getComputedStyle(this.getElement());
      return this.getElement().offsetHeight -
          parseInt(elStyle.getPropertyValue('padding-top')) -
          parseInt(elStyle.getPropertyValue('padding-bottom')) -
          this.getChildElement('.lists').offsetTop;
    },

    /**
     * Filters displayed settings with the given query.
     * @param {?string} query Query to filter settings by.
     * @private
     */
    filterLists_: function(query) {
    },

    /**
     * Resets the filter query.
     * @private
     */
    resetSearch_: function() {
      this.searchBox_.setQuery(null);
      this.filterLists_(null);
    },

    /**
     * Renders all of the available settings.
     * @private
     */
    renderSettings_: function() {
    },

    /**
     * Called when settings search query changes. Filters displayed settings
     * with the given query.
     * @param {Event} evt Contains search query.
     * @private
     */
    onSearch_: function(evt) {
      this.filterLists_(evt.query);
    }
  };

  // Export
  return {
    AdvancedSettings: AdvancedSettings
  };
});
