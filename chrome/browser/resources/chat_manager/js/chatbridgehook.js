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
 * Manage two-way communication with the central roster. Updated jid's are
 * forwarded to the background, while chats are forwarded to the page.
 * @param {string} chatType the chat event type.
 * @param {string} chatJid the jid to route the chat event to.
 */
function dispatchChatEvent(chatType, chatJid) {
  var showChatEvent = document.createEvent('MessageEvent');
  showChatEvent.initMessageEvent(chatType, true, true, chatJid);
  divRosterHandler.dispatchEvent(showChatEvent);
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
      var chatJid = msg.jid;
      dispatchChatEvent(msg.chatType, chatJid);
    });
  }
  var centralRosterJid = event.data;
  centralJidBroadcasterPort.postMessage({jid: centralRosterJid});
}

/**
 * Setup central roster jid listener.
 * @param {MessageEvent} event the event.
 */
function setupCentralRosterJidListener(event) {
  if (!centralJidListenerChatPort) {
    centralJidListenerChatPort = chrome.extension.connect(
        {name: 'centralJidListener'});
    centralJidListenerChatPort.onMessage.addListener(function(msg) {
      var centralRosterJid = msg.jid;
      var outgoingChatEvent = document.createEvent('MessageEvent');
      outgoingChatEvent.initMessageEvent(
          ChatBridgeEventTypes.CENTRAL_USER_UPDATE,
          true, true, centralRosterJid);
      divRosterHandler.dispatchEvent(outgoingChatEvent);
    });
  }
}

// Search for communication channel div.
divRosterHandler = document.getElementById('roster_comm_link');
if (divRosterHandler) {
  divRosterHandler.addEventListener(ChatBridgeEventTypes.SHOW_CHAT,
      forwardChatEvent, false);
  divRosterHandler.addEventListener(ChatBridgeEventTypes.NEW_VIDEO_CHAT,
      forwardChatEvent, false);
  divRosterHandler.addEventListener(ChatBridgeEventTypes.NEW_VOICE_CHAT,
      forwardChatEvent, false);
  divRosterHandler.addEventListener(ChatBridgeEventTypes.CENTRAL_USER_SET,
      centralRosterHandler, false);
  divRosterHandler.addEventListener(ChatBridgeEventTypes.CENTRAL_USER_WATCHER,
      setupCentralRosterJidListener, false);
}
