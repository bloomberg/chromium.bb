// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var receivedEvents = [];
var devtoolsTabEvents = undefined;

function pageEventListener() {
  receivedEvents.push("onPageEvent");
}

function tabCloseListener() {
  receivedEvents.push("onTabClose");
}

function setListenersOnTab(tabId) {
  try {
    devtoolsTabEvents = chrome.devtools.getTabEvents(tabId);
    devtoolsTabEvents.onPageEvent.addListener(pageEventListener);
    devtoolsTabEvents.onTabClose.addListener(tabCloseListener);
    window.domAutomationController.send(true);
  } catch(e) {
    window.domAutomationController.send(false);
  }
}

function testReceivePageEvent() {
  if (receivedEvents.length == 1) {
    var eventName = receivedEvents.pop();
    window.domAutomationController.send(eventName === "onPageEvent");
  } else {
    receivedEvents = [];
    window.domAutomationController.send(false);
  }
}

function testReceiveTabCloseEvent() {
  if (receivedEvents.length == 1) {
    var eventName = receivedEvents.pop();
    window.domAutomationController.send(eventName === "onTabClose");
  } else {
    receivedEvents = [];
    window.domAutomationController.send(false);
  }
}

function unregisterListeners() {
  devtoolsTabEvents.onPageEvent.removeListener(pageEventListener);
  devtoolsTabEvents.onTabClose.removeListener(tabCloseListener);
  window.domAutomationController.send(true);
}
