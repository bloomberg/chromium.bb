// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'local-state',
  properties: {
    /**
     * The current CryptAuth enrollment status.
     * @type {{
     *   lastSuccessTime: ?number,
     *   nextRefreshTime: string,
     *   lastAttemptFailed: boolean,
     * }}
     */
    enrollmentInfo: {
      type: Object,
      value: {
        lastSuccessTime: null,
        nextRefreshTime: '90 days',
        lastAttemptFailed: true
      },
      notify: true,
    },

    /**
     * The current CryptAuth device sync status.
     * @type {{
     *   lastSuccessTime: ?number,
     *   nextRefreshTime: string,
     *   lastAttemptFailed: boolean,
     * }}
     */
    deviceSyncInfo: {
      type: Object,
      value: {
        lastSuccessTime: 'April 20 14:23',
        nextRefreshTime: '15.5 hours',
        lastAttemptFailed: false
      },
      notify: true,
    },

    /**
     * List of unlock keys that can unlock the local device.
     * @type {Array<DeviceInfo>}
     */
    unlockKeys: {
      type: Array,
      value: [
        {
         publicKey: 'CAESRQogOlH8DgPMQu7eAt-b6yoTXcazG8mAl6SPC5Ds-LTULIcSIQDZ' +
                    'DMqsoYRO4tNMej1FBEl1sTiTiVDqrcGq-CkYCzDThw==',
         friendlyDeviceName: 'LGE Nexus 4',
         bluetoothAddress: 'C4:43:8F:12:07:07',
         unlockKey: true,
         unlockable: false,
         connectionStatus: 'connected',
         remoteState: {
           userPresent: true,
           secureScreenLock: true,
           trustAgent: true
         },
        },
      ],
      notify: true,
    },
  },

  /**
   * @param {SyncInfo} syncInfo
   * @param {string} neverSyncedString
   * @return {string}
   */
  getLastSyncTimeString_: function(syncInfo, neverSyncedString) {
    return syncInfo.lastSuccessTime || neverSyncedString;
  },

  /**
   * @param {SyncInfo} syncInfo
   * @return {string}
   */
  getNextEnrollmentString_: function(syncInfo) {
    return syncInfo.nextRefreshTime + ' to refresh';
  },

  /**
   * @param {SyncInfo} syncInfo
   * @return {string}
   */
  getIconForLastAttempt_: function(syncInfo) {
    return syncInfo.lastAttemptFailed ?
        'icons:error' : 'icons:cloud-done';
  },
});
