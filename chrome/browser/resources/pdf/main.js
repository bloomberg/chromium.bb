// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Global PDFViewer object, accessible for testing.
 * @type Object
 */
var viewer;

/**
 * Entrypoint for starting the PDF viewer. This function obtains the details
 * of the PDF 'stream' (the data that points to the PDF) and constructs a
 * PDFViewer object with it.
 */
(function main() {
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
        function(streamDetails) { viewer = new PDFViewer(streamDetails); });
    return;
  }

  // The viewer may be started directly by passing in the URL of the PDF to load
  // as the query string. This is used for print preview in particular. The URL
  // of this page will be of the form
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
    viewer = new PDFViewer(streamDetails);
    return;
  }
  chrome.tabs.getCurrent(function(tab) {
    if (tab && tab.id != undefined)
      streamDetails.tabId = tab.id;
    viewer = new PDFViewer(streamDetails);
  });
})();
