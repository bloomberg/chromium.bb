// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @type {number}
 * @const
 */
var FEEDBACK_WIDTH = 500;
/**
 * @type {number}
 * @const
 */
var FEEDBACK_HEIGHT = 610;

var initialFeedbackInfo = null;

/**
 * Callback which gets notified once our feedback UI has loaded and is ready to
 * receive its initial feedback info object.
 * @param {Object} request The message request object.
 * @param {Object} sender The sender of the message.
 * @param {function(Object)} sendResponse Callback for sending a response.
 */
function feedbackReadyHandler(request, sender, sendResponse) {
  if (request.ready) {
    chrome.runtime.sendMessage(
        {sentFromEventPage: true, data: initialFeedbackInfo});
  }
}


/**
 * Callback which gets notified if another extension is requesting feedback.
 * @param {Object} request The message request object.
 * @param {Object} sender The sender of the message.
 * @param {function(Object)} sendResponse Callback for sending a response.
 */
function requestFeedbackHandler(request, sender, sendResponse) {
  if (request.requestFeedback)
    startFeedbackUI(request.feedbackInfo);
}

/**
 * Callback which starts up the feedback UI.
 * @param {Object} feedbackInfo Object containing any initial feedback info.
 */
function startFeedbackUI(feedbackInfo) {
  initialFeedbackInfo = feedbackInfo;
  chrome.app.window.create('html/default.html', {
      frame: 'none',
      id: 'default_window',
      width: FEEDBACK_WIDTH,
      height: FEEDBACK_HEIGHT,
      hidden: true,
      singleton: true },
      function(appWindow) {});
}

chrome.runtime.onMessage.addListener(feedbackReadyHandler);
chrome.runtime.onMessageExternal.addListener(requestFeedbackHandler);
chrome.feedbackPrivate.onFeedbackRequested.addListener(startFeedbackUI);
