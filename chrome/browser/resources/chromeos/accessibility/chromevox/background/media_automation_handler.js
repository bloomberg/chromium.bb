// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Handles media automation events.  Note that to perform any of
 * the actions below such as ducking, and suspension of media sessions, the
 * --enable-audio-focus flag must be passed at the command line.
 */

goog.provide('MediaAutomationHandler');

goog.require('BaseAutomationHandler');
goog.require('TtsCapturingEventListener');

goog.scope(function() {
const AutomationEvent = chrome.automation.AutomationEvent;
const AutomationNode = chrome.automation.AutomationNode;
const EventType = chrome.automation.EventType;
const RoleType = chrome.automation.RoleType;

/**
 * @implements {TtsCapturingEventListener}
 */
MediaAutomationHandler = class extends BaseAutomationHandler {
  constructor() {
    super(null);
    /** @type {!Set<AutomationNode>} @private */
    this.mediaRoots_ = new Set();

    /** @type {Date} @private */
    this.lastTtsEvent_ = new Date();

    ChromeVox.tts.addCapturingEventListener(this);

    chrome.automation.getDesktop((node) => {
      this.node_ = node;

      this.addListener_(
          EventType.MEDIA_STARTED_PLAYING, this.onMediaStartedPlaying);
      this.addListener_(
          EventType.MEDIA_STOPPED_PLAYING, this.onMediaStoppedPlaying);
    });
  }

  /** @override */
  onTtsStart() {
    this.lastTtsEvent_ = new Date();
    this.update_({start: true});
  }

  /** @override */
  onTtsEnd() {
    const now = new Date();
    setTimeout(function() {
      const then = this.lastTtsEvent_;
      if (now < then) {
        return;
      }
      this.lastTtsEvent_ = now;
      this.update_({end: true});
    }.bind(this), MediaAutomationHandler.MIN_WAITTIME_MS);
  }

  /** @override */
  onTtsInterrupted() {
    this.onTtsEnd();
  }

  /**
   * @param {!AutomationEvent} evt
   */
  onMediaStartedPlaying(evt) {
    this.mediaRoots_.add(evt.target);
    const audioStrategy = localStorage['audioStrategy'];
    if (ChromeVox.tts.isSpeaking() && audioStrategy == 'audioDuck') {
      this.update_({start: true});
    }
  }

  /**
   * @param {!AutomationEvent} evt
   */
  onMediaStoppedPlaying(evt) {
    // Intentionally does nothing (to cover resume).
  }

  /**
   * Updates the media state for all observed automation roots.
   * @param {{start: (boolean|undefined),
   *          end: (boolean|undefined)}} options
   * @private
   */
  update_(options) {
    const it = this.mediaRoots_.values();
    let item = it.next();
    const audioStrategy = localStorage['audioStrategy'];
    while (!item.done) {
      const root = item.value;
      if (options.start) {
        if (audioStrategy == 'audioDuck') {
          root.startDuckingMedia();
        } else if (audioStrategy == 'audioSuspend') {
          root.suspendMedia();
        }
      } else if (options.end) {
        if (audioStrategy == 'audioDuck') {
          root.stopDuckingMedia();
        } else if (audioStrategy == 'audioSuspend') {
          root.resumeMedia();
        }
      }
      item = it.next();
    }
  }
};

/** @type {number} */
MediaAutomationHandler.MIN_WAITTIME_MS = 1000;

});  // goog.scope
