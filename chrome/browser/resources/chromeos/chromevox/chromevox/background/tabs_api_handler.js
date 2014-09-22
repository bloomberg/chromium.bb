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
goog.require('cvox.BrailleUtil');
goog.require('cvox.NavBraille');


/**
 * Class that adds listeners and handles events from the tabs API.
 * @constructor
 * @param {cvox.TtsInterface} tts The TTS to use for speaking.
 * @param {cvox.BrailleInterface} braille The braille interface to use for
 * brailling.
 * @param {cvox.AbstractEarcons} earcons The earcons object to use for playing
 *        earcons.
 */
cvox.TabsApiHandler = function(tts, braille, earcons) {
  /** @type {cvox.TtsInterface} @private */
  this.tts_ = tts;
  /** @type {cvox.BrailleInterface} @private */
  this.braille_ = braille;
  /** @type {cvox.AbstractEarcons} @private */
  this.earcons_ = earcons;
  /** @type {function(string)} @private */
  this.msg_ = cvox.ChromeVox.msgs.getMsg.bind(cvox.ChromeVox.msgs);

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
    this.tts_.speak(this.msg_('chrome_tab_created'),
                   cvox.AbstractTts.QUEUE_MODE_FLUSH,
                   cvox.AbstractTts.PERSONALITY_ANNOUNCEMENT);
    this.braille_.write(
        cvox.NavBraille.fromText(this.msg_('chrome_tab_created')));
    this.earcons_.playEarcon(cvox.AbstractEarcons.OBJECT_OPEN);
  },

  /**
   * Handles chrome.tabs.onRemoved.
   * @param {Object} tab
   */
  onRemoved: function(tab) {
    if (!cvox.ChromeVox.isActive) {
      return;
    }
    this.earcons_.playEarcon(cvox.AbstractEarcons.OBJECT_CLOSE);
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
      if (tab.status == 'loading') {
        return;
      }
      var title = tab.title ? tab.title : tab.url;
      this.tts_.speak(this.msg_('chrome_tab_selected',
                         [title]),
                     cvox.AbstractTts.QUEUE_MODE_FLUSH,
                     cvox.AbstractTts.PERSONALITY_ANNOUNCEMENT);
      this.braille_.write(
          cvox.NavBraille.fromText(this.msg_('chrome_tab_selected', [title])));
      this.earcons_.playEarcon(cvox.AbstractEarcons.OBJECT_SELECT);
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
        this.earcons_.playEarcon(cvox.AbstractEarcons.BUSY_PROGRESS_LOOP);
      } else {
        this.earcons_.playEarcon(cvox.AbstractEarcons.TASK_SUCCESS);
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
      chrome.tabs.getSelected(windowId, function(tab) {
        var msgId = window.incognito ? 'chrome_incognito_window_selected' :
          'chrome_normal_window_selected';
        var title = tab.title ? tab.title : tab.url;
        this.tts_.speak(this.msg_(msgId, [title]),
                       cvox.AbstractTts.QUEUE_MODE_FLUSH,
                       cvox.AbstractTts.PERSONALITY_ANNOUNCEMENT);
        this.braille_.write(
            cvox.NavBraille.fromText(this.msg_(msgId, [title])));
        this.earcons_.playEarcon(cvox.AbstractEarcons.OBJECT_SELECT);
      }.bind(this));
    }.bind(this));
  }
};
