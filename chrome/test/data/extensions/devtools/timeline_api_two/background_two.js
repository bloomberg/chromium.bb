// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var devtoolsTabEvents = undefined;

function pageEventListener() {
  receivedEvents.push("onPageEvent");
}

function tabCloseListener() {
  receivedEvents.push("onTabClose");
}

function registerListenersForTab(tabId) {
  devtoolsTabEvents = chrome.devtools.getTabEvents(tabId);
  devtoolsTabEvents.onPageEvent.addListener(pageEventListener);
  devtoolsTabEvents.onTabClose.addListener(tabCloseListener);
  window.domAutomationController.send(true);
}

function unregisterListeners() {
  devtoolsTabEvents.onPageEvent.removeListener(pageEventListener);
  devtoolsTabEvents.onTabClose.removeListener(tabCloseListener);
  window.domAutomationController.send(true);
}
