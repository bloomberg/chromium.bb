// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var g_commands = {
    createTab: executeCreateTab
};

function executeCommand(name, params, callback) {
  g_commands[name](params, callback);
}

function executeCreateTab(params, callback) {
  chrome.tabs.create(params, function() {
    callback(params);
  });
}
