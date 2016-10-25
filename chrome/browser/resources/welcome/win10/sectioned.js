// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('sectioned', function() {
  'use strict';

  function onContinue() {
    chrome.send('handleContinue');
  }

  function onOpenSettings() {
    chrome.send('handleSetDefaultBrowser');
  }

  function onToggle(app) {
    if (app.isCombined) {
      // Toggle sections.
      var sections = document.querySelectorAll('.section.expandable');
      sections.forEach(function(section) {
        section.classList.toggle('expanded');
      });
      // Toggle screenshots.
      var screenshots = document.querySelectorAll('.screenshot-image');
      screenshots.forEach(function(screenshot) {
        screenshot.classList.toggle('hidden');
      });
    }
  }

  function computeClasses(isCombined) {
    if (isCombined)
      return 'section expandable expanded';
    return 'section';
  }

  function initialize() {
    var app = $('sectioned-app');

    app.isCombined = window.location.href.includes('variant=combined');

    // Set handlers.
    app.computeClasses = computeClasses;
    app.onContinue = onContinue;
    app.onOpenSettings = onOpenSettings;
    app.onToggle = onToggle.bind(this, app);
  }

  return {
    initialize: initialize
  };
});

document.addEventListener('DOMContentLoaded', sectioned.initialize);
