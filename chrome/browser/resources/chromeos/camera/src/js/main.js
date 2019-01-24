// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Namespace for the Camera app.
 */
var cca = cca || {};

/**
 * Creates the Camera App main object.
 * @constructor
 */
cca.App = function() {
  /**
   * @type {cca.models.Gallery}
   * @private
   */
  this.model_ = new cca.models.Gallery();

  /**
   * @type {cca.views.Camera}
   * @private
   */
  this.cameraView_ = new cca.views.Camera(this.model_);

  /**
   * @type {cca.views.MasterSettings}
   * @private
   */
  this.settingsView_ = new cca.views.MasterSettings();

  /**
   * @type {cca.views.GridSettings}
   * @private
   */
  this.gridsettingsView_ = new cca.views.GridSettings();

  /**
   * @type {cca.views.TimerSettings}
   * @private
   */
  this.timersettingsView_ = new cca.views.TimerSettings();

  /**
   * @type {cca.views.Browser}
   * @private
   */
  this.browserView_ = new cca.views.Browser(this.model_);

  /**
   * @type {cca.views.Warning}
   * @private
   */
  this.warningView_ = new cca.views.Warning();

  /**
   * @type {cca.views.Dialog}
   * @private
   */
  this.dialogView_ = new cca.views.Dialog();

  // End of properties. Seal the object.
  Object.seal(this);

  document.body.addEventListener('keydown', this.onKeyPressed_.bind(this));

  document.title = chrome.i18n.getMessage('name');
  this.setupI18nElements_();
  this.setupToggles_();
};

/*
 * Checks if it is applicable to use CrOS gallery app.
 * @return {boolean} Whether applicable or not.
 */
cca.App.useGalleryApp = function() {
  return chrome.fileManagerPrivate &&
      document.body.classList.contains('ext-fs');
};

/**
 * Sets up i18n messages on elements by i18n attributes.
 * @private
 */
cca.App.prototype.setupI18nElements_ = function() {
  var getElements = (attr) => document.querySelectorAll('[' + attr + ']');
  var getMessage = (element, attr) => chrome.i18n.getMessage(
      element.getAttribute(attr));
  var setAriaLabel = (element, attr) => element.setAttribute(
      'aria-label', getMessage(element, attr));

  getElements('i18n-content').forEach(
      (element) => element.textContent = getMessage(element, 'i18n-content'));
  getElements('i18n-aria').forEach(
      (element) => setAriaLabel(element, 'i18n-aria'));
  cca.tooltip.setup(getElements('i18n-label')).forEach(
      (element) => setAriaLabel(element, 'i18n-label'));
};

/**
 * Sets up toggles (checkbox and radio) by data attributes.
 * @private
 */
cca.App.prototype.setupToggles_ = function() {
  document.querySelectorAll('input').forEach((element) => {
    element.addEventListener('keypress', (event) =>
        cca.util.getShortcutIdentifier(event) == 'Enter' && element.click());

    var css = element.getAttribute('data-css');
    var key = element.getAttribute('data-key');
    var payload = () => {
      var keys = {};
      keys[key] = element.checked;
      return keys;
    };
    element.addEventListener('change', (event) => {
      if (css) {
        document.body.classList.toggle(css, element.checked);
      }
      if (event.isTrusted) {
        element.save();
        if (element.type == 'radio' && element.checked) {
          // Handle unchecked grouped sibling radios.
          var grouped = `input[type=radio][name=${element.name}]:not(:checked)`;
          document.querySelectorAll(grouped).forEach((radio) =>
              radio.dispatchEvent(new Event('change')) && radio.save());
        }
      }
    });
    element.toggleChecked = (checked) => {
      element.checked = checked;
      element.dispatchEvent(new Event('change')); // Trigger toggling css.
    };
    element.save = () => {
      return key && chrome.storage.local.set(payload());
    };
    if (key) {
      // Restore the previously saved state on startup.
      chrome.storage.local.get(payload(),
          (values) => element.toggleChecked(values[key]));
    }
  });
};

/**
 * Starts the app by preparing views/model and opening the camera-view.
 */
cca.App.prototype.start = function() {
  cca.nav.setup([
    this.cameraView_,
    this.settingsView_,
    this.gridsettingsView_,
    this.timersettingsView_,
    this.browserView_,
    this.warningView_,
    this.dialogView_,
  ]);
  cca.models.FileSystem.initialize(() => {
    // Prompt to migrate pictures if needed.
    var message = chrome.i18n.getMessage('migratePicturesMsg');
    return cca.nav.open('dialog', message, false).then((acked) => {
      if (!acked) {
        throw new Error('no-migrate');
      }
    });
  }).then((external) => {
    document.body.classList.toggle('ext-fs', external);
    // Prepare the views/model and open camera-view.
    this.cameraView_.prepare();
    this.model_.addObserver(this.cameraView_.galleryButton);
    if (!cca.App.useGalleryApp()) {
      this.model_.addObserver(this.browserView_);
    }
    this.model_.load();
    cca.nav.open('camera');
  }).catch((error) => {
    console.error(error);
    if (error && error.message == 'no-migrate') {
      chrome.app.window.current().close();
      return;
    }
    cca.nav.open('warning', 'filesystem-failure');
  });
};

/**
 * Handles pressed keys.
 * @param {Event} event Key press event.
 * @private
 */
cca.App.prototype.onKeyPressed_ = function(event) {
  cca.tooltip.hide(); // Hide shown tooltip on any keypress.
  cca.nav.onKeyPressed(event);
};

/**
 * @type {cca.App} Singleton of the App object.
 * @private
 */
cca.App.instance_ = null;

/**
 * Creates the Camera object and starts screen capturing.
 */
document.addEventListener('DOMContentLoaded', () => {
  if (!cca.App.instance_) {
    cca.App.instance_ = new cca.App();
  }
  cca.App.instance_.start();
  chrome.app.window.current().show();
});
