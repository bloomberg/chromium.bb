// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @typedef {{device: string,
 *           lastUpdateTime: string,
 *           separatorIndexes: !Array<number>,
 *           timestamp: number,
 *           tabs: !Array<!ForeignSessionTab>,
 *           tag: string}}
 */
var ForeignDeviceInternal;

Polymer({
  is: 'history-synced-device-manager',

  properties: {
    /**
     * @type {?Array<!ForeignSession>}
     */
    sessionList: {
      type: Array,
      observer: 'updateSyncedDevices'
    },

    searchedTerm: {
      type: String,
      observer: 'searchTermChanged'
    },

    /**
     * An array of synced devices with synced tab data.
     * @type {!Array<!ForeignDeviceInternal>}
     */
    syncedDevices_: {
      type: Array,
      value: function() { return []; }
    }
  },

  /**
   * @param {!ForeignSession} session
   * @return {!ForeignDeviceInternal}
   */
  createInternalDevice_: function(session) {
    var tabs = [];
    var separatorIndexes = [];
    for (var i = 0; i < session.windows.length; i++) {
      var newTabs = session.windows[i].tabs;
      if (newTabs.length == 0)
        continue;


      if (!this.searchedTerm) {
        // Add all the tabs if there is no search term.
        tabs = tabs.concat(newTabs);
        separatorIndexes.push(tabs.length - 1);
      } else {
        var searchText = this.searchedTerm.toLowerCase();
        var windowAdded = false;
        for (var j = 0; j < newTabs.length; j++) {
          var tab = newTabs[j];
          if (tab.title.toLowerCase().indexOf(searchText) != -1) {
            tabs.push(tab);
            windowAdded = true;
          }
        }
        if (windowAdded)
          separatorIndexes.push(tabs.length - 1);
      }

    }
    return {
      device: session.name,
      lastUpdateTime: '– ' + session.modifiedTime,
      separatorIndexes: separatorIndexes,
      timestamp: session.timestamp,
      tabs: tabs,
      tag: session.tag,
    };
  },

  /**
   * Replaces the currently displayed synced tabs with |sessionList|. It is
   * common for only a single session within the list to have changed, We try to
   * avoid doing extra work in this case. The logic could be more intelligent
   * about updating individual tabs rather than replacing whole sessions, but
   * this approach seems to have acceptable performance.
   * @param {?Array<!ForeignSession>} sessionList
   */
  updateSyncedDevices: function(sessionList) {
    if (!sessionList)
      return;

    // First, update any existing devices that have changed.
    var updateCount = Math.min(sessionList.length, this.syncedDevices_.length);
    for (var i = 0; i < updateCount; i++) {
      var oldDevice = this.syncedDevices_[i];
      if (oldDevice.tag != sessionList[i].tag ||
          oldDevice.timestamp != sessionList[i].timestamp) {
        this.splice(
            'syncedDevices_', i, 1, this.createInternalDevice_(sessionList[i]));
      }
    }

    // Then, append any new devices.
    for (var i = updateCount; i < sessionList.length; i++) {
      this.push('syncedDevices_', this.createInternalDevice_(sessionList[i]));
    }
  },

  searchTermChanged: function(searchedTerm) {
    this.syncedDevices_ = [];
    this.updateSyncedDevices(this.sessionList);
  }
});
