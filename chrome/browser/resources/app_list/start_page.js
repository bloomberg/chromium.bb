// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview App launcher start page implementation.
 */

cr.define('appList.startPage', function() {
  'use strict';

  // The element containing the current Google Doodle.
  var doodle = null;

  // TODO(calamity): This is used for manual inspection of the doodle data.
  // Remove this once http://crbug.com/462082 is diagnosed.
  var doodleData = null;

  /**
   * Initialize the page.
   */
  function initialize() {
    chrome.send('initialize');
  }

  /**
   * Invoked when the hotword plugin availability is changed.
   *
   * @param {boolean} enabled Whether the plugin is enabled or not.
   */
  function setHotwordEnabled(enabled) {
  }

  /**
   * Sets the architecture of NaCl module to be loaded for hotword.
   * @param {string} arch The architecture.
   */
  function setNaclArch(arch) {
  }

  /**
   * Invoked when the app-list bubble is shown.
   *
   * @param {boolean} hotwordEnabled Whether the hotword is enabled or not.
   */
  function onAppListShown(hotwordEnabled, legacySpeechEnabled) {

    chrome.send('appListShown', [this.doodle != null]);
  }

  /**
   * Sets the doodle's visibility, hiding or showing the default logo.
   *
   * @param {boolean} visible Whether the doodle should be made visible.
   */
  function setDoodleVisible(visible) {
    var doodle = $('doodle');
    var defaultLogo = $('default_logo');
    if (visible) {
      doodle.style.display = 'flex';
      defaultLogo.style.display = 'none';
    } else {
      if (doodle)
        doodle.style.display = 'none';

      defaultLogo.style.display = 'block';
    }
  }

  /**
   * Invoked when the app-list doodle is updated.
   *
   * @param {Object} data The data object representing the current doodle.
   */
  function onAppListDoodleUpdated(data, base_url) {
    if (this.doodle) {
      this.doodle.parentNode.removeChild(this.doodle);
      this.doodle = null;
    }

    var doodleData = data.ddljson;
    this.doodleData = doodleData;
    if (!doodleData || !doodleData.transparent_large_image) {
      setDoodleVisible(false);
      return;
    }

    // Set the page's base URL so that links will resolve relative to the Google
    // homepage.
    $('base').href = base_url;

    this.doodle = document.createElement('div');
    this.doodle.id = 'doodle';
    this.doodle.style.display = 'none';

    var doodleImage = document.createElement('img');
    doodleImage.id = 'doodle_image';
    if (doodleData.alt_text) {
      doodleImage.alt = doodleData.alt_text;
      doodleImage.title = doodleData.alt_text;
    }

    doodleImage.onload = function() {
      setDoodleVisible(true);
    };
    doodleImage.src = doodleData.transparent_large_image.url;

    if (doodleData.target_url) {
      var doodleLink = document.createElement('a');
      doodleLink.id = 'doodle_link';
      doodleLink.href = doodleData.target_url;
      doodleLink.target = '_blank';
      doodleLink.appendChild(doodleImage);
      doodleLink.onclick = function() {
        chrome.send('doodleClicked');
        return true;
      };
      this.doodle.appendChild(doodleLink);
    } else {
      this.doodle.appendChild(doodleImage);
    }
    $('logo_container').appendChild(this.doodle);
  }

  /**
   * Invoked when the app-list bubble is hidden.
   */
  function onAppListHidden() {
  }

  /**
   * Invoked when the user explicitly wants to toggle the speech recognition
   * state.
   */
  function toggleSpeechRecognition() {
  }

  return {
    initialize: initialize,
    setHotwordEnabled: setHotwordEnabled,
    setNaclArch: setNaclArch,
    onAppListDoodleUpdated: onAppListDoodleUpdated,
    onAppListShown: onAppListShown,
    onAppListHidden: onAppListHidden,
    toggleSpeechRecognition: toggleSpeechRecognition
  };
});

// TODO(calamity): Suppress context the menu once http://crbug.com/462082 is
// diagnosed.
document.addEventListener('DOMContentLoaded', appList.startPage.initialize);
