// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var video;

function main() {
  video = document.getElementById("video");
  video.loop = true;
  video.addEventListener('timeupdate', SendSuccess);
  video.play();
}

function SendSuccess() {
  if (video.currentTime > 0) {
    video.removeEventListener('timeupdate', SendSuccess);
    domAutomationController.send("SUCCESS");
  }
}
