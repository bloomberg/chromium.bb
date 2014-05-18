// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The entry point for all ChromeVox2 related code for the
 * background page.
 */

/** ChromeVox2 namespace */
var cvox2 = function() {};

/** Namespace for global objects in the background page. */
cvox2.global = function() {};

/** Classic Chrome accessibility API. */
cvox2.global.accessibility =
    chrome.accessibilityPrivate || chrome.experimental.accessibility;

/**
 * ChromeVox2 background page.
 */
cvox2.Background = function() {
  // Only needed with unmerged ChromeVox classic loaded before.
  cvox2.global.accessibility.setAccessibilityEnabled(false);
  chrome.automation.getDesktop(this.onGotDesktop.bind(this));
};

cvox2.Background.prototype = {
  /**
   * ID of the port used to communicate between content script and background
   * page.
   * @const {string}
   */
  PORT_ID: 'chromevox2',

  /**
   * Waits until a desktop automation tree becomes available.
   * Thereafter, registers a simple exploration mode for the desktop tree.
   * @param {AutomationTree} tree The desktop automation tree.
   */
  onGotDesktop: function(tree) {
    if (!tree.root) {
      window.setTimeout(this.onGotDesktop, 500);
      return;
    }
    chrome.extension.onConnect.addListener(function(port) {
      if (port.name != this.PORT_ID)
        return;
      var cur = tree.root;
      port.onMessage.addListener(function(message) {
        switch (message.keydown) {
          case 37:
            cur = cur.previousSibling() || cur;
            break;
          case 38:
            cur = cur.parent() || cur;
            break;
          case 39:
            cur = cur.nextSibling() || cur;
            break;
          case 40:
            cur = cur.firstChild() || cur;
            break;
        }
        var index = 1;
        if (cur.parent())
          index = cur.parent().children().indexOf(cur) + 1;
        var name = '';
        if (cur.attributes && cur.attributes['ax_attr_name'])
          name = cur.attributes['ax_attr_name'];
        var utterance = index + ' ' + name + cur.role;
        chrome.tts.speak(String(utterance), {lang: 'en-US'});
      });
    }.bind(this));

    // Register all automation event listeners.
    tree.root.addEventListener('focus', this.onDesktopEvent.bind(this), true);
  },

  /**
   * A generic handler for all desktop automation events.
   * @param {AutomationEvent} evt The event.
   */
  onDesktopEvent: function(evt) {
    var output = evt.target.attributes.name + ' ' + evt.target.role;
    cvox.ChromeVox.tts.speak(output);
    cvox.ChromeVox.braille.write(cvox.NavBraille.fromText(output));
  }
};

/** @type {cvox2.Background} */
cvox2.global.backgroundObj = new cvox2.Background();
