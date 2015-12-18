// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.log('executing panel.js ...');

// panel-subframe has to be navigated to another SiteInstance, so that in
// --isolate-extensions mode it gets kicked out to another renderer process and
// therefore has to go through the browser process when trying to find a frame
// via window.open that gets called from panel-subframe.js (this will go through
// BrowsingInstance checks in the browser process).
chrome.test.getConfig(function(config) {
  var baseUri = 'http://foo.com:' + config.testServer.port +
      '/extensions/api_test/window_open/panel_browsing_instance/';

  chrome.test.log('navigating panel-subframe ...');
  var subframe = document.getElementById('panel-subframe-id');
  subframe.src = baseUri +
      'panel-subframe.html?extensionId=' + chrome.runtime.id;
});
