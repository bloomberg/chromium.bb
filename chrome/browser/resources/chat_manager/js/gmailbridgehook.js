// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
 * Triggered by Gmail on startup to request the current central roster jid.
 * @param {MessageEvent} event the central roster jid request event.
 */
function requestCentralUserJid(event) {
  chrome.extension.sendRequest(
      {msg: ChatBridgeEventTypes.REQUEST_CENTRAL_USER});
}

/**
 * Initialize a communication channel with a Gmail chat component and the
 * Chrome extension background logic.
 * @param {HTMLElement} divHandler the div element used to communicate.
 */
function attachToDivGmHandler(divHandler) {
  divHandler.addEventListener(ChatBridgeEventTypes.SHOW_CHAT,
      forwardChatEvent, false);
  divHandler.addEventListener(ChatBridgeEventTypes.START_VIDEO,
      forwardChatEvent, false);
  divHandler.addEventListener(ChatBridgeEventTypes.START_VOICE,
      forwardChatEvent, false);
  divHandler.addEventListener(ChatBridgeEventTypes.REQUEST_CENTRAL_USER,
      requestCentralUserJid, false);
  // Set up a direct channel with the extension to forward updated central
  // roster jid's.
  var port = chrome.extension.connect({name: 'centralJidWatcher'});
  port.onMessage.addListener(function(msg) {
    var centralRosterJid = msg.jid;
    var outgoingChatEvent = document.createEvent('MessageEvent');
    outgoingChatEvent.initMessageEvent(
        ChatBridgeEventTypes.CENTRAL_USER_UPDATE, true, true, centralRosterJid);
    divHandler.dispatchEvent(outgoingChatEvent);
  });
}

// Search for communication channel div.
var divGmailHandler = document.getElementById('mainElement');
if (divGmailHandler) {
  attachToDivGmHandler(divGmailHandler);
}
