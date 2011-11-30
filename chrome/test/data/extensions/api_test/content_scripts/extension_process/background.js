// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var numPings = 0;
chrome.extension.onRequest.addListener(function(data) {
  if (data != "ping")
    chrome.test.fail("Unexpected request: " + JSON.stringify(data));

  if (++numPings == 2)
    chrome.test.notifyPass();
});

chrome.test.getConfig(function(config) {
  var test_file_url = "http://localhost:PORT/files/extensions/test_file.html"
      .replace(/PORT/, config.testServer.port);

  // Add a window.
  var w = window.open(test_file_url);

  // Add an iframe.
  var iframe = document.createElement("iframe");
  iframe.src = test_file_url;
  document.getElementById("iframeContainer").appendChild(iframe);
});
