// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Register for events in 4 configurations, then navigate to page2.html, which
// will notify success and succeed the test on the C++ side. The C++ code
// asserts that the events have been unregistered.

// A single listener.
chrome.browserAction.onClicked.addListener(function() {});
// Multiple listeners for the same event.
chrome.runtime.onStartup.addListener(function() {});
chrome.runtime.onStartup.addListener(function() {});
// A single listener, which previously had multiple listeners.
chrome.runtime.onSuspend.addListener(function() {});
chrome.runtime.onSuspend.addListener(function() {});
chrome.runtime.onSuspend.removeListener(function() {});
// No listeners, which previously had listeners (all were removed).
chrome.runtime.onInstalled.addListener(function() {});
chrome.runtime.onInstalled.addListener(function() {});
chrome.runtime.onInstalled.removeListener(function() {});
chrome.runtime.onInstalled.removeListener(function() {});

location.assign('page2.html');
