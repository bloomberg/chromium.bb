// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Creates event stream logger.
 */

goog.provide('EventStreamLogger');

goog.require('LogStore');
goog.require('TextLog');

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
    /** Event log should not be written when event is dispatched from console or
     * chromevox log page.
     */
    if (target.docUrl &&
        (target.docUrl.indexOf('chrome-devtools://') == 0 ||
         target.docUrl == chrome.runtime.getURL('cvox2/background/log.html')))
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

    LogStore.getInstance().writeTextLog(logStr, TextLog.LogType.EVENT);
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
    for (var type in EventType) {
      if (localStorage[EventType[type]] == 'true')
        this.notifyEventStreamFilterChanged(EventType[type], checked);
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
