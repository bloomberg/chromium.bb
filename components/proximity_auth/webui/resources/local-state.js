// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer('local-state', {
  publish: {
    /**
     * The current CryptAuth enrollment status.
     * @type {{
     *   lastSuccessTime: ?number,
     *   nextRefreshTime: string,
     *   lastAttemptFailed: boolean,
     * }}
     */
    enrollmentInfo: null,

    /**
     * The current CryptAuth device sync status.
     * @type {{
     *   lastSuccessTime: ?number,
     *   nextRefreshTime: string,
     *   lastAttemptFailed: boolean,
     * }}
     */
    deviceSyncInfo: null,

    /**
     * List of unlock keys that can unlock the local device.
     * @type {Array<DeviceInfo>}
     */
    unlockKeys: null,
  },

  /**
   * Called when an instance is created.
   */
  created: function() {
    this.enrollmentInfo = {
      lastSuccessTime: null,
      nextRefreshTime: '90 days',
      lastAttemptFailed: true
    };

    this.deviceSyncInfo = {
      lastSuccessTime: 'April 20 14:23',
      nextRefreshTime: '15.5 hours',
      lastAttemptFailed: false
    };

    this.unlockKeys = [
      {
       publicKey: 'CAESRQogOlH8DgPMQu7eAt-b6yoTXcazG8mAl6SPC5Ds-LTULIcSIQDZDM' +
                  'qsoYRO4tNMej1FBEl1sTiTiVDqrcGq-CkYCzDThw==',
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
    ];
  },
});
