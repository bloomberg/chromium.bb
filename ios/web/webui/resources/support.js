// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Module "mojo/public/js/support"
//
// This module provides the JavaScript bindings for mojo/public/c/system/core.h.
// Refer to that file for more detailed documentation for equivalent methods.

define("mojo/public/js/support", [
  "ios/mojo/public/js/sync_message_channel",
  "ios/mojo/public/js/handle_util",
], function(syncMessageChannel, handleUtil) {

  /**
   * Holds callbacks for all currently active watches.
   * @constructor
   */
  var MojoWatchCallbacksHolder = function() {
    /**
     * Next callback Id to be used for watch.
     * @private {number}
     */
    this.nextCallbackId_ = 0;

    /**
     * Map where keys are callbacks ids and values are callbacks.
     * @private {!Map}
     */
    this.callbacks_ = new Map();

    /**
     * Map where keys are watch ids and values are callback ids.
     * @private {!Map}
     */
    this.callbackIds_ = new Map();
  };


  MojoWatchCallbacksHolder.prototype = {
    /**
     * Calls watch callback.
     *
     * @param {number} callbackId callback id previously returned from
           {@code getNextCallbackId}.
     * @param {!MojoResult} mojoResult to call callback with.
     */
    callCallback: function(callbackId, mojoResult) {
      var callback = this.callbackIds_.get(callbackId);

      // Signalling the watch is asynchronous operation and this function may be
      // called for already removed watch.
      if (callback)
        callback(mojoResult);
    },

    /**
     * Returns next callback Id to be used for watch (idempotent).
     *
     * @return {number} callback id.
     */
    getNextCallbackId: function() {
      return this.nextCallbackId_;
    },

    /**
     * Adds callback which must be executed when the Watch fires.
     *
     * @param {!MojoWatchId} watchId returned from "support.watch" mojo
           backend.
     * @param {!function (mojoResult)} callback which should be executed when
           the Watch fires.
     */
    addWatchCallback: function (watchId, callback) {
      this.callbackIds_.set(watchId, this.nextCallbackId_);
      this.callbackIds_.set(this.nextCallbackId_, callback);
      ++this.nextCallbackId_;
    },

    /**
     * Removes callback which should no longer be executed.
     *
     * @param {!MojoWatchId} watchId to remove callback for.
     */
    removeWatchCallback: function (watchId) {
      this.callbacks_.delete(this.callbackIds_.get(watchId));
      this.callbackIds_.delete(watchId);
    },
  };

  var watchCallbacksHolder = new MojoWatchCallbacksHolder();


  /**
   * Public functions called by Mojo backend when watch callback fires.
   * @public {!function(callbackId, mojoResult)}
   */
  __crWeb.mojo = {};
  __crWeb.mojo.signalWatch = function(callbackId, mojoResult) {
    watchCallbacksHolder.callCallback(callbackId, mojoResult);
  }

 /*
  * Begins watching a handle for |signals| to be satisfied or unsatisfiable.
  *
  * @param {!MojoHandle} handle The handle to watch.
  * @param {!MojoHandleSignals} signals The signals to watch.
  * @param {!function (mojoResult)} calback Called with a result any time
  *     the watched signals become satisfied or unsatisfiable.
  *
  * @return {!MojoWatchId} watchId An opaque identifier that identifies this
  *     watch.
  */
  function watch(handle, signals, callback) {
    var watchId = syncMessageChannel.sendMessage({
      name: "support.watch",
      args: {
         handle: handleUtil.getNativeHandle(handle),
         signals: signals,
         callbackId: watchCallbacksHolder.getNextCallbackId()
      }
    });
    watchCallbacksHolder.addWatchCallback(watchId, callback);
    return watchId;
  }

  /*
   * Cancels a handle watch initiated by watch().
   *
   * @param {!MojoWatchId} watchId The watch identifier returned by watch().
   */
  function cancelWatch(watchId) {
    syncMessageChannel.sendMessage({
      name: "support.cancelWatch",
      args: {
         watchId: watchId
      }
    });
    watchCallbacksHolder.removeWatchCallback(watchId);
  }

  var exports = {};
  exports.watch = watch;
  exports.cancelWatch = cancelWatch;
  return exports;
});
