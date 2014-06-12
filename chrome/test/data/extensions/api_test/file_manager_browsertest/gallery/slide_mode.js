// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

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
 * Runs a test to traverse images in the slide mode.
 *
 * @param {string} testVolumeName Test volume name passed to the addEntries
 *     function. Either 'drive' or 'local'.
 * @param {VolumeManagerCommon.VolumeType} volumeType Volume type.
 * @return {Promise} Promise to be fulfilled with on success.
 */
function traverseSlideImages(testVolumeName, volumeType) {
  var testEntries = [ENTRIES.desktop, ENTRIES.image2, ENTRIES.image3];
  var launchedPromise = launchWithTestEntries(
      testVolumeName, volumeType, testEntries, testEntries.slice(0, 1));
  var appWindow;
  var clickElement = function(element) { element.click(); };
  return launchedPromise.then(function(args) {
    appWindow = args.appWindow;
    return waitForSlideImage(appWindow.contentWindow.document,
                             800, 600, 'My Desktop Background');
  }).then(function() {
    return waitForElement(appWindow, '.arrow.right').then(clickElement);
  }).then(function() {
    return waitForSlideImage(appWindow.contentWindow.document,
                             1024, 768, 'image2');
  }).then(function() {
    return waitForElement(appWindow, '.arrow.right').then(clickElement);
  }).then(function() {
    return waitForSlideImage(appWindow.contentWindow.document,
                             640, 480, 'image3');
  }).then(function() {
    return waitForElement(appWindow, '.arrow.right').then(clickElement);
  }).then(function() {
    return waitForSlideImage(appWindow.contentWindow.document,
                             800, 600, 'My Desktop Background');
  });
}

/**
 * Runs a test to rename an image.
 *
 * @param {string} testVolumeName Test volume name passed to the addEntries
 *     function. Either 'drive' or 'local'.
 * @param {VolumeManagerCommon.VolumeType} volumeType Volume type.
 * @return {Promise} Promise to be fulfilled with on success.
 */
function renameImage(testVolumeName, volumeType) {
  var launchedPromise = launchWithTestEntries(
      testVolumeName, volumeType, [ENTRIES.desktop]);
  var appWindow;
  return launchedPromise.then(function(args) {
    appWindow = args.appWindow;
    return waitForSlideImage(appWindow.contentWindow.document,
                             800, 600, 'My Desktop Background');
  }).then(function() {
    var nameBox = appWindow.contentWindow.document.querySelector('.namebox');
    nameBox.focus();
    nameBox.value = 'New Image Name';
    nameBox.blur();
    return waitForSlideImage(appWindow.contentWindow.document,
                             800, 600, 'New Image Name');
  }).then(function() {
    return repeatUntil(function() {
      return getFilesUnderVolume(volumeType, ['New Image Name.png']).then(
          function() { return true; },
          function() { return pending('"New Image Name.png" is not found.'); });
    });
  });
}

/**
 * The traverseSlideImages test for Downloads.
 * @return {Promise} Promise to be fulfilled with on success.
 */
function traverseSlideImagesOnDownloads() {
  return traverseSlideImages('local', VolumeManagerCommon.VolumeType.DOWNLOADS);
}

/**
 * The traverseSlideImages test for Google Drive.
 * @return {Promise} Promise to be fulfilled with on success.
 */
function traverseSlideImagesOnDrive() {
  return traverseSlideImages('drive', VolumeManagerCommon.VolumeType.DRIVE);
}

/**
 * The renameImage test for Downloads.
 * @return {Promise} Promise to be fulfilled with on success.
 */
function renameImageOnDownloads() {
  return renameImage('local', VolumeManagerCommon.VolumeType.DOWNLOADS);
}

/**
 * The renameImage test for Google Drive.
 * @return {Promise} Promise to be fulfilled with on success.
 */
function renameImageOnDrive() {
  return renameImage('drive', VolumeManagerCommon.VolumeType.DRIVE);
}
