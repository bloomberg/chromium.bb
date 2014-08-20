// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Gets file entries just under the volume.
 *
 * @param {VolumeManagerCommon.VolumeType} volumeType Volume type.
 * @param {Array.<string>} names File name list.
 * @return {Promise} Promise to be fulflled with Array.<FileEntry>.
 */
function getFilesUnderVolume(volumeType, names) {
  var displayRootPromise =
      VolumeManager.getInstance().then(function(volumeManager) {
    var volumeInfo = volumeManager.getCurrentProfileVolumeInfo(volumeType);
    return volumeInfo.resolveDisplayRoot();
  });

  return displayRootPromise.then(function(displayRoot) {
    var filesPromise = names.map(function(name) {
      return new Promise(
          displayRoot.getFile.bind(displayRoot, name, {}));
    });
    return Promise.all(filesPromise);
  });
}

/**
 * Waits until an element appears and returns it.
 *
 * @param {AppWindow} appWindow Application window.
 * @param {string} query Query for the element.
 * @return {Promise} Promise to be fulfilled with the element.
 */
function waitForElement(appWindow, query) {
  return repeatUntil(function() {
    var element = appWindow.contentWindow.document.querySelector(query);
    if (element)
      return element;
    else
      return pending('The element %s is not found.', query);
  });
}

/**
 * Waits until an element disappears.
 *
 * @param {AppWindow} appWindow Application window.
 * @param {string} query Query for the element.
 * @return {Promise} Promise to be fulfilled with the element.
 */
function waitForElementLost(appWindow, query) {
  return repeatUntil(function() {
    var element = appWindow.contentWindow.document.querySelector(query);
    if (element)
      return pending('The element %s does not disappear.', query);
    else
      return true;
  });
}

/**
 * Shorthand for clicking an element.
 * @param {AppWindow} appWindow Application window.
 * @param {string} query Query for the element.
 * @param {Promise} Promise to be fulfilled with the clicked element.
 */
function waitAndClickElement(appWindow, query) {
  return waitForElement(appWindow, query).then(function(element) {
    element.click();
    return element;
  });
}

/**
 * Sends a fake key down event.
 *
 * @param {AppWindow} appWindow Application window.
 * @param {string} query Query for the element to be dispatched an event to.
 * @param {string} keyIdentifier Key identifier.
 * @return {boolean} True on success.
 */
function sendKeyDown(appWindow, query, keyIdentifier) {
  return appWindow.contentWindow.document.querySelector(query).dispatchEvent(
      new KeyboardEvent(
          'keydown',
          {bubbles: true, keyIdentifier: keyIdentifier}));
}

/**
 * Observes window errors that should fail the browser tests.
 * @param {DOMWindow} window Windows to be obserbed.
 */
function observeWindowError(window) {
  window.onerror = function(error) {
    chrome.test.fail('window.onerror: ' + error);
  };
}
