// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'history-synced-device-card',

  properties: {
    // Name of the synced device.
    device: String,

    // When the device information was last updated.
    lastUpdateTime: String,

    /**
     * The list of tabs open for this device.
     * @type {!Array<!ForeignSessionTab>}
     */
    tabs: {
      type: Array,
      value: function() { return []; },
      observer: 'updateIcons_'
    },

    /**
     * The indexes where a window separator should be shown. The use of a
     * separate array here is necessary for window separators to appear
     * correctly in search. See http://crrev.com/2022003002 for more details.
     * @type {!Array<number>}
     */
    separatorIndexes: Array,

    // Whether the card is open.
    cardOpen_: {type: Boolean, value: true},

    searchTerm: String,

    // Internal identifier for the device.
    sessionTag: String,
  },

  /**
   * @param {TapEvent} e
   * @private
   */
  openTab_: function(e) {
    var tab = /** @type {ForeignSessionTab} */(e.model.tab);
    var srcEvent = /** @type {Event} */(e.detail.sourceEvent);
    md_history.BrowserService.getInstance().openForeignSessionTab(
        this.sessionTag, tab.windowId, tab.sessionId, srcEvent);
    e.preventDefault();
  },

  /**
   * Toggles the dropdown display of synced tabs for each device card.
   */
  toggleTabCard: function() {
    this.$.collapse.toggle();
    this.$['dropdown-indicator'].icon =
        this.$.collapse.opened ? 'cr:expand-less' : 'cr:expand-more';
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
  },

  /** @private */
  isWindowSeparatorIndex_: function(index, separatorIndexes) {
    return this.separatorIndexes.indexOf(index) != -1;
  },

  /**
   * @param {boolean} cardOpen
   * @return {string}
   */
  getCollapseTitle_: function(cardOpen) {
    return cardOpen ? loadTimeData.getString('collapseSessionButton') :
                      loadTimeData.getString('expandSessionButton');
  },

  /**
   * @param {CustomEvent} e
   * @private
   */
  onMenuButtonTap_: function(e) {
    this.fire('toggle-menu', {
      target: Polymer.dom(e).localTarget,
      tag: this.sessionTag
    });
    e.stopPropagation();  // Prevent iron-collapse.
  },
});
