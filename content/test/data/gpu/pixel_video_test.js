// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var video;

function main() {
  video = document.getElementById("video");
  video.loop = true;
  video.addEventListener('timeupdate', waitForSwapToComplete);
  video.play();
}

function waitForSwapToComplete() {
  if (video.currentTime > 0) {
    video.removeEventListener('timeupdate', waitForSwapToComplete);
    chrome.gpuBenchmarking.addSwapCompletionEventListener(sendSuccess);
  }
}

function sendSuccess() {
  domAutomationController.send("SUCCESS");
}
