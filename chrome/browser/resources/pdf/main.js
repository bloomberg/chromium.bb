// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Global PDFViewer object, accessible for testing.
 * @type Object
 */
var viewer;


(function() {
  /**
   * Stores any pending messages received which should be passed to the
   * PDFViewer when it is created.
   * @type Array
   */
  var pendingMessages = [];

  /**
   * Handles events that are received prior to the PDFViewer being created.
   * @param {Object} message A message event received.
   */
  function handleScriptingMessage(message) {
    pendingMessages.push(message);
  }

  /**
   * Initialize the global PDFViewer and pass any outstanding messages to it.
   * @param {Object} streamDetails The stream object which points to the data
   *     contained in the PDF.
   */
  function initViewer(streamDetails) {
    // PDFViewer will handle any messages after it is created.
    window.removeEventListener('message', handleScriptingMessage, false);
    viewer = new PDFViewer(streamDetails);
    while (pendingMessages.length > 0)
      viewer.handleScriptingMessage(pendingMessages.shift());
  }

  function generateStreamDetailsAndInitViewer() {
    var url = window.location.search.substring(1);
    var streamDetails = {
      streamUrl: url,
      originalUrl: url,
      responseHeaders: '',
      embedded: window.parent != window,
      tabId: -1
    };
    if (!chrome.tabs) {
      initViewer(streamDetails);
      return;
    }
    chrome.tabs.getCurrent(function(tab) {
      streamDetails.tabId = tab.id;
      initViewer(streamDetails);
    });
  }

  /**
   * Entrypoint for starting the PDF viewer. This function obtains the details
   * of the PDF 'stream' (the data that points to the PDF) and constructs a
   * PDFViewer object with it.
   */
  function main() {
    // Set up an event listener to catch scripting messages which are sent prior
    // to the PDFViewer being created.
    window.addEventListener('message', handleScriptingMessage, false);

    if (window.location.search) {
      generateStreamDetailsAndInitViewer();
      return;
    }

    // If the viewer is started from the browser plugin, getStreamInfo will
    // return the details of the stream.
    chrome.mimeHandlerPrivate.getStreamInfo(function(streamDetails) {
      initViewer(streamDetails);
    });
  };

  main();
})();
