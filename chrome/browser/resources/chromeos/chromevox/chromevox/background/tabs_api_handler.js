// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Accesses Chrome's tabs extension API and gives
 * feedback for events that happen in the "Chrome of Chrome".
 */

goog.provide('cvox.TabsApiHandler');

goog.require('cvox.AbstractEarcons');
goog.require('cvox.AbstractTts');
goog.require('cvox.BrailleInterface');
goog.require('cvox.ChromeVox');
goog.require('cvox.NavBraille');


/**
 * Class that adds listeners and handles events from the tabs API.
 * @constructor
 */
cvox.TabsApiHandler = function() {
  /** @type {function(string, Array<string>=)} @private */
  this.msg_ = Msgs.getMsg.bind(Msgs);
  /**
   * Tracks whether the active tab has finished loading.
   * @type {boolean}
   * @private
   */
  this.lastActiveTabLoaded_ = false;

  chrome.tabs.onCreated.addListener(this.onCreated.bind(this));
  chrome.tabs.onRemoved.addListener(this.onRemoved.bind(this));
  chrome.tabs.onActivated.addListener(this.onActivated.bind(this));
  chrome.tabs.onUpdated.addListener(this.onUpdated.bind(this));
  chrome.windows.onFocusChanged.addListener(this.onFocusChanged.bind(this));
};

cvox.TabsApiHandler.prototype = {
  /**
   * Handles chrome.tabs.onCreated.
   * @param {Object} tab
   */
  onCreated: function(tab) {
    if (!cvox.ChromeVox.isActive) {
      return;
    }
    cvox.ChromeVox.tts.speak(this.msg_('chrome_tab_created'),
                             cvox.QueueMode.FLUSH,
                             cvox.AbstractTts.PERSONALITY_ANNOUNCEMENT);
    cvox.ChromeVox.braille.write(
        cvox.NavBraille.fromText(this.msg_('chrome_tab_created')));
    cvox.ChromeVox.earcons.playEarcon(cvox.Earcon.OBJECT_OPEN);
  },

  /**
   * Handles chrome.tabs.onRemoved.
   * @param {Object} tab
   */
  onRemoved: function(tab) {
    if (!cvox.ChromeVox.isActive) {
      return;
    }
    cvox.ChromeVox.earcons.playEarcon(cvox.Earcon.OBJECT_CLOSE);
  },

  /**
   * Handles chrome.tabs.onActivated.
   * @param {Object} activeInfo
   */
  onActivated: function(activeInfo) {
    if (!cvox.ChromeVox.isActive) {
      return;
    }
    chrome.tabs.get(activeInfo.tabId, function(tab) {
      this.lastActiveTabLoaded_ = tab.status == 'complete';
      if (tab.status == 'loading') {
        return;
      }
      var title = tab.title ? tab.title : tab.url;
      cvox.ChromeVox.tts.speak(this.msg_('chrome_tab_selected',
                                         [title]),
                               cvox.QueueMode.FLUSH,
                               cvox.AbstractTts.PERSONALITY_ANNOUNCEMENT);
      cvox.ChromeVox.braille.write(
          cvox.NavBraille.fromText(this.msg_('chrome_tab_selected', [title])));
      cvox.ChromeVox.earcons.playEarcon(cvox.Earcon.OBJECT_SELECT);
    }.bind(this));
  },

  /**
   * Handles chrome.tabs.onUpdated.
   * @param {number} tabId
   * @param {Object} selectInfo
   */
  onUpdated: function(tabId, selectInfo) {
    if (!cvox.ChromeVox.isActive) {
      return;
    }
    chrome.tabs.get(tabId, function(tab) {
      if (!tab.active) {
        return;
      }
      if (tab.status == 'loading') {
        this.lastActiveTabLoaded_ = false;
        cvox.ChromeVox.earcons.playEarcon(cvox.Earcon.PAGE_START_LOADING);
      } else if (!this.lastActiveTabLoaded_) {
        this.lastActiveTabLoaded_ = true;
        cvox.ChromeVox.earcons.playEarcon(cvox.Earcon.PAGE_FINISH_LOADING);
      }
    }.bind(this));
  },

  /**
   * Handles chrome.windows.onFocusChanged.
   * @param {number} windowId
   */
  onFocusChanged: function(windowId) {
    if (!cvox.ChromeVox.isActive) {
      return;
    }
    if (windowId == chrome.windows.WINDOW_ID_NONE) {
      return;
    }
    chrome.windows.get(windowId, function(window) {
      chrome.tabs.query({active: true, windowId: windowId}, function(tabs) {
        var msgId = window.incognito ? 'chrome_incognito_window_selected' :
            'chrome_normal_window_selected';
        var tab = tabs[0] || {};
        var title = tab.title ? tab.title : tab.url;
        cvox.ChromeVox.tts.speak(this.msg_(msgId, [title]),
                                 cvox.QueueMode.FLUSH,
                                 cvox.AbstractTts.PERSONALITY_ANNOUNCEMENT);
        cvox.ChromeVox.braille.write(
            cvox.NavBraille.fromText(this.msg_(msgId, [title])));
        cvox.ChromeVox.earcons.playEarcon(cvox.Earcon.OBJECT_SELECT);
      }.bind(this));
    }.bind(this));
  }
};
