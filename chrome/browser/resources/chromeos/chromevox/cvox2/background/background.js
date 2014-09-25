// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The entry point for all ChromeVox2 related code for the
 * background page.
 */

goog.provide('cvox2.Background');
goog.provide('cvox2.global');

goog.require('cvox.TabsApiHandler');

/** Classic Chrome accessibility API. */
cvox2.global.accessibility =
    chrome.accessibilityPrivate || chrome.experimental.accessibility;

/**
 * ChromeVox2 background page.
 */
cvox2.Background = function() {
  /**
   * A list of site substring patterns to use with ChromeVox next. Keep these
   * strings relatively specific.
   * @type {!Array.<string>}
   */
  this.whitelist_ = ['http://www.chromevox.com/', 'chromevox_next_test'];

  /** @type {cvox.TabsApiHandler} @private */
  this.tabsHandler_ = new cvox.TabsApiHandler(cvox.ChromeVox.tts,
                                              cvox.ChromeVox.braille,
                                              cvox.ChromeVox.earcons);

  // Only needed with unmerged ChromeVox classic loaded before.
  cvox2.global.accessibility.setAccessibilityEnabled(false);

  // Manually bind all functions to |this|.
  for (var func in this) {
    if (typeof(this[func]) == 'function')
      this[func] = this[func].bind(this);
  }

  // Register listeners for ...
  // Desktop.
  chrome.automation.getDesktop(this.onGotTree);

  // Tabs.
  chrome.tabs.onUpdated.addListener(this.onTabUpdated);
};

cvox2.Background.prototype = {
  /**
   * Handles chrome.tabs.onUpdated.
   * @param {number} tabId
   * @param {Object} changeInfo
   */
  onTabUpdated: function(tabId, changeInfo) {
    chrome.tabs.get(tabId, function(tab) {
      if (!tab.url)
        return;

      if (!this.isWhitelisted_(tab.url)) {
        chrome.commands.onCommand.removeListener(this.onGotCommand);
        cvox.ChromeVox.background.injectChromeVoxIntoTabs([tab], true);
        return;
      }

      if (!chrome.commands.onCommand.hasListeners()) {
        chrome.commands.onCommand.addListener(this.onGotCommand);
      }

      this.disableClassicChromeVox_(tab.id);

      chrome.automation.getTree(this.onGotTree.bind(this));
    }.bind(this));
  },

  /**
   * Handles all setup once a new automation tree appears.
   * @param {AutomationTree} tree The new automation tree.
   */
  onGotTree: function(root) {
    // Register all automation event listeners.
    root.addEventListener(chrome.automation.EventType.focus,
                          this.onAutomationEvent.bind(this),
                          true);
  },

  /**
   * A generic handler for all desktop automation events.
   * @param {AutomationEvent} evt The event.
   */
  onAutomationEvent: function(evt) {
    var output = evt.target.attributes.name + ' ' + evt.target.role;
    cvox.ChromeVox.tts.speak(output, cvox.AbstractTts.QUEUE_MODE_FLUSH);
    cvox.ChromeVox.braille.write(cvox.NavBraille.fromText(output));
    chrome.accessibilityPrivate.setFocusRing([evt.target.location]);
  },

  /**
   * Handles chrome.commands.onCommand.
   * @param {string} command
   */
  onGotCommand: function(command) {
  },

  /**
   * @private
   * @param {string} url
   * @return {boolean} Whether the given |url| is whitelisted.
   */
  isWhitelisted_: function(url) {
    return this.whitelist_.some(function(item) {
      return url.indexOf(item) != -1;
    }.bind(this));
  },

  /**
   * Disables classic ChromeVox.
   * @param {number} tabId The tab where ChromeVox classic is running.
   */
  disableClassicChromeVox_: function(tabId) {
    chrome.tabs.executeScript(
          tabId,
          {'code': 'try { window.disableChromeVox(); } catch(e) { }\n',
           'allFrames': true});
  }
};

/** @type {cvox2.Background} */
cvox2.global.backgroundObj = new cvox2.Background();
