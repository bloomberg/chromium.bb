// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  'use strict';

  /**
   * Encapsulates all settings and logic related to the media size selection UI.
   * @param {!print_preview.ticket_items.MediaSize} ticketItem Used to read and
   *     write the media size ticket item.
   * @constructor
   * @extends {print_preview.SettingsSection}
   */
  function MediaSizeSettings(ticketItem) {
    print_preview.SettingsSection.call(this);

    /** @private {!print_preview.ticket_items.MediaSize} */
    this.ticketItem_ = ticketItem;
  };

  MediaSizeSettings.prototype = {
    __proto__: print_preview.SettingsSection.prototype,

    /** @override */
    isAvailable: function() {
      return this.ticketItem_.isCapabilityAvailable();
    },

    /** @override */
    hasCollapsibleContent: function() {
      return this.isAvailable();
    },

    /** @override */
    set isEnabled(isEnabled) {
      this.select_.disabled = !isEnabled;
    },

    /** @override */
    enterDocument: function() {
      print_preview.SettingsSection.prototype.enterDocument.call(this);
      this.tracker.add(this.select_, 'change', this.onSelectChange_.bind(this));
      this.tracker.add(
          this.ticketItem_,
          print_preview.ticket_items.TicketItem.EventType.CHANGE,
          this.onTicketItemChange_.bind(this));
    },

    /**
     * @return {HTMLSelectElement} Select element containing media size options.
     * @private
     */
    get select_() {
      return this.getElement().getElementsByClassName(
          'media-size-settings-select')[0];
    },

    /**
     * Makes sure the content of the select element matches the capabilities of
     * the destination.
     * @private
     */
    updateSelect_: function() {
      var select = this.select_;
      if (!this.isAvailable()) {
        select.innerHTML = '';
        return;
      }
      // Should the select content be updated?
      var sameContent =
          this.ticketItem_.capability.option.length == select.length &&
          this.ticketItem_.capability.option.every(function(option, index) {
            return select.options[index].value == JSON.stringify(option);
          });
      var indexToSelect = select.selectedIndex;
      if (!sameContent) {
        select.innerHTML = '';
        // TODO: Better heuristics for the display name and options grouping.
        this.ticketItem_.capability.option.forEach(function(option, index) {
          var selectOption = document.createElement('option');
          var displayName = option.custom_display_name;
          if (!displayName && option.custom_display_name_localized) {
            var getLocaleToCompare = function(locale, languageOnly) {
              var code = languageOnly ? locale.split('-')[0] : locale;
              return code.toLowerCase();
            };
            var getItemForLocale = function(items, locale, languageOnly) {
              locale = getLocaleToCompare(locale, languageOnly);
              for (var i = 0; i < items.length; i++) {
                if (getLocaleToCompare(items[i].locale) == locale)
                  return items[i].value;
              }
              return '';
            };
            var items = option.custom_display_name_localized;
            displayName =
                getItemForLocale(items, navigator.language, false) ||
                getItemForLocale(items, navigator.language, true);
          }
          selectOption.text = displayName || option.name;
          selectOption.value = JSON.stringify(option);
          select.add(selectOption);
          if (option.is_default) {
            indexToSelect = index;
          }
        });
      }
      // Try to select current ticket item.
      var valueToSelect = JSON.stringify(this.ticketItem_.getValue());
      for (var i = 0, option; option = select.options[i]; i++) {
        if (option.value == valueToSelect) {
          indexToSelect = i;
          break;
        }
      }
      select.selectedIndex = indexToSelect;
      this.onSelectChange_();
    },

    /**
     * Called when the select element is changed. Updates the print ticket.
     * @private
     */
    onSelectChange_: function() {
      var select = this.select_;
      var mediaSize = JSON.parse(select.options[select.selectedIndex].value);
      this.ticketItem_.updateValue(mediaSize);
    },

    /**
     * Called when the print ticket store changes. Selects the corresponding
     * select option.
     * @private
     */
    onTicketItemChange_: function() {
      this.updateSelect_();
      this.updateUiStateInternal();
    }
  };

  // Export
  return {
    MediaSizeSettings: MediaSizeSettings
  };
});
