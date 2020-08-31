// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Handles automation events on the currently focused node.
 */

goog.provide('FocusAutomationHandler');

goog.require('BaseAutomationHandler');

goog.scope(function() {
const AutomationEvent = chrome.automation.AutomationEvent;
const AutomationNode = chrome.automation.AutomationNode;
const Dir = constants.Dir;
const EventType = chrome.automation.EventType;
const RoleType = chrome.automation.RoleType;
const StateType = chrome.automation.StateType;


FocusAutomationHandler = class extends BaseAutomationHandler {
  constructor() {
    super(null);

    /** @private {AutomationNode|undefined} */
    this.previousActiveDescendant_;

    chrome.automation.getDesktop((desktop) => {
      desktop.addEventListener(EventType.FOCUS, this.onFocus.bind(this), false);
    });
  }

  /**
   * @param {!AutomationEvent} evt
   */
  onFocus(evt) {
    this.removeAllListeners();
    this.previousActiveDescendant_ = evt.target.activeDescendant;
    this.node_ = evt.target;
    this.addListener_(
        EventType.ACTIVEDESCENDANTCHANGED, this.onActiveDescendantChanged);
    this.addListener_(
        EventType.MENU_LIST_ITEM_SELECTED, this.onEventIfSelected);
  }

  /**
   * Handles active descendant changes.
   * @param {!AutomationEvent} evt
   */
  onActiveDescendantChanged(evt) {
    if (!evt.target.activeDescendant || !evt.target.state.focused) {
      return;
    }

    // Various events might come before a key press (which forces flushed
    // speech) and this handler. Force output to be at least category flushed.
    Output.forceModeForNextSpeechUtterance(QueueMode.CATEGORY_FLUSH);

    const prev = this.previousActiveDescendant_ ?
        cursors.Range.fromNode(this.previousActiveDescendant_) :
        ChromeVoxState.instance.currentRange;
    new Output()
        .withRichSpeechAndBraille(
            cursors.Range.fromNode(evt.target.activeDescendant), prev,
            Output.EventType.NAVIGATE)
        .go();
    this.previousActiveDescendant_ = evt.target.activeDescendant;
  }

  /**
   * @param {!ChromeVoxEvent} evt
   */
  onEventIfSelected(evt) {
    if (evt.target.selected) {
      this.onEventDefault(evt);
    }
  }
};

});  // goog.scope
