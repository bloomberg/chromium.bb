// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview ChromeVox panel.
 *
 */

goog.provide('Panel');

goog.require('Msgs');
goog.require('PanelCommand');

function $(id) {
  return document.getElementById(id);
}

/**
 * Class to manage the panel.
 * @constructor
 */
Panel = function() {
};

/**
 * Initialize the panel.
 */
Panel.init = function() {
  /** @type {Element} @private */
  this.speechContainer_ = $('speech-container');

  /** @type {Element} @private */
  this.speechElement_ = $('speech');

  /** @type {Element} @private */
  this.brailleContainer_ = $('braille-container');

  /** @type {Element} @private */
  this.brailleTextElement_ = $('braille-text');

  /** @type {Element} @private */
  this.brailleCellsElement_ = $('braille-cells');

  Panel.updateFromPrefs();
  window.addEventListener('storage', function(event) {
    if (event.key == 'brailleCaptions') {
      Panel.updateFromPrefs();
    }
  }, false);

  window.addEventListener('message', function(message) {
    var command = JSON.parse(message.data);
    Panel.exec(/** @type {PanelCommand} */(command));
  }, false);

  $('options').addEventListener('click', Panel.onOptions, false);
  $('close').addEventListener('click', Panel.onClose, false);

  // The ChromeVox menu isn't fully implemented yet, disable it.
  $('menu').disabled = true;
  $('triangle').style.display = 'none';

  Msgs.addTranslatedMessagesToDom(document);
};

/**
 * Update the display based on prefs.
 */
Panel.updateFromPrefs = function() {
  if (localStorage['brailleCaptions'] === String(true)) {
    this.speechContainer_.style.visibility = 'hidden';
    this.brailleContainer_.style.visibility = 'visible';
  } else {
    this.speechContainer_.style.visibility = 'visible';
    this.brailleContainer_.style.visibility = 'hidden';
  }
};

/**
 * Execute a command to update the panel.
 *
 * @param {PanelCommand} command The command to execute.
 */
Panel.exec = function(command) {
  /**
   * Escape text so it can be safely added to HTML.
   * @param {*} str Text to be added to HTML, will be cast to string.
   * @return {string} The escaped string.
   */
  function escapeForHtml(str) {
    return String(str)
        .replace(/&/g, '&amp;')
        .replace(/</g, '&lt;')
        .replace(/\>/g, '&gt;')
        .replace(/"/g, '&quot;')
        .replace(/'/g, '&#039;')
        .replace(/\//g, '&#x2F;');
  }

  switch (command.type) {
    case PanelCommandType.CLEAR_SPEECH:
      this.speechElement_.innerHTML = '';
      break;
    case PanelCommandType.ADD_NORMAL_SPEECH:
      if (this.speechElement_.innerHTML != '') {
        this.speechElement_.innerHTML += '&nbsp;&nbsp;';
      }
      this.speechElement_.innerHTML += '<span class="usertext">' +
                                       escapeForHtml(command.data) +
                                       '</span>';
      break;
    case PanelCommandType.ADD_ANNOTATION_SPEECH:
      if (this.speechElement_.innerHTML != '') {
        this.speechElement_.innerHTML += '&nbsp;&nbsp;';
      }
      this.speechElement_.innerHTML += escapeForHtml(command.data);
      break;
    case PanelCommandType.UPDATE_BRAILLE:
      this.brailleTextElement_.textContent = command.data.text;
      this.brailleCellsElement_.textContent = command.data.braille;
      break;
  }
};

/**
 * Open the ChromeVox Options.
 */
Panel.onOptions = function() {
  var bkgnd =
      chrome.extension.getBackgroundPage()['global']['backgroundObj'];
  bkgnd['showOptionsPage']();
  window.location = '#';
};

/**
 * Exit ChromeVox.
 */
Panel.onClose = function() {
  window.location = '#close';
};

window.addEventListener('load', function() {
  Panel.init();
}, false);
