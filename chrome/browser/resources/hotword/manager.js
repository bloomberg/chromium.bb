// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
  'use strict';

  /**
   * @fileoverview This extension provides hotword triggering capabilites to
   * Chrome.
   *
   * This extension contains all the JavaScript for loading and managing the
   * hotword detector. The hotword detector and language model data will be
   * provided by a shared module loaded from the web store.
   *
   * IMPORTANT! Whenever adding new events, the extension version number MUST be
   * incremented.
   */

  // Hotwording state.
  var stateManager = new hotword.StateManager();

  // Detect Chrome startup and make sure we get a chance to run.
  chrome.runtime.onStartup.addListener(function() {
    stateManager.updateStatus();
  });

  // Detect when hotword settings have changed.
  chrome.hotwordPrivate.onEnabledChanged.addListener(function() {
    stateManager.updateStatus();
  });

  // Detect when the shared module containing the NaCL module and language model
  // is installed.
  chrome.management.onInstalled.addListener(function(info) {
    if (info.id == hotword.constants.SHARED_MODULE_ID)
      chrome.runtime.reload();
  });

  // Detect when a session has requested to be started and stopped.
  chrome.hotwordPrivate.onHotwordSessionRequested.addListener(function() {
    // TODO(amistry): This event should change state depending on whether the
    // user has enabled always-on hotwording. But for now, always signal the
    // start of a hotwording session. This allows this extension to work with
    // the app launcher in the current state.
    stateManager.startSession(
        hotword.constants.SessionSource.LAUNCHER,
        function() {
          chrome.hotwordPrivate.setHotwordSessionState(true, function() {});
        });
  });

  chrome.hotwordPrivate.onHotwordSessionStopped.addListener(function() {
    stateManager.stopSession(hotword.constants.SessionSource.LAUNCHER);
    chrome.hotwordPrivate.setHotwordSessionState(false, function() {});
  });
}());
