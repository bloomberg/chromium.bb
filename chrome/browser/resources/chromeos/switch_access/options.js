// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Alias for document.getElementById.
 *
 * @param  {string} id
 * @return {Element}
 */
let $ = function(id) {
  return document.getElementById(id);
};

/**
 * Class to manage the options page.
 *
 * @constructor
 */
let SwitchAccessOptions = function() {
  let background = chrome.extension.getBackgroundPage();

  /**
   * User preferences.
   *
   * @type {SwitchAccessPrefs}
   */
  this.switchAccessPrefs_ = background.switchAccess.switchAccessPrefs;

  this.init_();
  document.addEventListener('change', this.handleInputChange_.bind(this));
  background.document.addEventListener(
      'prefsUpdate', this.handlePrefsUpdate_.bind(this));
};

SwitchAccessOptions.prototype = {
  /**
   * Initialize the options page by setting all elements representing a user
   * preference to show the correct value.
   *
   * @private
   */
  init_: function() {
    let prefs = this.switchAccessPrefs_.getPrefs();
    $('enableAutoScan').checked = prefs['enableAutoScan'];
    $('autoScanTime').value = prefs['autoScanTime'];
  },

  /**
   * Handle a change by the user to an element representing a user preference.
   *
   * @param {!Event} event
   * @private
   */
  handleInputChange_: function(event) {
    switch (event.target.id) {
      case 'enableAutoScan':
        this.switchAccessPrefs_.setPref(event.target.id, event.target.checked);
        break;
      case 'autoScanTime':
        this.switchAccessPrefs_.setPref(event.target.id, event.target.value);
        break;
    }
  },

  /**
   * Handle a change in user preferences.
   *
   * @param {!Event} event
   * @private
   */
  handlePrefsUpdate_: function(event) {
    let updatedPrefs = event.detail;
    for (let key of Object.keys(updatedPrefs)) {
      switch (key) {
        case 'enableAutoScan':
          $(key).checked = updatedPrefs[key];
          break;
        case 'autoScanTime':
          $(key).value = updatedPrefs[key];
          break;
      }
    }
  }
};

document.addEventListener('DOMContentLoaded', function() {
  new SwitchAccessOptions();
});
