// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Waits for the "Press Enter" message.
 *
 * @param {AppWindow} appWindow App window.
 * @return {Promise} Promise to be fulfilled when the element appears.
 */
function waitForPressEnterMessage(appWindow) {
  return waitForElement(appWindow, '.prompt-wrapper .prompt').
      then(function(element) {
        chrome.test.assertEq(
            'Press Enter when done', element.innerText.trim());
      });
}

/**
 * Prepares the photo editor.
 *
 * @param {string} testVolumeName Test volume name passed to the addEntries
 *     function. Either 'drive' or 'local'.
 * @param {VolumeManagerCommon.VolumeType} volumeType Volume type.
 * @return {Promise} Promise to be fulfilled with on success.
 */
function setupPhotoEditor(testVolumeName, volumeType) {
  // Lauch the gallery.
  var launchedPromise = launchWithTestEntries(
      testVolumeName,
      volumeType,
      [ENTRIES.desktop]);
  return launchedPromise.then(function(args) {
    var appWindow = args.appWindow;

    // Show the slide image.
    var slideImagePromise = waitForSlideImage(
        appWindow.contentWindow.document,
        800,
        600,
        'My Desktop Background');

    // Lauch the photo editor.
    var photoEditorPromise = slideImagePromise.then(function() {
      return waitAndClickElement(
          appWindow, 'button.edit');
    });

    return photoEditorPromise.then(function() {
      return args;
    });
  });
}

/**
 * Tests to rotate an image.
 *
 * @param {string} testVolumeName Test volume name passed to the addEntries
 *     function. Either 'drive' or 'local'.
 * @param {VolumeManagerCommon.VolumeType} volumeType Volume type.
 * @return {Promise} Promise to be fulfilled with on success.
 */
function rotateImage(testVolumeName, volumeType) {
  var launchedPromise = setupPhotoEditor(testVolumeName, volumeType);
  return launchedPromise.then(function(args) {
    var appWindow = args.appWindow;
    return waitAndClickElement(
        appWindow, '.gallery:not([locked]) button.rotate_right').
        then(function() {
          return waitForSlideImage(
              appWindow.contentWindow.document,
              600,
              800,
              'My Desktop Background');
        }).
        then(function() {
          return waitAndClickElement(
              appWindow, '.gallery:not([locked]) button.rotate_left');
        }).
        then(function() {
          return waitForSlideImage(
              appWindow.contentWindow.document,
              800,
              600,
              'My Desktop Background');
        });
  });
}

/**
 * Tests to crop an image.
 *
 * @param {string} testVolumeName Test volume name passed to the addEntries
 *     function. Either 'drive' or 'local'.
 * @param {VolumeManagerCommon.VolumeType} volumeType Volume type.
 * @return {Promise} Promise to be fulfilled with on success.
 */
function cropImage(testVolumeName, volumeType) {
  var launchedPromise = setupPhotoEditor(testVolumeName, volumeType);
  return launchedPromise.then(function(args) {
    var appWindow = args.appWindow;
    return waitAndClickElement(appWindow, '.gallery:not([locked]) button.crop').
        then(function() {
          return Promise.all([
            waitForPressEnterMessage(appWindow),
            waitForElement(appWindow, '.crop-overlay')
          ]);
        }).
        then(function() {
          chrome.test.assertTrue(sendKeyDown(appWindow, 'body', 'Enter'));
        }).
        then(function() {
          return Promise.all([
            waitForElementLost(appWindow, '.prompt-wrapper .prompt'),
            waitForElementLost(appWindow, '.crop-overlay')
          ]);
        }).
        then(function() {
          return waitForSlideImage(
              appWindow.contentWindow.document,
              534,
              400,
              'My Desktop Background');
        });
  });
}

/**
 * Obtains metadata from an entry.
 *
 * @param {Entry} entry Entry.
 * @return {Promise} Promise to be fulfilled with the result metadata.
 */
function getMetadata(entry) {
  return new Promise(entry.getMetadata.bind(entry));
}

/**
 * Tests to exposure an image.
 *
 * @param {string} testVolumeName Test volume name passed to the addEntries
 *     function. Either 'drive' or 'local'.
 * @param {VolumeManagerCommon.VolumeType} volumeType Volume type.
 * @return {Promise} Promise to be fulfilled with on success.
 */
function exposureImage(testVolumeName, volumeType) {
  var launchedPromise = setupPhotoEditor(testVolumeName, volumeType);
  return launchedPromise.then(function(args) {
    var appWindow = args.appWindow;
    var entry = args.entries[0];
    var buttonQuery = '.gallery:not([locked]) button.exposure';

    // Click the exposure button.
    return waitAndClickElement(appWindow, buttonQuery).then(function() {
      // Wait until the edit controls appear.
      return Promise.all([
        waitForPressEnterMessage(appWindow),
        waitForElement(appWindow, 'input.range[name="brightness"]'),
        waitForElement(appWindow, 'input.range[name="contrast"]'),
      ]);
    }).then(function(results) {
      // Update bright.
      var brightnessRange = results[1];
      brightnessRange.value = 20;
      chrome.test.assertTrue(
          brightnessRange.dispatchEvent(new Event('change')));

      // Update contrast.
      var contrastRange = results[2];
      contrastRange.value = -20;
      chrome.test.assertTrue(
          contrastRange.dispatchEvent(new Event('change')));

      return getMetadata(entry).then(function(firstMetadata) {
        // Push the Enter key.
        chrome.test.assertTrue(sendKeyDown(appWindow, 'body', 'Enter'));

        // Wait until the image is updated.
        return repeatUntil(function() {
          return getMetadata(entry).then(function(secondMetadata) {
            if (firstMetadata.modificationTime !=
                secondMetadata.modificationTime) {
              return true;
            } else {
              return pending(
                  '%s is not updated. ' +
                      'First last modified: %s, Second last modified: %s.',
                  entry.name,
                  firstMetadata.modificationTime.toString(),
                  secondMetadata.modificationTime.toString());
            }
          });
        });
      });
    });
  });
}

/**
 * The rotateImage test for Downloads.
 * @return {Promise} Promise to be fulfilled with on success.
 */
function rotateImageOnDownloads() {
  return rotateImage('local', VolumeManagerCommon.VolumeType.DOWNLOADS);
}

/**
 * The rotateImage test for Google Drive.
 * @return {Promise} Promise to be fulfilled with on success.
 */
function rotateImageOnDrive() {
  return rotateImage('drive', VolumeManagerCommon.VolumeType.DRIVE);
}

/**
 * The cropImage test for Downloads.
 * @return {Promise} Promise to be fulfilled with on success.
 */
function cropImageOnDownloads() {
  return cropImage('local', VolumeManagerCommon.VolumeType.DOWNLOADS);
}

/**
 * The cropImage test for Google Drive.
 * @return {Promise} Promise to be fulfilled with on success.
 */
function cropImageOnDrive() {
  return cropImage('drive', VolumeManagerCommon.VolumeType.DRIVE);
}

/**
 * The exposureImage test for Downloads.
 * @return {Promise} Promise to be fulfilled with on success.
 */
function exposureImageOnDownloads() {
  return exposureImage('local', VolumeManagerCommon.VolumeType.DOWNLOADS);
}

/**
 * The exposureImage test for Google Drive.
 * @return {Promise} Promise to be fulfilled with on success.
 */
function exposureImageOnDrive() {
  return exposureImage('drive', VolumeManagerCommon.VolumeType.DRIVE);
}
