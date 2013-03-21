// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Test API for Chrome OS Image Editor.
 *
 * There are two ways to load Image Editor before testing:
 *   - open File Manager, and then click on an image file to open Image Editor;
 *   or
 *   - open tab with URL:
 *     chrome-extension://hhaomjibdihmijegdhdafkllkbggdgoj/gallery.html
 *     and then call |galleryTestAPI.load('/Downloads/path/to/file.jpg')|.
 *
 *  After Image Editor is loaded, you can call methods from |galleryTestAPI| to
 *  emulate user actions and get feedback.
 */
var galleryTestAPI = {
  /**
   * Open the Photo Editor.
   * @param {string} path Path to the directory or an image file.
   */
  load: function(path) {
    Gallery.openStandalone(path, null, function() {
      galleryTestAPI.waitFor_('loaded');
    });
  },

  /**
   * Responds with the selected file name.
   */
  getSelectedFileName: function() {
    galleryTestAPI.respond_(document.querySelector('.namebox').value);
  },

  /**
   * Toggles edit mode.
   */
  clickEditToggle: function() {
    galleryTestAPI.click('.edit');
    setTimeout(galleryTestAPI.respond_.bind(null, true), 0);
  },

  /**
   * Clicks arrow to select next image.
   */
  clickNextImageArrow: function() {
    galleryTestAPI.click('.arrow.right');
    galleryTestAPI.waitFor_('image-displayed');
  },

  /**
   * Clicks arrow to select previous image.
   */
  clickPreviousImageArrow: function() {
    galleryTestAPI.click('.arrow.left');
    galleryTestAPI.waitFor_('image-displayed');
  },

  /**
   * Clicks last thumbnail in ribbon to select an image.
   */
  clickLastRibbonThumbnail: function() {
    galleryTestAPI.clickRibbonThumbnail(true);
  },

  /**
   * Clicks first thumbnail in ribbon to select an image.
   */
  clickFirstRibbonThumbnail: function() {
    galleryTestAPI.clickRibbonThumbnail(false);
  },

  /**
   * Clicks thumbnail in ribbon.
   * @param {boolean} last Whether to click on last vs first.
   */
  clickRibbonThumbnail: function(last) {
    // TODO(dgozman): investigate why this timeout is required sometimes.
    setTimeout(function() {
      var nodes = document.querySelectorAll('.ribbon > :not([vanishing])');
      if (nodes.length == 0) {
        galleryTestAPI.respond_(false);
        return;
      }
      nodes[last ? nodes.length - 1 : 0].click();
      galleryTestAPI.waitFor_('image-displayed');
    }, 0);
  },

  /**
   * Clicks 'rotate left' tool.
   */
  clickRotateLeft: function() {
    galleryTestAPI.editAndRespond_(
        galleryTestAPI.click.bind(null, '.rotate_left'));
  },

  /**
   * Clicks 'rotate right' tool.
   */
  clickRotateRight: function() {
    galleryTestAPI.editAndRespond_(
        galleryTestAPI.click.bind(null, '.rotate_right'));
  },

  /**
   * Clicks 'undo' tool.
   */
  clickUndo: function() {
    galleryTestAPI.editAndRespond_(galleryTestAPI.click.bind(null, '.undo'));
  },

  /**
   * Clicks 'redo' tool.
   */
  clickRedo: function() {
    galleryTestAPI.editAndRespond_(galleryTestAPI.click.bind(null, '.redo'));
  },

  /**
   * Clicks 'autofix' tool.
   */
  clickAutofix: function() {
    galleryTestAPI.editAndRespond_(galleryTestAPI.click.bind(null, '.autofix'));
  },

  /**
   * Responds whether autofix tool is available.
   */
  isAutofixAvailable: function() {
    galleryTestAPI.respond_(
        !document.querySelector('.autofix').hasAttribute('disabled'));
  },

  /**
   * Performs a click on the element with specififc selector.
   * @param {string} selector CSS selector.
   */
  click: function(selector) {
    document.querySelector(selector).click();
  },

  /**
   * Waits until editor is ready, performs action and then responds.
   * @param {function} action The action to perform.
   * @private
   */
  editAndRespond_: function(action) {
    // TODO(dgozman): investigate why this is required sometimes.
    setTimeout(function() {
      action();
      galleryTestAPI.waitFor_('image-saved');
    }, 0);
  },

  /**
   * Waits for event fired and then calls a function.
   * @param {string} event Event name.
   * @param {function=} opt_callback Callback. If not passed,
   *     |galleryTestAPI.respond_(true)| is called.
   * @private
   */
  waitFor_: function(event, opt_callback) {
    var callback = opt_callback || galleryTestAPI.respond_.bind(null, true);
    var listener = function() {
      Gallery.instance.removeEventListener(event, listener);
      callback();
    };
    Gallery.instance.addEventListener(event, listener);
  },

  /**
   * @param {string|boolean|number} value Value to send back.
   * @private
   */
  respond_: function(value) {
    if (window.domAutomationController) {
      window.domAutomationController.send(value);
    } else if (galleryTestAPI.respondCallback) {
      galleryTestAPI.respondCallback(value);
    } else {
      console.log('playerTestAPI response: ' + value);
    }
  }
};

/**
 * Test example.
 */
galleryTestAPI.testExample = function() {
  var api = galleryTestAPI;
  var PATH = '/Downloads/file.jpg';

  var steps = [
    function load() {
      if (!Gallery.instance) {
        api.load(PATH);
      } else {
        nextStep();
      }
    },

    function getName() { api.getSelectedFileName(); },

    function lastRibbon(filename) {
      console.log(filename);
      api.clickLastRibbonThumbnail();
    },

    function toggleEdit() { api.clickEditToggle(); },

    function rightArrow() { api.clickNextImageArrow(); },

    function rotateLeft() { api.clickRotateLeft(); },

    function rotateRight() { api.clickRotateRight(); },

    function undo() { api.clickUndo(); },

    function queryAutofix() { api.isAutofixAvailable(); },

    function autofix(available) {
      if (available) {
        api.clickAutofix();
      } else {
        nextStep();
      }
    },

    function done() { console.log('done'); }
  ];

  var step = 0;

  function nextStep() {
    ++step;
    console.log('nextStep calls: ' + steps[step - 1].name);
    steps[step - 1].apply(null, arguments);
  }

  api.respondCallback = nextStep;
  nextStep();
};
