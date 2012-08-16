// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function ChromeOSValidator() {
}

ChromeOSValidator.getInstance = function() {
  if (!ChromeOSValidator.instance_) {
    ChromeOSValidator.instance_ = new ChromeOSValidator();
  }
  return ChromeOSValidator.instance_;
};

ChromeOSValidator.prototype = {
  LOADER_ORIGIN: 'chrome-extension://nbicjcbcmclhihdkigkjgkgafckdfcom',
  LOADER_PAGE: '/background.html',
  callback_: undefined,

  validate: function(callback) {
    this.callback_ = callback;
    var msg = { method: 'validate' };
    window.parent.postMessage(msg,
        this.LOADER_ORIGIN + this.LOADER_PAGE);
  },

  initialize: function() {
    window.addEventListener('message', this.onMessage.bind(this), false);
  },

  isValidMessage_: function(msg) {
    return msg.origin == this.LOADER_ORIGIN;
  },

  onMessage: function(e) {
    var msg = e.data;
    if (msg.method == 'validationResults' && this.isValidMessage_(e)) {
      if (this.callback_)
        this.callback_(msg.os == 'ChromeOS');
    } else {
      console.log('#### ChromeOSValidator.onMessage: unknown message');
      if (this.callback_)
        this.callback_(false);
    }
  }
};

ChromeOSValidator.getInstance().initialize();
