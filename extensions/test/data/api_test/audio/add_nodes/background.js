// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.audio.OnDevicesChanged.addListener(function (devices) {
  if (devices.length === 3) {
    if (devices[0].id != "30001" ||
        devices[0].isInput != false ||
        devices[0].deviceType != "USB" ||
        devices[0].deviceName != "Jabra Speaker" ||
        devices[0].displayName != "Jabra Speaker 1") {
      console.error("Got wrong device property for device:" +
          JSON.stringify(devices[0]));
      chrome.test.sendMessage("failure");
    }
    if (devices[1].id != "30002" ||
        devices[1].isInput != false ||
        devices[1].deviceType != "USB" ||
        devices[1].deviceName != "Jabra Speaker" ||
        devices[1].displayName != "Jabra Speaker 2") {
      console.error("Got wrong device property for device:" +
          JSON.stringify(devices[1]));
      chrome.test.sendMessage("failure");
    }
    if (devices[2].id != "30003" ||
        devices[2].isInput != false ||
        devices[2].deviceType != "HDMI" ||
        devices[2].deviceName != "HDMI output" ||
        devices[2].displayName != "HDA Intel MID") {
      console.error("Got wrong device property for device:" +
          JSON.stringify(devices[2]));
      chrome.test.sendMessage("failure");
    }
    chrome.test.sendMessage("success");
  } else {
    console.error("Got unexpected OnNodesChanged event failed");
    chrome.test.sendMessage("failure");
  }
});
chrome.test.sendMessage("loaded");
