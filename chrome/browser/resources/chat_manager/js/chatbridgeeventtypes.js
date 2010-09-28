// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Chat bridge event types.
 * @enum {string}
 */
var ChatBridgeEventTypes = {
  SHOW_CHAT: 'showChat',
  START_VIDEO: 'startVideo',
  NEW_VIDEO_CHAT: 'newVideoChat',
  START_VOICE: 'startVoice',
  NEW_VOICE_CHAT: 'newVoiceChat',
  CENTRAL_USER_SET: 'centralJidSet',
  CENTRAL_USER_UPDATE: 'centralJidUpdate',
  CENTRAL_USER_WATCHER: 'getCentralJid',
  OPENED_MOLE_INCOMING: 'onMoleOpened',
  OPENED_MOLE_OUTGOING: 'onCentralMoleOpened',
  CLOSED_MOLE_INCOMING: 'onMoleClosed',
  CLOSED_MOLE_OUTGOING: 'onCentralMoleClosed'
};
