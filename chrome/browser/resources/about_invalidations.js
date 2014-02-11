// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('chrome.invalidations', function() {

  function quote(str) {
    return '\"' + str + '\"';
  }

  function nowTimeString() {
    return '[' + new Date().getTime() + '] ';
  }

  /**
   * Appends a string to a textarea log.
   *
   * @param {string} logMessage The string to be appended.
   */
  function appendToLog(logMessage) {
    var invalidationsLog = $('invalidations-log');
    invalidationsLog.value += logMessage + '\n';
  }

  /**
   * Shows the current state of the InvalidatorService
   *
   * @param {string} newState The string to be displayed and logged.
   */
  function updateState(newState) {
    var logMessage = nowTimeString() +
      'Invalidations service state changed to ' + quote(newState);

    appendToLog(logMessage);
    $('invalidations-state').textContent = newState;
    currentInvalidationState = newState;
  }

  /**
   * Adds to the log the latest invalidations received
   *
   * @param {Array of Object} allInvalidations The array of ObjectId
   * that contains the invalidations received by the InvalidatorService
   */
  function logInvalidations(allInvalidations) {
    for (var i = 0; i < allInvalidations.length; i++) {
      var inv = allInvalidations[i];
      if (inv.hasOwnProperty('objectId')) {
        var logMessage = nowTimeString() +
          'Received Invalidation with type ' +
          quote(inv.objectId.name) +
          ' version ' +
          quote((inv.isUnknownVersion ? 'Unknown' : inv.version)) +
          ' with payload ' +
          quote(inv.payload);

        appendToLog(logMessage);
      }
    }
  }

  /**
   * Function that notifies the Invalidator Logger that the UI is
   * ready to receive real-time notifications.
   */
  function onLoadWork() {
    chrome.send('doneLoading');
  }

  return {
    updateState: updateState,
    logInvalidations: logInvalidations,
    onLoadWork: onLoadWork
  };
});

document.addEventListener('DOMContentLoaded', chrome.invalidations.onLoadWork);
