// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// require cr.js
// require cr/event_target.js
// require cr/util.js

/**
 * Bridge between the browser and the page.
 * In this file:
 *   * define EventTargets to recieve message from the browser,
 *   * dispatch browser messages to EventTarget,
 *   * define interface to request data to the browser.
 */

cr.define('cr.quota', function() {
  'use strict';

  /**
   * Post requestData message to Browser.
   */
  function requestData() {
    chrome.send('requestData');
  }

  /**
   * Callback entry point from Browser.
   * Messages are Dispatched as Event to:
   *   * onAvailableSpaceUpdated,
   *   * onGlobalDataUpdated,
   *   * onHostDataUpdated,
   *   * onOriginDataUpdated,
   *   * onStatisticsUpdated.
   * @param {string} message Message label. Possible Values are:
   *   * 'AvailableSpaceUpdated',
   *   * 'GlobalDataUpdated',
   *   * 'HostDataUpdated',
   *   * 'OriginDataUpdated',
   *   * 'StatisticsUpdated'.
   * @param {Object} detail Message specific additional data.
   */
  function messageHandler(message, detail) {
    var target = null;
    switch (message) {
      case 'AvailableSpaceUpdated':
        target = cr.quota.onAvailableSpaceUpdated;
        break;
      case 'GlobalDataUpdated':
        target = cr.quota.onGlobalDataUpdated;
        break;
      case 'HostDataUpdated':
        target = cr.quota.onHostDataUpdated;
        break;
      case 'OriginDataUpdated':
        target = cr.quota.onOriginDataUpdated;
        break;
      case 'StatisticsUpdated':
        target = cr.quota.onStatisticsUpdated;
        break;
      default:
        console.error('Unknown Message');
        break;
    }
    if (target) {
      var event = cr.doc.createEvent('CustomEvent');
      event.initCustomEvent('update', false, false, detail);
      target.dispatchEvent(event);
    }
  }

  return {
    onAvailableSpaceUpdated: new cr.EventTarget(),
    onGlobalDataUpdated: new cr.EventTarget(),
    onHostDataUpdated: new cr.EventTarget(),
    onOriginDataUpdated: new cr.EventTarget(),
    onStatisticsUpdated: new cr.EventTarget(),

    requestData: requestData,
    messageHandler: messageHandler
  };
});
