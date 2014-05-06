// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a minimal sample of a Apps V2 app with background permission.
//
// This function gets called in the packaged app model on launch.
chrome.app.runtime.onLaunched.addListener(function() {
  console.log("Background App Launched!");

  // We'll set up push messaging so we have something to keep the background
  // app registered.
  setupPush();
});

// This function gets called in the packaged app model on install.
chrome.runtime.onInstalled.addListener(function() {
  console.log("Background App installed!");
});

// This function gets called in the packaged app model on shutdown.
chrome.runtime.onSuspend.addListener(function() {
  console.log("Background App shutting down");
});

// Register for push messages.
// This should be called every time the Push Messaging App starts up.
function setupPush() {
  chrome.pushMessaging.onMessage.addListener(messageCallback);

  // Ensure that adding the listener took effect.
  var listeners = chrome.pushMessaging.onMessage.hasListeners();
  console.log('registered listener for push messages ' + listeners);
}

// This callback receives the pushed message from the push server.
function messageCallback(message) {
  console.log("push messaging callback seen");
}
