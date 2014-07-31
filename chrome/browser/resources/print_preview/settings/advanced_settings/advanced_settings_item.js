// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  'use strict';

  /**
   * Component that renders a destination item in a destination list.
   * @param {!cr.EventTarget} eventTarget Event target to dispatch selection
   *     events to.
   * @param {!print_preview.PrintTicketStore} printTicketStore Contains the
   *     print ticket to print.
   * @param {!print_preview.Destination} destination Destination data object to
   *     render.
   * @param {!Object} capability Capability to render.
   * @constructor
   * @extends {print_preview.Component}
   */
  function AdvancedSettingsItem(eventTarget, printTicketStore, capability) {
    print_preview.Component.call(this);

    /**
     * Event target to dispatch selection events to.
     * @private {!cr.EventTarget}
     */
    this.eventTarget_ = eventTarget;

    /**
     * Contains the print ticket to print.
     * @private {!print_preview.PrintTicketStore}
     */
    this.printTicketStore_ = printTicketStore;

    /**
     * Capability that the list item renders.
     * @private {!Object}
     */
    this.capability_ = capability;

    /**
     * Active filter query text.
     * @private {RegExp}
     */
    this.query_ = null;
  };

  /**
   * Event types dispatched by this class.
   * @enum {string}
   */
  AdvancedSettingsItem.EventType = {
    CHANGED: 'print_preview.AdvancedSettingsItem.CHANGED'
  };

  AdvancedSettingsItem.prototype = {
    __proto__: print_preview.Component.prototype,

    /** @override */
    createDom: function() {
      this.setElementInternal(this.cloneTemplateInternal(
          'advanced-settings-item-template'));

      var nameEl = this.getChildElement('.advanced-settings-item-label');
      var textContent = this.capability_.display_name;
      if (this.query_)
        this.addTextWithHighlight_(nameEl, textContent);
      else
        nameEl.textContent = textContent;
      nameEl.title = textContent;

      this.initializeValue_();
    },

    /**
     * Whether the corresponding ticket item was changed or not.
     * @return {boolean}
     */
    isModified: function() {
      return false;
    },

    /**
     * Initializes the element's value control.
     * @private
     */
    initializeValue_: function() {
      if (this.capability_.type == 'SELECT')
        this.initializeSelectValue_();
      else
        this.initializeTextValue_();
    },

    /**
     * Initializes the select element.
     * @private
     */
    initializeSelectValue_: function() {
      var selectEl = this.getChildElement(
          '.advanced-settings-item-value-select-control');
      setIsVisible(
          this.getChildElement('.advanced-settings-item-value-select'), true);
      var indexToSelect = 0;
      this.capability_.select_cap.option.forEach(function(option, index) {
        var item = document.createElement('option');
        item.text = option.display_name;
        item.value = option.value;
        if (option.is_default)
          indexToSelect = index;
        selectEl.add(item);
      });
      // TODO: Try to select current ticket item.
      var valueToSelect = '';
      for (var i = 0, option; option = selectEl.options[i]; i++) {
        if (option.value == valueToSelect) {
          indexToSelect = i;
          break;
        }
      }
      selectEl.selectedIndex = indexToSelect;
    },

    /**
     * Initializes the text element.
     * @private
     */
    initializeTextValue_: function() {
      var textEl = this.getChildElement(
          '.advanced-settings-item-value-text-control');
      setIsVisible(
          this.getChildElement('.advanced-settings-item-value-text'), true);
    },

    /**
     * Adds text to parent element wrapping search query matches in highlighted
     * spans.
     * @param {!Element} parent Element to build the text in.
     * @param {string} text The text string to highlight segments in.
     * @private
     */
    addTextWithHighlight_: function(parent, text) {
      text.split(this.query_).forEach(function(section, i) {
        if (i % 2 == 0) {
          parent.appendChild(document.createTextNode(section));
        } else {
          var span = document.createElement('span');
          span.className = 'advanced-settings-item-query-highlight';
          span.textContent = section;
          parent.appendChild(span);
        }
      });
    }
  };

  // Export
  return {
    AdvancedSettingsItem: AdvancedSettingsItem
  };
});
