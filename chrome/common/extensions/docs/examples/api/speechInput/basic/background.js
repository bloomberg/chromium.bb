// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function setStartIcon() {
  chrome.browserAction.setIcon({ path: "start.png" });
}

function setStopIcon() {
  chrome.browserAction.setIcon({ path: "stop.png" });
}

chrome.browserAction.onClicked.addListener(function(tab) {
  chrome.experimental.speechInput.isRecording(function(recording) {
    if (!recording) {
      chrome.experimental.speechInput.start({}, function() {
        if (chrome.runtime.lastError) {
          alert("Couldn't start speech input: " +
              chrome.runtime.lastError.message);
          setStartIcon();
        } else {
          setStopIcon();
        }
      });
    } else {
      chrome.experimental.speechInput.stop(function() {
        setStartIcon();
      });
    }
  });
});

chrome.experimental.speechInput.onError.addListener(function(error) {
  alert("Speech input failed: " + error.code);
  setStartIcon();
});

chrome.experimental.speechInput.onResult.addListener(function(result) {
  alert(result.hypotheses[0].utterance);
  setStartIcon();
});
