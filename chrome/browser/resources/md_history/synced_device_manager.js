// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'history-synced-device-manager',

  properties: {
    /**
     * An array of synced devices with synced tab data.
     * @type {!Array<!{device: string,
     *                 lastUpdateTime: string,
     *                 tabs: !Array<!ForeignSessionTab>}>}
     */
    syncedDevices: {
      type: Array,
      value: function() { return []; }
    }
  },

  /**
   * Adds |sessionList| to the currently displayed synced tabs.
   * @param {!Array<!ForeignSession>} sessionList
   */
  addSyncedHistory: function(sessionList) {
    // TODO(calamity): Does not add more items onto the page when the
    // sessionList updates. Update the cards dynamically by refreshing the tab
    // list and last update time for each synced tab card.
    if (this.syncedDevices.length > 0)
      return;

    for (var i = 0; i < sessionList.length; i++) {
      var tabs = [];
      for (var j = 0; j < sessionList[i].windows.length; j++) {
        var newTabs = sessionList[i].windows[j].tabs;
        if (newTabs.length == 0)
          continue;

        tabs = tabs.concat(newTabs);
        tabs[tabs.length - 1].needsWindowSeparator = true;
      }

      this.push('syncedDevices', {
        device: sessionList[i].name,
        lastUpdateTime: '– ' + sessionList[i].modifiedTime,
        tabs: tabs,
      });
    }
  }
});
