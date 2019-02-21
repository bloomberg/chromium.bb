// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var video = null;
var box = null;
var video_in_fullscreen = false;

function main() {
  video = document.getElementById("video");
  video.loop = true;
  video.muted = true;
  video.addEventListener('timeupdate', waitForVideoToPlay);
  video.play();
}

function waitForVideoToPlay() {
  if (video.currentTime > 0) {
    video.removeEventListener('timeupdate', waitForVideoToPlay);
    domAutomationController.send("SUCCESS");
  }
}

function goFullscreen() {
  // Call requestFullscreen() on video's parent element, so we could add
  // colored box on top of the video.
  var container = video.parentNode;
  video.style.width="100%";
  video.style.height="100%";
  if (container.requestFullscreen) {
    container.requestFullscreen();
    container.addEventListener("fullscreenchange", function() {
      video_in_fullscreen = true;
    });
  } else if (container.webkitRequestFullscreen) {
    container.webkitRequestFullscreen();
    container.addEventListener("webkitfullscreenchange", function() {
      video_in_fullscreen = true;
    });
  }
}

function isVideoInFullscreen() {
  return video_in_fullscreen;
}

function goUnderlay() {
  if (!box) {
    // Draw a solid color box in the top left corner of the video, so the
    // video becomes an underlay.
    box = document.createElement("div");
    box.style.backgroundColor = "red";
    box.style.width = "100px";
    box.style.height = "50px";
    box.style.position = "absolute";
    box.style.zIndex = "2147483647";  // max zIndex value
    var vid_rect = video.getBoundingClientRect();
    var parent_rect = video.parentNode.getBoundingClientRect();
    var top = vid_rect.top - parent_rect.top + 10;
    var left = vid_rect.left - parent_rect.left + 10;
    box.style.top = top.toString() + "px";
    box.style.left = left.toString() + "px";
    box.style.visibility = "visible";
    video.parentNode.appendChild(box);
  }
  // Change box color between red/blue every second. This is to emulate UI
  // or ads change (once per second) on top of a video.
  setTimeout(setBoxColorToBlue, 1000);
}

function setBoxColorToBlue() {
  if (!box)
    return;
  box.style.backgroundColor = "blue";
  setTimeout(setBoxColorToRed, 1000);
}

function setBoxColorToRed() {
  if (!box)
    return;
  box.style.backgroundColor = "red";
  setTimeout(setBoxColorToBlue, 1000);
}
