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
     * Capability this component renders.
     * @private {!Object}
     */
    this.capability_ = capability;

    /**
     * Value selected by user. {@code null}, if user has not changed the default
     * value yet (still, the value can be the default one, if it is what user
     * selected).
     * @private {?string}
     */
    this.selectedValue_ = null;

    /**
     * Active filter query.
     * @private {RegExp}
     */
    this.query_ = null;

    /**
     * Search hint for the control.
     * @private {print_preview.SearchBubble}
     */
    this.searchBubble_ = null;

    /** @private {!EventTracker} */
    this.tracker_ = new EventTracker();
  };

  AdvancedSettingsItem.prototype = {
    __proto__: print_preview.Component.prototype,

    /** @override */
    createDom: function() {
      this.setElementInternal(this.cloneTemplateInternal(
          'advanced-settings-item-template'));

      this.tracker_.add(
          this.select_, 'change', this.onSelectChange_.bind(this));
      this.tracker_.add(this.text_, 'input', this.onTextInput_.bind(this));

      this.initializeValue_();

      this.renderCapability_();
    },

    /**
     * ID of the corresponding vendor capability.
     * @return {string}
     */
    get id() {
      return this.capability_.id;
    },

    /**
     * Currently selected value.
     * @return {string}
     */
    get selectedValue() {
      return this.selectedValue_ || '';
    },

    /**
     * Whether the corresponding ticket item was changed or not.
     * @return {boolean}
     */
    isModified: function() {
      return !!this.selectedValue_;
    },

    /** @param {RegExp} query Query to update the filter with. */
    updateSearchQuery: function(query) {
      this.query_ = query;
      this.renderCapability_();
    },

    get searchBubbleShown() {
      return getIsVisible(this.getElement()) && !!this.searchBubble_;
    },

    /**
     * @return {HTMLSelectElement} Select element.
     * @private
     */
    get select_() {
      return this.getChildElement(
          '.advanced-settings-item-value-select-control');
    },

    /**
     * @return {HTMLSelectElement} Text element.
     * @private
     */
    get text_() {
      return this.getChildElement('.advanced-settings-item-value-text-control');
    },

    /**
     * Called when the select element value is changed.
     * @private
     */
    onSelectChange_: function() {
      this.selectedValue_ = this.select_.value;
      this.capability_.select_cap.option.some(function(option) {
        if (this.select_.value == option.value && option.is_default)
          this.selectedValue_ = null;
        return this.select_.value == option.value || option.is_default;
      }.bind(this));
    },

    /**
     * Called when the text element value is changed.
     * @private
     */
    onTextInput_: function() {
      this.selectedValue_ = this.text_.value || null;

      if (this.query_) {
        var optionMatches = (this.selectedValue_ || '').match(this.query_);
        // Even if there's no match anymore, keep the item visible to do not
        // surprise user.
        if (optionMatches)
          this.showSearchBubble_(optionMatches[0]);
        else
          this.hideSearchBubble_();
      }
    },

    /**
     * Renders capability properties according to the current state.
     * @private
     */
    renderCapability_: function() {
      var textContent = this.capability_.display_name;
      var nameMatches = this.query_ ? !!textContent.match(this.query_) : true;
      var optionMatches = null;
      if (this.query_) {
        if (this.capability_.type == 'SELECT') {
          this.capability_.select_cap.option.some(function(option) {
            optionMatches = (option.display_name || '').match(this.query_);
            return !!optionMatches;
          }.bind(this));
        } else {
          optionMatches = (this.text_.value || '').match(this.query_);
        }
      }
      var matches = nameMatches || optionMatches;

      if (!matches || !optionMatches)
        this.hideSearchBubble_();

      setIsVisible(this.getElement(), matches);
      if (!matches)
        return;

      var nameEl = this.getChildElement('.advanced-settings-item-label');
      if (this.query_) {
        nameEl.textContent = '';
        this.addTextWithHighlight_(nameEl, textContent);
      } else {
        nameEl.textContent = textContent;
      }
      nameEl.title = textContent;

      if (optionMatches)
        this.showSearchBubble_(optionMatches[0]);
    },

    /**
     * Shows search bubble for this element.
     * @param {string} text Text to show in the search bubble.
     * @private
     */
    showSearchBubble_: function(text) {
      var element =
          this.capability_.type == 'SELECT' ? this.select_ : this.text_;
      if (!this.searchBubble_) {
        this.searchBubble_ = new print_preview.SearchBubble(text);
        this.searchBubble_.attachTo(element);
      } else {
        this.searchBubble_.content = text;
      }
    },

    /**
     * Hides search bubble associated with this element.
     * @private
     */
    hideSearchBubble_: function() {
      if (this.searchBubble_) {
        this.searchBubble_.dispose();
        this.searchBubble_ = null;
      }
    },

    /**
     * Initializes the element's value control.
     * @private
     */
    initializeValue_: function() {
      this.selectedValue_ =
          this.printTicketStore_.vendorItems.ticketItems[this.id] || null;

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
      setIsVisible(
        this.getChildElement('.advanced-settings-item-value-select'), true);
      var selectEl = this.select_;
      var indexToSelect = 0;
      this.capability_.select_cap.option.forEach(function(option, index) {
        var item = document.createElement('option');
        item.text = option.display_name;
        item.value = option.value;
        if (option.is_default)
          indexToSelect = index;
        selectEl.add(item);
      });
      for (var i = 0, option; option = selectEl.options[i]; i++) {
        if (option.value == this.selectedValue_) {
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
      setIsVisible(
          this.getChildElement('.advanced-settings-item-value-text'), true);
      this.text_.value = this.selectedValue;
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
