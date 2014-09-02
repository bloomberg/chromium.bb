// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Launches the video player with the given entries.
 *
 * @param {string} testVolumeName Test volume name passed to the addEntries
 *     function. Either 'drive' or 'local'.
 * @param {VolumeManagerCommon.VolumeType} volumeType Volume type.
 * @param {Array.<TestEntryInfo>} entries Entries to be parepared and passed to
 *     the application.
 * @param {Array.<TestEntryInfo>=} opt_selected Entries to be selected. Should
 *     be a sub-set of the entries argument.
 * @return {Promise} Promise to be fulfilled with the video player element.
 */
function launch(testVolumeName, volumeType, entries, opt_selected) {
  var entriesPromise = addEntries([testVolumeName], entries).then(function() {
    var selectedEntries = opt_selected || entries;
    return getFilesUnderVolume(
        volumeType,
        selectedEntries.map(function(entry) { return entry.nameText; }));
  });

  var appWindow = null;
  return entriesPromise.then(function(entries) {
    return open(entries.map(function(entry) {
      return {entry: entry, title: entry.name, url: entry.toURL()};
    })).then(function() {
      appWindow = appWindowsForTest[entries[0].name];
    });
  }).then(function() {
    return waitForElement(appWindow, 'body').then(function() {
      var script = document.createElement('script');
      script.src =
          'chrome-extension://ljoplibgfehghmibaoaepfagnmbbfiga/' +
          'video_player/test_helper_on_ui_page.js';
      appWindow.contentWindow.document.body.appendChild(script);
    });
  }).then(function() {
    return Promise.all([
      waitForElement(appWindow, '#video-player[first-video][last-video]'),
      waitForElement(appWindow, '.play.media-button[state="playing"]'),
    ]).then(function(args) {
      return [appWindow, args[0]];
    });
  });
}

/**
 * Loads the mock cast extension to the content page.
 * @param {AppWindow} appWindow The target video player window.
 */
function loadMockCastExtesntion(appWindow) {
  var script = document.createElement('script');
  script.src =
      'chrome-extension://ljoplibgfehghmibaoaepfagnmbbfiga/' +
      'cast_extension_mock/load.js';
  appWindow.contentWindow.document.body.appendChild(script);
}
