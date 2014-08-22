// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  'use strict';

  /**
   * Toggles visibility of the specified printing options sections.
   * @param {!Array.<print_preview.SettingsSection>} settingsSections Sections
   *     to toggle by this component.
   * @constructor
   * @extends {print_preview.Component}
   */
  function MoreSettings(settingsSections) {
    print_preview.Component.call(this);

    /** @private {!Array.<print_preview.SettingsSection>} */
    this.settingsSections_ = settingsSections;

    /** @private {MoreSettings.SettingsToShow} */
    this.settingsToShow_ = MoreSettings.SettingsToShow.MOST_POPULAR;
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
      this.settingsSections_.forEach(function(section) {
        this.tracker.add(
            section,
            print_preview.SettingsSection.EventType.COLLAPSIBLE_CONTENT_CHANGED,
            this.updateState_.bind(this));
      }.bind(this));

      this.updateState_();
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
      this.updateState_();
    },

    /**
     * Updates the component appearance according to the current state.
     * @private
     */
    updateState_: function() {
      var all = this.settingsToShow_ == MoreSettings.SettingsToShow.ALL;
      this.getChildElement('.more-settings-label').textContent =
          localStrings.getString(all ? 'lessOptionsLabel' : 'moreOptionsLabel');
      var iconEl = this.getChildElement('.more-settings-icon');
      iconEl.classList.toggle('more-settings-icon-plus', !all);
      iconEl.classList.toggle('more-settings-icon-minus', all);

      var hasSectionsToToggle = this.settingsSections_.some(function(section) {
        return section.hasCollapsibleContent();
      });

      if (hasSectionsToToggle)
        fadeInElement(this.getElement());
      else
        fadeOutElement(this.getElement());

      var collapseContent =
          this.settingsToShow_ == MoreSettings.SettingsToShow.MOST_POPULAR &&
          hasSectionsToToggle;
      this.settingsSections_.forEach(function(section) {
        section.collapseContent = collapseContent;
      });
    }
  };

  // Export
  return {
    MoreSettings: MoreSettings
  };
});
