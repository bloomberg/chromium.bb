// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Polymer class definition for 'oobe-i18n-dropdown'.
 */
(function() {


/**
 * Languages/keyboard descriptor to display
 * @type {!OobeTypes.LanguageDsc|!OobeTypes.IMEDsc}
 */
var I18nMenuItem;

Polymer({
  is: 'oobe-i18n-dropdown',

  properties: {
    /**
     * List of languages/keyboards to display
     * @type {!Array<I18nMenuItem>}
     */
    items: {
      type: Array,
      observer: 'onItemsChanged_',
    },

    /**
     * Accessibility label.
     * @type {!string}
     */
    label: {
      type: String,
    },
  },

  /**
   * This flag prevents recursive calls of observers and callbacks.
   */
  observersDisabled_: false,

  /**
   * @param {!{detail: !{item: { item: {!I18nMenuItem}}}}} event
   * @private
   */
  onSelect_: function(event) {
    if (this.observersDisabled_)
      return;

    var selectedModel = this.$.domRepeat.modelForElement(event.detail.item);
    if (!selectedModel.item)
      return;

    selectedModel.set('item.selected', true);
    this.fire('select-item', selectedModel.item);
  },

  onDeselect_: function(event) {
    if (this.observersDisabled_)
      return;

    var deSelectedModel = this.$.domRepeat.modelForElement(event.detail.item);
    if (!deSelectedModel.item)
      return;

    deSelectedModel.set('item.selected', false);
  },

  onItemsChanged_: function(items) {
    if (this.observersDisabled_)
      return;

    if (!items)
      return;

    this.observersDisabled_ = true;

    var index = items.findIndex(function(item) { return item.selected; });
    if (index != -1) {
      // This is needed for selectIndex() to pick up values from updated
      // this.items (for example, translated into other language).
      Polymer.dom.flush();

      if (this.$.listboxDropdown.selected == index) {
        // This is to force update real <input> element value even if selection
        // index has not changed.
        this.$.listboxDropdown.selectIndex(null);
      }
      this.$.listboxDropdown.selectIndex(index);
    }

    this.observersDisabled_ = false;
  },
});
})();
