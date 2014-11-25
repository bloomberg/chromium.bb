// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var appWindow = chrome.app.window.current();

document.addEventListener('DOMContentLoaded', function() {
  chrome.hotwordPrivate.getLocalizedStrings(function(strings) {
    loadTimeData.data = strings;
    i18nTemplate.process(document, loadTimeData);

    var flow = new Flow();
    flow.startFlow();

    $('steps').addEventListener('click', function(e) {
      var classes = e.target.classList;
      if (classes.contains('close') || classes.contains('finish-button')) {
        flow.stopTraining();
        appWindow.close();
        e.preventDefault();
      }
      if (classes.contains('retry-button')) {
        flow.handleRetry();
        e.preventDefault();
      }
    });

    // TODO(kcarattini): Change this to update the setting instead of
    // advancing the flow.
    $('audio-history-agree').addEventListener('click', function(e) {
      flow.advanceStep();
      e.preventDefault();
    });

    $('hotword-start').addEventListener('click', function(e) {
      flow.advanceStep();
      e.preventDefault();
    });

    $('settings-link').addEventListener('click', function(e) {
      chrome.browser.openTab({'url': 'chrome://settings'}, function() {});
      e.preventDefault();
    });

  });
});
