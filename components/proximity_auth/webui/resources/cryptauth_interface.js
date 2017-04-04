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
    var index = CryptAuthInterface.observers_.indexOf(observer);
    if (observer)
      CryptAuthInterface.observers_.splice(index, 1);
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
   * Makes the device with |publicKey| an unlock key if |makeUnlockKey| is true.
   * Otherwise, the device will be removed as an unlock key.
   */
  toggleUnlockKey: function(publicKey, makeUnlockKey) {
    chrome.send('toggleUnlockKey', [publicKey, makeUnlockKey]);
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

  /*
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

  /**
   * Called by the browser when an unlock key is toggled.
   */
  onUnlockKeyToggled: function() {
    CryptAuthInterface.observers_.forEach(function(observer) {
      if (observer.onUnlockKeyToggled != null)
        observer.onUnlockKeyToggled();
    });
  },
};

// This message tells the native WebUI handler that the WebContents backing the
// WebUI has been iniitalized. This signal allows the native handler to execute
// JavaScript inside the page.
chrome.send('onWebContentsInitialized');
