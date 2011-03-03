// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('gpu', function() {
  /**
   * This class provides a 'bridge' for communicating between javascript and
   * the browser.
   * @constructor
   */
  function BrowserBridge() {
    // If we are not running inside WebUI, output chrome.send messages
    // to the console to help with quick-iteration debugging.
    if (chrome.send === undefined && console.log) {
      chrome.send = function(messageHandler, args) {
        console.log('chrome.send', messageHandler, args);
      };
    }

    this.nextRequestId_ = 0;
    this.pendingCallbacks_ = [];

    // Tell c++ code that we are ready to receive GPU Info.
    chrome.send('requestGpuInfo');
  }

  BrowserBridge.prototype = {
    __proto__: cr.EventTarget.prototype,

    /**
     * Sends a message to the browser with specified args. The
     * browser will reply asynchronously via the provided callback.
     */
    callAsync: function(submessage, args, callback) {
      var requestId = this.nextRequestId_;
      this.nextRequestId_ += 1;
      this.pendingCallbacks_[requestId] = callback;
      if (!args) {
        chrome.send('callAsync', [requestId.toString(), submessage]);
      } else {
        var allArgs = [requestId.toString(), submessage].concat(args);
        chrome.send('callAsync', allArgs);
      }
    },

    /**
     * Called by gpu c++ code when client info is ready.
     */
    onCallAsyncReply: function(requestId, args) {
      if (this.pendingCallbacks_[requestId] === undefined) {
        throw new Error('requestId ' + requestId + ' is not pending');
      }
      var callback = this.pendingCallbacks_[requestId];
      callback(args);
      delete this.pendingCallbacks_[requestId];
    },

    /**
     * Get gpuInfo data.
     */
    get gpuInfo() {
      return this.gpuInfo_;
    },

    /**
     * Called from gpu c++ code when GPU Info is updated.
     */
    onGpuInfoUpdate: function(gpuInfo) {
      this.gpuInfo_ = gpuInfo;
      cr.dispatchSimpleEvent(this, 'gpuInfoUpdate');
    }
  };

  return {
    BrowserBridge : BrowserBridge
  };
});
