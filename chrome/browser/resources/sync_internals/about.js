// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function highlightIfChanged(node, oldVal, newVal) {
  function clearHighlight() {
    this.removeAttribute('highlighted');
  }

  var oldStr = oldVal.toString();
  var newStr = newVal.toString();
  if (oldStr != '' && oldStr != newStr) {
    // Note the addListener function does not end up creating duplicate
    // listeners.  There can be only one listener per event at a time.
    // Reference: https://developer.mozilla.org/en/DOM/element.addEventListener
    node.addEventListener('webkitAnimationEnd', clearHighlight, false);
    node.setAttribute('highlighted');
  }
}

(function() {
// Contains the latest snapshot of sync about info.
chrome.sync.aboutInfo = {};

// TODO(akalin): Make aboutInfo have key names likeThis and not
// like_this.
function refreshAboutInfo(aboutInfo) {
  chrome.sync.aboutInfo = aboutInfo;
  var aboutInfoDiv = $('aboutInfo');
  jstProcess(new JsEvalContext(aboutInfo), aboutInfoDiv);
}

function onLoad() {
  chrome.sync.getAboutInfo(refreshAboutInfo);

  chrome.sync.onServiceStateChanged.addListener(function() {
    chrome.sync.getAboutInfo(refreshAboutInfo);
  });
}

document.addEventListener('DOMContentLoaded', onLoad, false);
})();
