// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.launcherPage.onTransitionChanged.addListener(function(progress) {
  if (progress == 0) {
    chrome.test.sendMessage('onPageProgressAt0');
  } else if (progress == 1) {
    // Push 2 launcher page subpages.
    chrome.launcherPage.pushSubpage(function() {
      chrome.launcherPage.pushSubpage(function() {
        chrome.test.sendMessage('onPageProgressAt1');
      });
    });
  }
})

chrome.launcherPage.onPopSubpage.addListener(function() {
  chrome.test.sendMessage('onPopSubpage');
});

function disableCustomLauncherPage() {
  chrome.launcherPage.setEnabled(false, function() {
      chrome.test.sendMessage('launcherPageDisabled');
  });
}

function enableCustomLauncherPage() {
  chrome.launcherPage.setEnabled(true, function() {
      chrome.test.sendMessage('launcherPageEnabled');
  });
}

document.addEventListener('DOMContentLoaded', function() {
  chrome.test.sendMessage('Launched');
});

var textfield = document.getElementById('textfield');
textfield.onfocus = function() {
  chrome.test.sendMessage('textfieldFocused');
};

textfield.onblur = function() {
  chrome.test.sendMessage('textfieldBlurred');
};
