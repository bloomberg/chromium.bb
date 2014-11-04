// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var appWindow = chrome.app.window.current();

document.addEventListener('DOMContentLoaded', function() {
  var flow = new Flow();
  flow.startFlow();

  var closeAppWindow = function(e) {
    var classes = e.target.classList;
    if (classes.contains('close') || classes.contains('finish-button')) {
      flow.stopTraining();
      appWindow.close();
      e.preventDefault();
    }
  };

  $('steps').addEventListener('click', closeAppWindow);

  $('hw-agree-button').addEventListener('click', function(e) {
    flow.advanceStep();
    flow.startTraining();
    e.preventDefault();
  });

  $('settings-link').addEventListener('click', function(e) {
    chrome.browser.openTab({'url': 'chrome://settings'}, function() {});
    e.preventDefault();
  });
});
