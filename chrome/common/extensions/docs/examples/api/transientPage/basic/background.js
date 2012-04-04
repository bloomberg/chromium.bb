// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Global variables only exist for the life of the page, so they get reset
// each time the page is unloaded.
var counter = 1;

var lastTabId = -1;
function sendMessage() {
  chrome.tabs.query({active: true, currentWindow: true}, function(tabs) {
    lastTabId = tabs[0].id;
    chrome.tabs.sendMessage(lastTabId, "Background page started.");
  });
}

sendMessage();
chrome.browserAction.setBadgeText({text: "ON"});
console.log("Loaded.");

chrome.experimental.runtime.onInstalled.addListener(function() {
  // localStorage is persisted, so it's a good place to keep state that you
  // need to persist across page reloads.
  localStorage.counter = 1;
  console.log("Installed.");
});

chrome.bookmarks.onRemoved.addListener(function(id, info) {
  alert("I never liked that site anyway.");
});

chrome.browserAction.onClicked.addListener(function() {
  // The transient page will unload after handling this event (assuming nothing
  // else is keeping it awake). The content script will become the main way to
  // interact with us.
  chrome.tabs.executeScript({file: "content.js"}, function() {
    // Note: we also sent a message above, upon loading the transient page,
    // but the content script will not be loaded at that point, so we send
    // another here.
    sendMessage();
  });
});

chrome.extension.onMessage.addListener(function(msg, _, sendResponse) {
  if (msg.setAlarm) {
    chrome.experimental.alarms.create({delayInSeconds: 5});
  } else if (msg.delayedResponse) {
    // Note: setTimeout itself does NOT keep the page awake. We return true
    // from the onMessage event handler, which keeps the message channel open -
    // in turn keeping the transient page awake - until we call sendResponse.
    setTimeout(function() {
      sendResponse("Got your message.");
    }, 5000);
    return true;
  } else if (msg.getCounters) {
    sendResponse({counter: counter++,
                  persistentCounter: localStorage.counter++});
  }
  // If we don't return anything, the message channel will close, regardless
  // of whether we called sendResponse.
});

chrome.experimental.alarms.onAlarm.addListener(function() {
  alert("Time's up!");
});

chrome.experimental.runtime.onBackgroundPageUnloadingSoon.addListener(
    function() {
  chrome.tabs.query({active: true, currentWindow: true}, function(tabs) {
    // After the unload event listener runs, the page will unload, so any
    // asynchronous callbacks will not fire.
    alert("This does not show up.");
  });
  console.log("Unloading.");
  chrome.browserAction.setBadgeText({text: ""});
  chrome.tabs.sendMessage(lastTabId, "Background page unloaded.");
});
