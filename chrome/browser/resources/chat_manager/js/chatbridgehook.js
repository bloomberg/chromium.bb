// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Port used for:
// 1. forwarding updated central user from the chat page to the background.
// 2. forwarding chats from the background to the chat page we're attached to.
var centralJidBroadcasterPort;

// Port used for:
// 1. forwarding central user requests from the chat page to the background.
// 2. forwarding the central user from the background to the chat page.
var centralJidListenerChatPort;

// The chat page div used to funnel events through.
var divRosterHandler;

// The current central roster Jid.
// Requested and cached as early as possible.
var centralRosterJid;

/**
 * Triggered on a user initiated chat request. Forward to extension to be
 * processed by the Chrome central roster.
 * @param {MessageEvent} event the new chat event.
 */
function forwardChatEvent(event) {
  var eventType = event.type;
  switch (event.type) {
    case ChatBridgeEventTypes.NEW_VIDEO_CHAT:
      eventType = ChatBridgeEventTypes.START_VIDEO;
      break;
    case ChatBridgeEventTypes.NEW_VOICE_CHAT:
      eventType = ChatBridgeEventTypes.START_VOICE;
      break;
  }
  var chatJid = event.data;
  if (!centralJidBroadcasterPort) {
    chrome.extension.sendRequest({msg: eventType, jid: chatJid});
  } else {
    dispatchChatEvent(eventType, chatJid);
  }
}

/**
 * Forward mole events to extension.
 * Triggered on opening/closing/focus a central roster chat.
 * @param {MessageEvent} event the opened/closed event.
 */
function forwardMoleEvent(event) {
  var eventType = event.type;
  var chatJid = event.data;
  chrome.extension.sendRequest({msg: eventType, jid: chatJid});
}

/**
 * Manage two-way communication with the central roster. Updated jid's are
 * forwarded to the background, while chats are forwarded to the page.
 * @param {string} eventType the event type.
 * @param {string} chatJid the jid to route the chat event to.
 */
function dispatchChatEvent(eventType, chatJid) {
  var chatEvent = document.createEvent('MessageEvent');
  chatEvent.initMessageEvent(eventType, true, true, chatJid);
  divRosterHandler.dispatchEvent(chatEvent);
}

/**
 * Forward central roster Jid to page.
 * @param {string} jid the central roster Jid.
 */
function dispatchCentralJid(jid) {
  dispatchChatEvent(ChatBridgeEventTypes.CENTRAL_USER_UPDATE, jid);
}

/**
 * Manage two-way communication with the central roster. Updated jid's are
 * forwarded to the background, while chats are forwarded to the page.
 * @param {MessageEvent} event the new central roster jid event.
 */
function centralRosterHandler(event) {
  if (!centralJidBroadcasterPort) {
    centralJidBroadcasterPort = chrome.extension.connect(
        {name: 'centralJidBroadcaster'});
    centralJidBroadcasterPort.onMessage.addListener(function(msg) {
      dispatchChatEvent(msg.eventType, msg.jid);
    });
  }
  centralRosterJid = event.data;
  centralJidBroadcasterPort.postMessage({jid: centralRosterJid});
}

/**
 * Setup central roster jid listener.
 * @param {MessageEvent} event the event.
 */
function setupCentralRosterJidListener(event) {
  if (!centralJidListenerChatPort) {
    if (centralRosterJid) {
      dispatchCentralJid(centralRosterJid);
    }
    centralJidListenerChatPort = chrome.extension.connect(
        {name: 'centralJidListener'});
    centralJidListenerChatPort.onMessage.addListener(function(msg) {
      if (msg.eventType == ChatBridgeEventTypes.CENTRAL_USER_UPDATE) {
        centralRosterJid = msg.jid;
      }
      dispatchChatEvent(msg.eventType, msg.jid);
    });
  }
}

/**
 * When the page loads, search for the communication channel div.
 */
function onPageLoaded() {
  divRosterHandler = document.getElementById('roster_comm_link');
  if (divRosterHandler) {
    divRosterHandler.addEventListener(
        ChatBridgeEventTypes.SHOW_CHAT,
        forwardChatEvent, false);
    divRosterHandler.addEventListener(
        ChatBridgeEventTypes.NEW_VIDEO_CHAT,
        forwardChatEvent, false);
    divRosterHandler.addEventListener(
        ChatBridgeEventTypes.NEW_VOICE_CHAT,
        forwardChatEvent, false);
    divRosterHandler.addEventListener(
        ChatBridgeEventTypes.CENTRAL_USER_SET,
        centralRosterHandler, false);
    divRosterHandler.addEventListener(
        ChatBridgeEventTypes.CENTRAL_USER_WATCHER,
        setupCentralRosterJidListener, false);
    divRosterHandler.addEventListener(
        ChatBridgeEventTypes.OPENED_MOLE_INCOMING,
        forwardMoleEvent, false);
    divRosterHandler.addEventListener(
        ChatBridgeEventTypes.CLOSED_MOLE_INCOMING,
        forwardMoleEvent, false);
    divRosterHandler.addEventListener(
        ChatBridgeEventTypes.MOLE_FOCUSED,
        forwardMoleEvent, false);
  }
}

// Retrieve the initial central roster Jid and cache the result.
chrome.extension.sendRequest(
    {msg: ChatBridgeEventTypes.CENTRAL_USER_WATCHER}, function(response) {
      centralRosterJid = response.jid;

      // The initial centralRosterJid is sent in setupCentralRosterJidListener,
      // but if it's already been called, send it here.
      if (centralJidListenerChatPort && centralRosterJid) {
        dispatchCentralJid(centralRosterJid);
      }
    }
);

window.addEventListener("load", onPageLoaded, false);
