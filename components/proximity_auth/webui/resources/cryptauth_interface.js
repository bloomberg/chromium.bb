// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Responsible for interfacing with the native component of the WebUI to make
 * CryptAuth API requests and handling the responses.
 */
CryptAuthInterface = {
  /**
   * A list of observers of CryptAuth events.
   */
  observers_: [],

  /**
   * Adds an observer.
   */
  addObserver: function(observer) {
    CryptAuthInterface.observers_.push(observer);
  },

  /**
   * Removes an observer.
   */
  removeObserver: function(observer) {
    CryptAuthInterface.observers_.remove(observer);
  },

  /**
   * Starts the findEligibleUnlockDevices API call.
   * The onGotEligibleDevices() function will be called upon success.
   */
  findEligibleUnlockDevices: function() {
    chrome.send('findEligibleUnlockDevices');
  },

  /**
   * Starts the flow to find reachable devices. Reachable devices are those that
   * respond to a CryptAuth ping.
   * The onGotReachableDevices() function will be called upon success.
   */
  findReachableDevices: function() {
    chrome.send('findReachableDevices');
  },

  /**
   * Called by the browser when the API request fails.
   */
  onError: function(errorMessage) {
    CryptAuthInterface.observers_.forEach(function(observer) {
      if (observer.onCryptAuthError != null)
        observer.onCryptAuthError(errorMessage);
    });
  },

  /**
   * Called by the browser when a findEligibleUnlockDevices completes
   * successfully.
   * @param {Array<DeviceInfo>} eligibleDevices
   * @param {Array<DeviceInfo>} ineligibleDevices
   */
  onGotEligibleDevices: function(eligibleDevices, ineligibleDevices) {
    CryptAuthInterface.observers_.forEach(function(observer) {
      if (observer.onGotEligibleDevices != null)
        observer.onGotEligibleDevices(eligibleDevices, ineligibleDevices);
    });
  },

  /**
   * Called by the browser when the reachable devices flow completes
   * successfully.
   * @param {Array<DeviceInfo>} reachableDevices
   */
  onGotReachableDevices: function(reachableDevices) {
    CryptAuthInterface.observers_.forEach(function(observer) {
      if (observer.onGotReachableDevices != null)
        observer.onGotReachableDevices(reachableDevices);
    });
  },
};
