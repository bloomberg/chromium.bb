// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.audio.OnDevicesChanged.addListener(function (devices) {
  if (devices.length === 2) {
    if (devices[0].id != "40001" ||
      devices[0].isInput != true ||
      devices[0].deviceType != "USB" ||
      devices[0].deviceName != "Jabra Mic" ||
      devices[0].displayName != "Jabra Mic 1") {
      console.error("Got wrong device property for device:" +
          JSON.stringify(devices[0]));
      chrome.test.sendMessage("failure");
    }
    if (devices[1].id != "40002" ||
        devices[1].isInput != true ||
        devices[1].deviceType != "USB" ||
        devices[1].deviceName != "Jabra Mic" ||
        devices[1].displayName != "Jabra Mic 2") {
      console.error("Got wrong device property for device:" +
          JSON.stringify(devices[1]));
      chrome.test.sendMessage("failure");
    }
    chrome.test.sendMessage("success");
  } else {
    console.error("Got unexpected OnNodesChanged event failed");
    chrome.test.sendMessage("failure");
  }
});
chrome.test.sendMessage("loaded");
