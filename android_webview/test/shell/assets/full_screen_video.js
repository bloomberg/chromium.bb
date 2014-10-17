// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function goFullscreen(id) {
  var element = document.getElementById(id);
  if (element.webkitRequestFullScreen) {
    element.webkitRequestFullScreen();
  }
}

function playVideo() {
    document.getElementById('video').play();
}

addEventListener('DOMContentLoaded', function() {
    document.addEventListener('webkitfullscreenchange', function() {
        javaOnEnterFullscreen.notifyJava();
    }, false);
    document.getElementById('video').addEventListener('play', function() {
        javaOnPlayObserver.notifyJava();
    }, false);
    document.addEventListener('webkitfullscreenerror', function() {
        javaFullScreenErrorObserver.notifyJava();
    }, false);
}, false);

