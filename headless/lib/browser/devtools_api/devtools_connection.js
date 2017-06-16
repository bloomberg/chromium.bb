// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Contains a class which marshals DevTools protocol messages over
 * a provided low level message transport. This transport might be a headless
 * TabSocket, or a WebSocket or a mock for testing.
 */

'use strict';

goog.module('chromium.DevTools.Connection');

/**
 * Handles sending and receiving DevTools JSON protocol messages over the
 * provided low level message transport.
 */
class Connection {
  /**
   * @param {!Object} transport The API providing transport for devtools
   *     commands.
   */
  constructor(transport) {
    /** @private {!Object} */
    this.transport_ = transport;

    /** @private {number} */
    this.commandId_ = 1;

    /**
     * An object containing pending DevTools protocol commands keyed by id.
     *
     * @private {!Map<number, !Connection.CallbackFunction>}
     */
    this.pendingCommands_ = new Map();

    /** @private {number} */
    this.nextListenerId_ = 1;

    /**
     * An object containing DevTools protocol events we are listening for keyed
     * by name.
     *
     * @private {!Map<string, !Map<number, !Connection.CallbackFunction>>}
     */
    this.eventListeners_ = new Map();

    /**
     * Used for removing event listeners by id.
     *
     * @private {!Map<number, string>}
     */
    this.eventListenerIdToEventName_ = new Map();

    this.transport_.onmessage = this.onJsonMessage_.bind(this);
  }

  /**
   * Listens for DevTools protocol events of the specified name and issues the
   * callback upon reception.
   *
   * @param {string} eventName Name of the DevTools protocol event to listen
   *     for.
   * @param {!Connection.CallbackFunction} listener The callback issued when we
   *     receive a DevTools protocol event corresponding to the given name.
   * @return {number} The id of this event listener.
   */
  addEventListener(eventName, listener) {
    if (!this.eventListeners_.has(eventName)) {
      this.eventListeners_.set(eventName, new Map());
    }
    let id = this.nextListenerId_++;
    this.eventListeners_.get(eventName).set(id, listener);
    this.eventListenerIdToEventName_.set(id, eventName);
    return id;
  }


  /**
   * Removes an event listener previously added by
   * <code>addEventListener</code>.
   *
   * @param {number} id The id of the event listener to remove.
   * @return {boolean} Whether the event listener was actually removed.
   */
  removeEventListener(id) {
    if (!this.eventListenerIdToEventName_.has(id)) return false;
    let eventName = this.eventListenerIdToEventName_.get(id);
    this.eventListenerIdToEventName_.delete(id);
    // This shouldn't happen, but lets check anyway.
    if (!this.eventListeners_.has(eventName)) return false;
    return this.eventListeners_.get(eventName).delete(id);
  }


  /**
   * Issues a DevTools protocol command and returns a promise for the results.
   *
   * @param {string} method The name of the DevTools protocol command method.
   * @param {!Object=} params An object containing the command parameters if
   *     any.
   * @return {!Promise<!Object>} A promise for the results object.
   */
  sendDevToolsMessage(method, params = {}) {
    let id = this.commandId_;
    // We increment by two because these bindings are intended to be used in
    // conjunction with HeadlessDevToolsClient::RawProtocolListener and using
    // odd numbers for js generated IDs lets the implementation of =
    // OnProtocolMessage easily distinguish between C++ and JS generated
    // commands and route the response accordingly.
    this.commandId_ += 2;
    // Note the names are in quotes to prevent closure compiler name mangling.
    this.transport_.send(
        JSON.stringify({'method': method, 'id': id, 'params': params}));
    return new Promise((resolve, reject) => {
      this.pendingCommands_.set(id, resolve);
    });
  }

  /**
   * If a subclass needs to customize message handling it should override this
   * method.
   *
   * @param {*} message A parsed DevTools protocol message.
   * @param {string} jsonMessage A string containing a JSON DevTools protocol
   *     message.
   * @protected
   */
  onMessage_(message, jsonMessage) {
    if (message.hasOwnProperty('id')) {
      if (!this.pendingCommands_.has(message.id))
        throw new Error('Unrecognized id:' + jsonMessage);
      if (message.hasOwnProperty('error'))
        throw new Error('DevTools protocol error: ' + message.error);
      this.pendingCommands_.get(message.id)(message.result);
      this.pendingCommands_.delete(message.id);
    } else {
      if (!message.hasOwnProperty('method') ||
          !message.hasOwnProperty('params')) {
        throw new Error('Bad message:' + jsonMessage);
      }
      const method = message['method'];
      const params = message['params'];
      if (this.eventListeners_.has(method)) {
        this.eventListeners_.get(method).forEach(function(listener) {
          listener(params);
        });
      }
    }
  }


  /**
   * @param {string} jsonMessage A string containing a JSON DevTools protocol
   *     message.
   * @private
   */
  onJsonMessage_(jsonMessage) {
    this.onMessage_(JSON.parse(jsonMessage), jsonMessage);
  }
}

/**
 * @typedef {function(Object): undefined|function(string): undefined}
 */
Connection.CallbackFunction;

exports = Connection;
