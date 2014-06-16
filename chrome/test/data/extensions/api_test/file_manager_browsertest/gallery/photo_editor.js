// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

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
