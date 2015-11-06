// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Basic facillities to handle events from a single automation
 * node.
 */

goog.provide('BaseAutomationHandler');

goog.scope(function() {
var AutomationEvent = chrome.automation.AutomationEvent;
var AutomationNode = chrome.automation.AutomationNode;
var EventType = chrome.automation.EventType;

/**
 * @param {!AutomationNode} node
 * @constructor
 */
BaseAutomationHandler = function(node) {
  /**
   * @type {!AutomationNode}
   */
  this.node_ = node;

  /**
   * Maps an automation event to its listener.
   * @type {!Object<EventType, function(Object) : void>}
   */
  this.listenerMap_ = {
    alert: this.onAlert,
    focus: this.onFocus,
    hover: this.onEventDefault,
    loadComplete: this.onLoadComplete,
    menuStart: this.onEventDefault,
    menuEnd: this.onEventDefault,
    textChanged: this.onTextOrTextSelectionChanged,
    textSelectionChanged: this.onTextOrTextSelectionChanged,
    valueChanged: this.onValueChanged
  };

  /** @type {boolean} @private */
  this.isRegistered_ = false;

  /** @type {!Object<string, function(AutomationEvent): void>} @private */
  this.listeners_ = {};

  this.register_();
};

BaseAutomationHandler.prototype = {
  /**
   * Registers event listeners. Can be called repeatedly without duplicate
   * listeners.
   * @private
   */
  register_: function() {
    for (var eventType in this.listenerMap_) {
      var listener =
          this.makeListener_(this.listenerMap_[eventType].bind(this));
      this.node_.addEventListener(eventType, listener, true);
      this.listeners_[eventType] = listener;
    }

    this.isRegistered_ = true;
  },

  /**
   * Unregisters listeners.
   */
  unregister: function() {
    if (!this.isRegistered_)
      throw new Error('Not registered on node ' + this.node_.toString());

    for (var eventType in this.listenerMap_) {
      this.node_.removeEventListener(
          eventType, this.listeners_[eventType], true);
    }

    this.isRegistered_ = false;
  },

  /**
   * @return {!function(AutomationEvent): void}
   * @private
   */
  makeListener_: function(callback) {
    return function(evt) {
      if (this.willHandleEvent_(evt))
        return;
      callback(evt);
      this.didHandleEvent_(evt);
    }.bind(this);
  },

  /**
   * Called before the event |evt| is handled.
   * @return {boolean} True to skip processing this event.
   * @protected
   */
  willHandleEvent_: function(evt) {
    return false;
  },

  /**
    * Called after the event |evt| is handled.
   * @protected
   */
  didHandleEvent_: function(evt) {
  },

  /**
   * @param {Object} evt
   */
  onAlert: function(evt) {},

  /**
   * @param {Object} evt
   */
  onFocus: function(evt) {},

  /**
   * @param {Object} evt
   */
  onLoadComplete: function(evt) {},
  onEventDefault: function(evt) {},

  /**
   * @param {Object} evt
   */
  onTextOrTextSelectionChanged: function(evt) {},

  /**
   * @param {Object} evt
   */
  onValueChanged: function(evt) {}
};

});  // goog.scope
