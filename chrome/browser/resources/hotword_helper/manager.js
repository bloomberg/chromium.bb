// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * @fileoverview This extension manages communications between Chrome,
 * Google.com pages and the Chrome Hotword extension.
 *
 * This helper extension is required due to the depoyment plan for Chrome M34:
 *
 *  - The hotword extension will be distributed as an externally loaded
 *      component extension.
 *  - Settings for enabling and disabling the hotword extension has moved to
 *      Chrome settings.
 *  - Newtab page is served via chrome://newtab/
 *
 */



/** @constructor */
var OptInManager = function() {};


/**
 * @const {string}
 * @private
 */
OptInManager.HOTWORD_EXTENSION_ID_ = 'bepbmhgboaologfdajaanbcjmnhjmhfn';


/**
 * Commands sent from this helper extension to the hotword extension.
 * @enum {string}
 */
OptInManager.CommandFromHelper = {
  DISABLE: 'ds',
  ENABLE: 'en'
};


/**
 * Commands sent from the page to this content script.
 * @enum {string}
 */
OptInManager.CommandFromPage = {
  // User has explicitly clicked 'no'.
  CLICKED_NO_OPTIN: 'hcno',
  // User has opted in.
  CLICKED_OPTIN: 'hco'
};


/**
 * Handles a tab being activated / focused on.
 * @param {{tabId: number}} info Information about the activated tab.
 * @private
 */
OptInManager.prototype.handleActivatedTab_ = function(info) {
  chrome.tabs.get(info.tabId, this.preInjectTab_.bind(this));
};


/**
 * Handles an updated tab.
 * @param {number} tabId Id of the updated tab.
 * @param {{status: string}} info Change info of the tab.
 * @param {!Tab} tab Updated tab.
 * @private
 */
OptInManager.prototype.handleUpdatedTab_ = function(tabId, info, tab) {
  //  Chrome fires multiple update events: undefined, loading and completed.
  // We perform content injection on loading state.
  if ('loading' !== info['status'])
    return;
  this.preInjectTab_(tab);
};


/**
 * @param {Tab} tab Tab to consider.
 * @private
 */
OptInManager.prototype.preInjectTab_ = function(tab) {
  if (!tab || !this.isEligibleUrl(tab.url))
    return;
  if (chrome.hotwordPrivate && chrome.hotwordPrivate.getStatus)
    chrome.hotwordPrivate.getStatus(this.injectTab_.bind(this, tab));
};


/**
 * @param {Tab} tab Tab to inject.
 * @param {HotwordStatus} hotwordStatus Status of the hotword extension.
 * @private
 */
OptInManager.prototype.injectTab_ = function(tab, hotwordStatus) {
  if (hotwordStatus.available) {
    if (hotwordStatus.enabled)
      chrome.tabs.executeScript(tab.id, {'file': 'audio_client.js'});
    else if (!hotwordStatus.enabledSet)
      chrome.tabs.executeScript(tab.id, {'file': 'optin_client.js'});
  }
};


/**
 * Handles changes in the enabled state of the hotword feature in the
 * Chrome settings page.
 * @private
 */
OptInManager.prototype.handleEnabledChange_ = function() {
  if (chrome.hotwordPrivate && chrome.hotwordPrivate.getStatus)
    chrome.hotwordPrivate.getStatus(this.updateEnabled_.bind(this));
};


/**
 * Sends a message to the hotword extension to update it about Chrome settings.
 * @param {HotwordStatus} hotwordStatus Status of the hotword extension.
 * @private
 */
OptInManager.prototype.updateEnabled_ = function(hotwordStatus) {
  if (hotwordStatus.enabled) {
    chrome.runtime.sendMessage(
        OptInManager.HOTWORD_EXTENSION_ID_,
        {'cmd': OptInManager.CommandFromHelper.ENABLE});
  } else {
    chrome.runtime.sendMessage(
        OptInManager.HOTWORD_EXTENSION_ID_,
        {'cmd': OptInManager.CommandFromHelper.DISABLE});
  }
};


/**
 * Handles messages from the helper content script.
 * @param {*} request Message from the sender.
 * @param {MessageSender} sender Information about the sender.
 * @param {function(*)} sendResponse Callback function to respond to sender.
 * @private
 */
OptInManager.prototype.handleMessage_ = function(
    request, sender, sendResponse) {
  if (request.type) {
    if (request.type === OptInManager.CommandFromPage.CLICKED_OPTIN) {
      if (chrome.hotwordPrivate && chrome.hotwordPrivate.setEnabled) {
        chrome.hotwordPrivate.setEnabled(true);
        this.preInjectTab_(sender.tab);
      }
    }
    // User has explicitly clicked 'no thanks'.
    if (request.type === OptInManager.CommandFromPage.CLICKED_NO_OPTIN) {
      if (chrome.hotwordPrivate && chrome.hotwordPrivate.setEnabled) {
        chrome.hotwordPrivate.setEnabled(false);
      }
    }
  }
};


/**
 * Determines if a URL is eligible for hotwording. For now, the
 * valid pages are the Google HP and SERP (this will include the NTP).
 * @param {string} url Url to check.
 * @return {boolean} True if url is eligible hotword url.
 */
OptInManager.prototype.isEligibleUrl = function(url) {
  if (!url) {
    return false;
  }

  var baseUrls = [
    'https://www.google.com',
    'chrome://newtab',
    'https://encrypted.google.com'
  ];

  for (var i = 0; i < baseUrls.length; i++) {
    var base = baseUrls[i];
    if (url === base + '/' ||
        url.indexOf(base + '/_/chrome/newtab?') === 0 ||  // Appcache NTP.
        url.indexOf(base + '/?') === 0 ||
        url.indexOf(base + '/#') === 0 ||
        url.indexOf(base + '/webhp') === 0 ||
        url.indexOf(base + '/search') === 0) {
      return true;
    }
  }
  return false;
};


/**
 * Initializes the extension.
 */
OptInManager.prototype.initialize = function() {
  chrome.tabs.onActivated.addListener(this.handleActivatedTab_.bind(this));
  chrome.tabs.onUpdated.addListener(this.handleUpdatedTab_.bind(this));
  chrome.runtime.onMessage.addListener(this.handleMessage_.bind(this));

  if (chrome.hotwordPrivate && chrome.hotwordPrivate.onEnabledChanged) {
    chrome.hotwordPrivate.onEnabledChanged.addListener(
        this.handleEnabledChange_.bind(this));
  }
};


new OptInManager().initialize();
