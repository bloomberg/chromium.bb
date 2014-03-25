// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

chrome.app.runtime.onLaunched.addListener(function(launchData) {
  if (!launchData || !launchData.items || launchData.items.length == 0)
    return;

  var entry = launchData.items[0].entry;
  entry.file(function(file) {
    var url = window.URL.createObjectURL(file);
    open(url, entry.name);
  }, function() {
    // TODO(yoshiki): handle error in a smarter way.
    open('', 'error');  // Empty URL shows the error message.
  });
});

function open(url, title) {
  chrome.app.window.create('video_player.html', {
    id: 'video',
    singleton: false,
    minWidth: 160,
    minHeight: 100
  },
  function(createdWindow) {
    createdWindow.setIcon('images/200/icon.png');
    createdWindow.contentWindow.videoUrl = url;
    createdWindow.contentWindow.videoTitle = title;
  });
}
