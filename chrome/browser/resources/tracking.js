// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var g_browserBridge;
var g_mainView;

/**
 * Main entry point called once the page has loaded.
 */
function onLoad() {
  g_browserBridge = new BrowserBridge();
  g_mainView = new MainView();

  // Ask the browser to send us the current data.
  g_browserBridge.sendGetData();
}

document.addEventListener('DOMContentLoaded', onLoad);

/**
 * This class provides a "bridge" for communicating between the javascript and
 * the browser. Used as a singleton.
 */
var BrowserBridge = (function() {
  'use strict';

  /**
   * @constructor
   */
  function BrowserBridge() {
  }

  BrowserBridge.prototype = {
    //--------------------------------------------------------------------------
    // Messages sent to the browser
    //--------------------------------------------------------------------------

    sendGetData: function() {
      chrome.send('getData');
    },

    sendResetData: function() {
      chrome.send('resetData');
    },

    //--------------------------------------------------------------------------
    // Messages received from the browser.
    //--------------------------------------------------------------------------

    receivedData: function(data) {
      g_mainView.setData(data);
    },
  };

  return BrowserBridge;
})();

/**
 * This class handles the presentation of our tracking view. Used as a
 * singleton.
 */
var MainView = (function() {
  'use strict';

  /**
   * @constructor
   */
  function MainView() {
  }

  MainView.prototype = {
    setData: function(data) {
      // TODO(eroman): IMPLEMENT.
      alert('data received');
    },
  };

  return MainView;
})();
