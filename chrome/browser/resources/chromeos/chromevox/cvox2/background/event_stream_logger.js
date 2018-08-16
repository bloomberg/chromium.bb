// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Creates event stream logger.
 */

goog.provide('EventStreamLogger');

goog.scope(function() {
var AutomationEvent = chrome.automation.AutomationEvent;
var AutomationNode = chrome.automation.AutomationNode;
var EventType = chrome.automation.EventType;

/**
 * @constructor
 */
EventStreamLogger = function(node) {
  /**
   * @type {!AutomationNode}
   */
  this.node_ = node;

  /**
   * @type {function(!AutomationEvent): void}
   * @private
   */
  this.watcher_ = this.eventStreamLogging.bind(this);
};

EventStreamLogger.prototype = {
  /**
   * Adds eventStreamLogging to this handler.
   * @param {chrome.automation.EventType} eventType
   * @protected
   */
  addWatcher_: function(eventType) {
    this.node_.addEventListener(eventType, this.watcher_, false);
  },

  /**
   * Removes eventStreamLogging from this handler.
   * @param {chrome.automation.EventType} eventType
   * @protected
   */
  removeWatcher_: function(eventType) {
    this.node_.removeEventListener(eventType, this.watcher_, false);
  },

  /**
   * @param {!AutomationNode} target
   */
  isDescendantOfConsole: function(target) {
    if (target.docUrl && target.docUrl.indexOf('chrome-devtools://') == 0)
      return true;

    if (!target.parent)
      return false;

    return this.isDescendantOfConsole(target.parent);
  },

  /**
   * @param {!AutomationEvent} evt
   */
  eventStreamLogging: function(evt) {
    /**
     * If evt is dispatched to console, don't show.
     * Console event log are unnecessary for developers.
     */
    if (this.isDescendantOfConsole(evt.target))
      return;

    var logStr = 'EventType = ' + evt.type;
    logStr += ', TargetName = ' + evt.target.name;
    logStr += ', RootName = ' + evt.target.root.name;
    logStr += ', DocumentURL = ' + evt.target.docUrl;

    console.log(logStr);
  },

  /**
   * @param {chrome.automation.EventType} eventType
   * @param {boolean} checked
   */
  notifyEventStreamFilterChanged: function(eventType, checked) {
    if (checked) {
      this.addWatcher_(eventType);
    } else {
      this.removeWatcher_(eventType);
    }
  },

  /**
   * @param {boolean} checked
   */
  notifyEventStreamFilterChangedAll: function(checked) {
    var EventTypeList = [
      EventType.ACTIVEDESCENDANTCHANGED,
      EventType.ALERT,
      EventType.ARIA_ATTRIBUTE_CHANGED,
      EventType.AUTOCORRECTION_OCCURED,
      EventType.BLUR,
      EventType.CHECKED_STATE_CHANGED,
      EventType.CHILDREN_CHANGED,
      EventType.CLICKED,
      EventType.DOCUMENT_SELECTION_CHANGED,
      EventType.DOCUMENT_TITLE_CHANGED,
      EventType.EXPANDED_CHANGED,
      EventType.FOCUS,
      EventType.FOCUS_CONTEXT,
      EventType.IMAGE_FRAME_UPDATED,
      EventType.HIDE,
      EventType.HIT_TEST_RESULT,
      EventType.HOVER,
      EventType.INVALID_STATUS_CHANGED,
      EventType.LAYOUT_COMPLETE,
      EventType.LIVE_REGION_CREATED,
      EventType.LIVE_REGION_CHANGED,
      EventType.LOAD_COMPLETE,
      EventType.LOCATION_CHANGED,
      EventType.MEDIA_STARTED_PLAYING,
      EventType.MEDIA_STOPPED_PLAYING,
      EventType.MENU_END,
      EventType.MENU_LIST_ITEM_SELECTED,
      EventType.MENU_LIST_VALUE_CHANGED,
      EventType.MENU_POPUP_END,
      EventType.MENU_POPUP_START,
      EventType.MENU_START,
      EventType.MOUSE_CANCELED,
      EventType.MOUSE_DRAGGED,
      EventType.MOUSE_MOVED,
      EventType.MOUSE_PRESSED,
      EventType.MOUSE_RELEASED,
      EventType.ROW_COLLAPSED,
      EventType.ROW_COUNT_CHANGED,
      EventType.ROW_EXPANDED,
      EventType.SCROLL_POSITION_CHANGED,
      EventType.SCROLLED_TO_ANCHOR,
      EventType.SELECTED_CHILDREN_CHANGED,
      EventType.SELECTION,
      EventType.SELECTION_ADD,
      EventType.SELECTION_REMOVE,
      EventType.SHOW,
      EventType.STATE_CHANGED,
      EventType.TEXT_CHANGED,
      EventType.TEXT_SELECTION_CHANGED,
      EventType.TREE_CHANGED,
      EventType.VALUE_CHANGED
    ];

    for (var evtType of EventTypeList) {
      if (localStorage[evtType] == 'true')
        this.notifyEventStreamFilterChanged(evtType, checked);
    }
  },
};

/**
 * Global instance.
 * @type {EventStreamLogger}
 */
EventStreamLogger.instance;

/**
 * Initializes global state for EventStreamLogger.
 * @private
 */
EventStreamLogger.init_ = function() {
  chrome.automation.getDesktop(function(desktop) {
    EventStreamLogger.instance = new EventStreamLogger(desktop);
    EventStreamLogger.instance.notifyEventStreamFilterChangedAll(
        localStorage['enableEventStreamLogging'] == 'true');
  });
};

EventStreamLogger.init_();

});  // goog.scope
