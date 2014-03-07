// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('gcmInternals', function() {
  'use strict';

  /**
   * If the info dictionary has property prop, then set the text content of
   * element to the value of this property.
   * @param {!Object} info A dictionary of device infos to be displayed.
   * @param {string} prop Name of the property.
   * @param {string} element The id of a HTML element.
   */
  function setIfExists(info, prop, element) {
    if (info[prop] !== undefined) {
      $(element).textContent = info[prop];
    }
  }

  /**
   * Display device informations.
   * @param {!Object} info A dictionary of device infos to be displayed.
   */
  function displayDeviceInfo(info) {
    setIfExists(info, 'androidId', 'android-id');
    setIfExists(info, 'profileServiceCreated', 'profile-service-created');
    setIfExists(info, 'gcmEnabledState', 'gcm-enabled-state');
    setIfExists(info, 'signedInUserName', 'signed-in-username');
    setIfExists(info, 'gcmClientCreated', 'gcm-client-created');
    setIfExists(info, 'gcmClientState', 'gcm-client-state');
    setIfExists(info, 'gcmClientReady', 'gcm-client-ready');
    setIfExists(info, 'connectionClientCreated', 'connection-client-created');
    setIfExists(info, 'connectionState', 'connection-state');
  }

  function initialize() {
    chrome.send('getGcmInternalsInfo');
  }

  /**
   * Callback function accepting a dictionary of info items to be displayed.
   * @param {!Object} infos A dictionary of info items to be displayed.
   */
  function setGcmInternalsInfo(infos) {
    if (infos.deviceInfo !== undefined) {
      displayDeviceInfo(infos.deviceInfo);
    }
  }

  // Return an object with all of the exports.
  return {
    initialize: initialize,
    setGcmInternalsInfo: setGcmInternalsInfo,
  };
});

document.addEventListener('DOMContentLoaded', gcmInternals.initialize);
