// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  'use strict';

  /**
   * Toggles visibility of the specified printing options sections.
   * @param {!print_preview.DestinationStore} destinationStore To listen for
   *     destination changes.
   * @param {!Array.<print_preview.SettingsSection>} settingsSections Sections
   *     to toggle by this component.
   * @constructor
   * @extends {print_preview.Component}
   */
  function MoreSettings(destinationStore, settingsSections) {
    print_preview.Component.call(this);

    /** @private {!print_preview.DestinationStore} */
    this.destinationStore_ = destinationStore;

    /** @private {!Array.<print_preview.SettingsSection>} */
    this.settingsSections_ = settingsSections;

    /** @private {MoreSettings.SettingsToShow} */
    this.settingsToShow_ = MoreSettings.SettingsToShow.MOST_POPULAR;

    /** @private {boolean} */
    this.capabilitiesReady_ = false;

    /** @private {boolean} */
    this.firstDestinationReady_ = false;
  };

  /**
   * Which settings are visible to the user.
   * @enum {number}
   */
  MoreSettings.SettingsToShow = {
    MOST_POPULAR: 1,
    ALL: 2
  };

  MoreSettings.prototype = {
    __proto__: print_preview.Component.prototype,

    /** @override */
    enterDocument: function() {
      print_preview.Component.prototype.enterDocument.call(this);

      this.tracker.add(this.getElement(), 'click', this.onClick_.bind(this));
      this.tracker.add(
          this.destinationStore_,
          print_preview.DestinationStore.EventType.DESTINATION_SELECT,
          this.onDestinationChanged_.bind(this));
      this.tracker.add(
          this.destinationStore_,
          print_preview.DestinationStore.EventType.
              SELECTED_DESTINATION_CAPABILITIES_READY,
          this.onDestinationCapabilitiesReady_.bind(this));
      this.settingsSections_.forEach(function(section) {
        this.tracker.add(
            section,
            print_preview.SettingsSection.EventType.COLLAPSIBLE_CONTENT_CHANGED,
            this.updateState_.bind(this));
      }.bind(this));

      this.updateState_(true);
    },

    /**
     * Toggles "more/fewer options" state and notifies all the options sections
     *     to reflect the new state.
     * @private
     */
    onClick_: function() {
      this.settingsToShow_ =
          this.settingsToShow_ == MoreSettings.SettingsToShow.MOST_POPULAR ?
              MoreSettings.SettingsToShow.ALL :
              MoreSettings.SettingsToShow.MOST_POPULAR;
      this.updateState_(false);
    },

    /**
     * Called when the destination selection has changed. Updates UI elements.
     * @private
     */
    onDestinationChanged_: function() {
      this.firstDestinationReady_ = true;
      this.capabilitiesReady_ = false;
      this.updateState_(false);
    },

    /**
     * Called when the destination selection has changed. Updates UI elements.
     * @private
     */
    onDestinationCapabilitiesReady_: function() {
      this.capabilitiesReady_ = true;
      this.updateState_(false);
    },

    /**
     * Updates the component appearance according to the current state.
     * @param {boolean} noAnimation Whether section visibility transitions
     *     should not be animated.
     * @private
     */
    updateState_: function(noAnimation) {
      if (!this.firstDestinationReady_) {
        fadeOutElement(this.getElement(), noAnimation);
        return;
      }

      var all = this.settingsToShow_ == MoreSettings.SettingsToShow.ALL;
      this.getChildElement('.more-settings-label').textContent =
          loadTimeData.getString(all ? 'lessOptionsLabel' : 'moreOptionsLabel');
      var iconEl = this.getChildElement('.more-settings-icon');
      iconEl.classList.toggle('more-settings-icon-plus', !all);
      iconEl.classList.toggle('more-settings-icon-minus', all);

      var availableSections = this.settingsSections_.reduce(
          function(count, section) {
            return count + (section.isAvailable() ? 1 : 0);
          }, 0);

      // Magic 6 is chosen as the number of sections when it still feels like
      // manageable and not too crowded. Also, when capabilities are not know
      // yet, ignore this limit to avoid unnecessary fade in/out cycles.
      var hasSectionsToToggle =
          (availableSections > 6 || !this.capabilitiesReady_) &&
          this.settingsSections_.some(function(section) {
            return section.hasCollapsibleContent();
          });

      if (hasSectionsToToggle)
        fadeInElement(this.getElement(), noAnimation);
      else
        fadeOutElement(this.getElement(), noAnimation);

      var collapseContent =
          this.settingsToShow_ == MoreSettings.SettingsToShow.MOST_POPULAR &&
          hasSectionsToToggle;
      this.settingsSections_.forEach(function(section) {
        section.setCollapseContent(collapseContent, noAnimation);
      });
    }
  };

  // Export
  return {
    MoreSettings: MoreSettings
  };
});
