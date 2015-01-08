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

  /**
   * Entrypoint for starting the PDF viewer. This function obtains the details
   * of the PDF 'stream' (the data that points to the PDF) and constructs a
   * PDFViewer object with it.
   */
  function main() {
    // Set up an event listener to catch scripting messages which are sent prior
    // to the PDFViewer being created.
    window.addEventListener('message', handleScriptingMessage, false);

    // If the viewer is started from the browser plugin, the view ID will be
    // passed in which identifies the instance of the plugin.
    var params = window.location.search.substring(1).split('=');
    if (params.length == 2 && params[0] == 'id') {
      var viewId = params[1];

      // Send a message to the background page to obtain the stream details. It
      // will run the callback function passed in to initialize the viewer.
      chrome.runtime.sendMessage(
          'mhjfbmdgcfjbbpaeojofohoefgiehjai',
          {viewId: viewId},
          initViewer);
      return;
    }

    // The viewer may be started directly by passing in the URL of the PDF to
    // load as the query string. This is used for print preview in particular.
    // The URL of this page will be of the form
    // 'chrome-extension://<extension id>?<pdf url>'. We pull out the <pdf url>
    // part here.
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
      if (tab && tab.id != undefined)
        streamDetails.tabId = tab.id;
      initViewer(streamDetails);
    });
  }

  main();
})();
