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
  var displayRootPromise = backgroundComponentsPromise.then(
      function(backgroundComponent) {
        var volumeManager = backgroundComponent.volumeManager;
        var volumeInfo = volumeManager.getCurrentProfileVolumeInfo(volumeType);
        return new Promise(function(fulfill, reject) {
          volumeInfo.resolveDisplayRoot(fulfill, reject);
        });
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
 * Launches the Gallery app with the test entries.
 *
 * @param {string} testVolumeName Test volume name passed to the addEntries
 *     function. Either 'drive' or 'local'.
 * @param {VolumeManagerCommon.VolumeType} volumeType Volume type.
 * @param {Array.<TestEntryInfo>} entries Entries to be parepared and passed to
 *     the application.
 * @param {Array.<TestEntryInfo>=} opt_selected Entries to be selected. Should
 *     be a sub-set of the entries argument.
 */
function launchWithTestEntries(
    testVolumeName, volumeType, entries, opt_selected) {
  var entriesPromise = addEntries([testVolumeName], entries).then(function() {
    var selectedEntries = opt_selected || entries;
    return getFilesUnderVolume(
        volumeType,
        selectedEntries.map(function(entry) { return entry.nameText; }));
  });
  return launch(entriesPromise).then(function() {
    var launchedPromise = Promise.all([appWindowPromise, entriesPromise]);
    return launchedPromise.then(function(results) {
      return {appWindow: results[0], entries: results[1]};
    });
  });
}

/**
 * Waits until the expected image is shown.
 *
 * @param {document} document Document.
 * @param {number} width Expected width of the image.
 * @param {number} height Expected height of the image.
 * @param {string} name Expected name of the image.
 * @return {Promise} Promsie to be fulfilled when the check is passed.
 */
function waitForSlideImage(document, width, height, name) {
  var expected = {width: width, height: height, name: name};
  return repeatUntil(function() {
    var fullResCanvas = document.querySelector(
        '.gallery[mode="slide"] .content canvas.fullres');
    var nameBox = document.querySelector('.namebox');
    var actual = {
      width: fullResCanvas && fullResCanvas.width,
      height: fullResCanvas && fullResCanvas.height,
      name: nameBox && nameBox.value
    };
    if (!chrome.test.checkDeepEq(expected, actual)) {
      return pending('Slide mode state, expected is %j, actual is %j.',
                     expected, actual);
    }
    return actual;
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
