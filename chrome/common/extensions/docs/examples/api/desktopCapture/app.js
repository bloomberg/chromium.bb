// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function gotStream(stream) {
  console.log("Received local stream");
  var video = document.querySelector("video");
  video.src = webkitURL.createObjectURL(stream);
  localstream = stream;
  stream.onended = function() { console.log("Ended"); };
}

function getUserMediaError() {
  console.log("getUserMedia() failed.");
}

function onAccessApproved(id) {
  if (!id) {
    console.log("Access rejected.");
    return;
  }
  navigator.webkitGetUserMedia({
      audio:false,
      video: { mandatory: { chromeMediaSource: "desktop",
                            chromeMediaSourceId: id } }
  }, gotStream, getUserMediaError);
}

document.querySelector('button').addEventListener('click', function(e) {
  chrome.desktopCapture.chooseDesktopMedia(
      ["screen", "window"], onAccessApproved );
});
