// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('cloudprint', function() {
  'use strict';

  /** @type {?cloudprint.CloudPrintInterface} */
  let instance = null;

  /**
   * @param {string} baseUrl Base part of the Google Cloud Print service URL
   *     with no trailing slash. For example,
   *     'https://www.google.com/cloudprint'.
   * @param {!print_preview.NativeLayer} nativeLayer Native layer instance.
   * @param {boolean} isInAppKioskMode Whether the print preview is in App
   *     Kiosk mode.
   * @return {!cloudprint.CloudPrintInterface}
   */
  function getCloudPrintInterface(baseUrl, nativeLayer, isInAppKioskMode) {
    if (instance === null) {
      if (loadTimeData.getBoolean('cloudPrinterHandlerEnabled')) {
        instance = new cloudprint.CloudPrintInterfaceNative();
      } else {
        instance = new cloudprint.CloudPrintInterfaceJS(
            baseUrl, nativeLayer, isInAppKioskMode);
      }
    }
    return instance;
  }

  /**
   * Sets the CloudPrintInterface singleton instance, useful for testing.
   * @param {!cloudprint.CloudPrintInterface} cloudPrintInterface
   */
  function setCloudPrintInterfaceForTesting(cloudPrintInterface) {
    instance = cloudPrintInterface;
  }

  // Export
  return {
    getCloudPrintInterface: getCloudPrintInterface,
    setCloudPrintInterfaceForTesting: setCloudPrintInterfaceForTesting,
  };
});
