// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Port used for:
// 1. forwarding central user requests from the gmail page to the background.
// 2. forwarding the central user from the background to the gmail page.
var centralJidListenerGmailPort;

// The gmail page div used to funnel events through.
var divGmailHandler;

/**
 * Triggered on a user initiated chat request. Forward to extension to be
 * processed by the Chrome central roster.
 * @param {MessageEvent} event the new chat event.
 */
function forwardChatEvent(event) {
  var chatJid = event.data;
  chrome.extension.sendRequest({msg: event.type, jid: chatJid});
}

/**
 * Setup central roster jid listener.
 * @param {MessageEvent} event the event.
 */
function setupCentralRosterJidListener(event) {
  if (!centralJidListenerGmailPort) {
    centralJidListenerGmailPort = chrome.extension.connect(
        {name: 'centralJidListener'});
    centralJidListenerGmailPort.onMessage.addListener(function(msg) {
      var centralRosterJid = msg.jid;
      var outgoingChatEvent = document.createEvent('MessageEvent');
      outgoingChatEvent.initMessageEvent(
          ChatBridgeEventTypes.CENTRAL_USER_UPDATE,
          true, true, centralRosterJid);
      divGmailHandler.dispatchEvent(outgoingChatEvent);
    });
  }
}

// Search for communication channel div.
divGmailHandler = document.getElementById('mainElement');
if (divGmailHandler) {
  divGmailHandler.addEventListener(ChatBridgeEventTypes.SHOW_CHAT,
      forwardChatEvent, false);
  divGmailHandler.addEventListener(ChatBridgeEventTypes.START_VIDEO,
      forwardChatEvent, false);
  divGmailHandler.addEventListener(ChatBridgeEventTypes.START_VOICE,
      forwardChatEvent, false);
  divGmailHandler.addEventListener(ChatBridgeEventTypes.CENTRAL_USER_WATCHER,
      setupCentralRosterJidListener, false);
}
