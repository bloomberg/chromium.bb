// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Opens video player with a single video.
 * @param {string} volumeName Test volume name passed to the addEntries
 *     function. Either 'drive' or 'local'.
 * @param {VolumeManagerCommon.VolumeType} volumeType Volume type.
 * @return {Promise} Promise to be fulfilled with the video player element.
 */
function openSingleVideo(volumeName, volumeType) {
  var entries = [ENTRIES.world];
  return launch(volumeName, volumeType, entries).then(function(videoPlayer) {
    chrome.test.assertTrue(videoPlayer.hasAttribute('first-video'));
    chrome.test.assertTrue(videoPlayer.hasAttribute('last-video'));
    chrome.test.assertFalse(videoPlayer.hasAttribute('multiple'));
    chrome.test.assertFalse(videoPlayer.hasAttribute('disabled'));
    return videoPlayer;
  });
}

/**
 * The openSingleImage test for Downloads.
 * @return {Promise} Promise to be fulfilled with on success.
 */
function openSingleVideoOnDownloads() {
  var test = openSingleVideo('local', VolumeManagerCommon.VolumeType.DOWNLOADS);
  return test.then(function(videoPlayer) {
    chrome.test.assertFalse(videoPlayer.hasAttribute('cast-available'));
  });
}

/**
 * The openSingleImage test for Drive.
 * @return {Promise} Promise to be fulfilled with on success.
 */
function openSingleVideoOnDrive() {
  var test = openSingleVideo('drive', VolumeManagerCommon.VolumeType.DRIVE);
  return test.then(function(videoPlayer) {
    // TODO(yoshiki): flip this after launching the feature.
    chrome.test.assertFalse(videoPlayer.hasAttribute('cast-available'));
  });
}
