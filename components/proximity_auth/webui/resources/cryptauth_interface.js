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
   */
  findEligibleUnlockDevices: function() {
    chrome.send('findEligibleUnlockDevices');
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
   */
  onGotEligibleDevices: function(eligibleDevices, ineligibleDevices) {
    CryptAuthInterface.observers_.forEach(function(observer) {
      if (observer.onGotEligibleDevices != null)
        observer.onGotEligibleDevices(eligibleDevices, ineligibleDevices);
    });
  }
};
