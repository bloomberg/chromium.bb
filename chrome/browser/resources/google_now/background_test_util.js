// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Mocks for globals needed for loading background.js.

function emptyMock() {}

var wrapper = {instrumentChromeApiFunction: emptyMock};

function buildTaskManager() {
  return {debugSetStepName: emptyMock};
}

function buildAuthenticationManager() {
  return {
    addListener: emptyMock
  };
}

var instrumentApiFunction = emptyMock;
var buildAttemptManager = emptyMock;
var buildCardSet = emptyMock;
var emptyListener = {addListener: emptyMock};
var instrumented = {};
instrumented['location'] = {onLocationUpdate: emptyListener};
instrumented['notifications'] = {
  onButtonClicked: emptyListener,
  onClicked: emptyListener,
  onClosed: emptyListener
};
instrumented['omnibox'] = {onInputEntered: emptyListener};
instrumented['preferencesPrivate'] = {
  googleGeolocationAccessEnabled: {
    onChange: emptyListener
  }
};
instrumented['runtime'] = {
  onInstalled: emptyListener,
  onStartup: emptyListener
};
