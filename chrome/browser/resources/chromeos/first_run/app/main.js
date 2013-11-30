// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Global wallpaperManager reference useful for poking at from the console.
 */
var wallpaperManager;

function init() {
  chrome.firstRunPrivate.getLocalizedStrings(function(strings) {
    loadTimeData.data = strings;
    i18nTemplate.process(document, loadTimeData);
  });
  var content = $('greeting');
  var closeButton = content.getElementsByClassName('close-button')[0];
  // Make close unfocusable by mouse.
  closeButton.addEventListener('mousedown', function(e) {
    e.preventDefault();
  });
  closeButton.addEventListener('click', function(e) {
    appWindow.close();
    e.stopPropagation();
  });
  var tutorialButton = content.getElementsByClassName('next-button')[0];
  tutorialButton.addEventListener('click', function(e) {
    chrome.firstRunPrivate.launchTutorial();
    appWindow.close();
    e.stopPropagation();
  });
}

document.addEventListener('DOMContentLoaded', init);
