// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Mocks for globals needed for loading background.js.

function emptyMock() {}
function buildTaskManager() {
  return {instrumentApiFunction: emptyMock};
}
var instrumentApiFunction = emptyMock;
var buildAttemptManager = emptyMock;
var buildCardSet = emptyMock;
var emptyListener = {addListener: emptyMock};
chrome['location'] = {onLocationUpdate: emptyListener};
chrome['notifications'] = {
  onButtonClicked: emptyListener,
  onClicked: emptyListener,
  onClosed: emptyListener
};
chrome['omnibox'] = {onInputEntered: emptyListener};
chrome['runtime'] = {
  onInstalled: emptyListener,
  onStartup: emptyListener
};

var storage = {};
