// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function () {
// Contains the latest snapshot of sync about info.
chrome.sync.aboutInfo = {};

// TODO(akalin): Make aboutInfo have key names likeThis and not
// like_this.
function refreshAboutInfo(aboutInfo) {
  chrome.sync.aboutInfo = aboutInfo;
  var aboutInfoDiv = document.getElementById('aboutInfo');
  jstProcess(new JsEvalContext(aboutInfo), aboutInfoDiv);
}

function onLoad() {
  chrome.sync.getAboutInfo(refreshAboutInfo);
  chrome.sync.onServiceStateChanged.addListener(function() {
    chrome.sync.getAboutInfo(refreshAboutInfo);
  });
}

document.addEventListener("DOMContentLoaded", onLoad, false);
})();
