// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview App launcher start page implementation.
 */

<include src="speech_manager.js">

cr.define('appList.startPage', function() {
  'use strict';

  var speechManager = null;

  /**
   * Initialize the page.
   */
  function initialize() {
    speechManager = new speech.SpeechManager();
    chrome.send('initialize');
  }

  /**
   * Invoked when the hotword plugin availability is changed.
   *
   * @param {boolean} enabled Whether the plugin is enabled or not.
   */
  function setHotwordEnabled(enabled) {
    speechManager.setHotwordEnabled(enabled);
  }

  /**
   * Sets the architecture of NaCl module to be loaded for hotword.
   * @param {string} arch The architecture.
   */
  function setNaclArch(arch) {
    speechManager.setNaclArch(arch);
  }

  /**
   * Invoked when the app-list bubble is shown.
   *
   * @param {boolean} hotwordEnabled Whether the hotword is enabled or not.
   */
  function onAppListShown(hotwordEnabled) {
    speechManager.onShown(hotwordEnabled);
  }

  /**
   * Invoked when the app-list doodle is updated.
   *
   * @param {Object} data The data object representing the current doodle.
   */
  function onAppListDoodleUpdated(data, base_url) {
    var defaultLogo = $('default_logo');
    var doodle = $('doodle');
    if (!data.ddljson || !data.ddljson.transparent_large_image) {
      defaultLogo.style.display = 'block';
      doodle.style.display = 'none';
      return;
    }

    doodle.onload = function() {
      defaultLogo.style.display = 'none';
      doodle.style.display = 'block';
    };
    doodle.src = base_url + data.ddljson.transparent_large_image.url;
  }

  /**
   * Invoked when the app-list bubble is hidden.
   */
  function onAppListHidden() {
    speechManager.onHidden();
  }

  /**
   * Invoked when the user explicitly wants to toggle the speech recognition
   * state.
   */
  function toggleSpeechRecognition() {
    speechManager.toggleSpeechRecognition();
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

document.addEventListener('DOMContentLoaded', appList.startPage.initialize);
