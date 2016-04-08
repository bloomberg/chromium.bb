// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @typedef {{device: string,
 *           lastUpdateTime: string,
 *           timestamp: number,
 *           tabs: !Array<!ForeignSessionTab>,
 *           tag: string}}
 */
var ForeignDeviceInternal;

Polymer({
  is: 'history-synced-device-manager',

  properties: {
    /**
     * An array of synced devices with synced tab data.
     * @type {!Array<!ForeignDeviceInternal>}
     */
    syncedDevices: {
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
    for (var j = 0; j < session.windows.length; j++) {
      var newTabs = session.windows[j].tabs;
      if (newTabs.length == 0)
        continue;

      tabs = tabs.concat(newTabs);
      tabs[tabs.length - 1].needsWindowSeparator = true;
    }
    return {
      device: session.name,
      lastUpdateTime: '– ' + session.modifiedTime,
      timestamp: session.timestamp,
      tabs: tabs,
      tag: session.tag
    };
  },

  /**
   * Replaces the currently displayed synced tabs with |sessionList|. It is
   * common for only a single session within the list to have changed, We try to
   * avoid doing extra work in this case. The logic could be more intelligent
   * about updating individual tabs rather than replacing whole sessions, but
   * this approach seems to have acceptable performance.
   * @param {!Array<!ForeignSession>} sessionList
   */
  setSyncedHistory: function(sessionList) {
    // First, update any existing devices that have changed.
    var updateCount = Math.min(sessionList.length, this.syncedDevices.length);
    for (var i = 0; i < updateCount; i++) {
      var oldDevice = this.syncedDevices[i];
      if (oldDevice.tag != sessionList[i].tag ||
          oldDevice.timestamp != sessionList[i].timestamp) {
        this.splice(
            'syncedDevices', i, 1, this.createInternalDevice_(sessionList[i]));
      }
    }

    // Then, append any new devices.
    for (var i = updateCount; i < sessionList.length; i++) {
      this.push('syncedDevices', this.createInternalDevice_(sessionList[i]));
    }
  }
});
