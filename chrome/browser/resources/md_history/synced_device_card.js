// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'history-synced-device-card',

  properties: {
    // Name of the synced device.
    device: {
      type: String,
      value: ''
    },

    // When the device information was last updated.
    lastUpdateTime: {
      type: String,
      value: ''
    },

    /**
     * The list of tabs open for this device.
     * @type {!Array<!ForeignSessionTab>}
     */
    tabs: {
      type: Array,
      value: function() { return []; },
      observer: 'updateIcons_'
    },

    // Whether the card is open.
    cardOpen_: {
      type: Boolean,
      value: true
    },
  },

  /**
   * Opens all the tabs displayed on the device in separate tabs.
   * @private
   */
  openAllTabs_: function() {
    // TODO(calamity): add a warning if an excessive number of tabs will open.
    for (var i = 0; i < this.tabs.length; i++)
      window.open(this.tabs[i].url, '_blank');
  },

  /**
   * Toggles the dropdown display of synced tabs for each device card.
   */
  toggleTabCard: function() {
    this.$.collapse.toggle();
    this.$['dropdown-indicator'].icon =
        this.$.collapse.opened ? 'expand-less' : 'expand-more';
  },

  /**
   * When the synced tab information is set, the icon associated with the tab
   * website is also set.
   * @private
   */
  updateIcons_: function() {
    this.async(function() {
      var icons = Polymer.dom(this.root).querySelectorAll('.website-icon');

      for (var i = 0; i < this.tabs.length; i++) {
        icons[i].style.backgroundImage =
            cr.icon.getFaviconImageSet(this.tabs[i].url);
      }
    });
  }
});
