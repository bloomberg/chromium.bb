// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function Authenticator() {
}

/**
 * Singleton getter of Authenticator.
 * @return {Object} The singleton instance of Authenticator.
 */
Authenticator.getInstance = function() {
  if (!Authenticator.instance_) {
    Authenticator.instance_ = new Authenticator();
  }
  return Authenticator.instance_;
};

Authenticator.prototype = {
  email_: null,
  password_: null,
  attemptToken_: null,

  // Input params from extension initialization URL.
  inputLang_: undefined,
  intputEmail_: undefined,

  GAIA_PAGE_ORIGIN: 'https://accounts.google.com',
  GAIA_PAGE_PATH: '/ServiceLogin?service=chromeoslogin' +
      '&skipvpage=true&sarp=1&rm=hide' +
      '&continue=chrome-extension://mfffpogegjflfpflabcdkioaeobkgjik/' +
      'success.html',
  THIS_EXTENSION_ORIGIN: 'chrome-extension://mfffpogegjflfpflabcdkioaeobkgjik',
  PARENT_PAGE: 'chrome://oobe/',

  initialize: function() {
    var params = getUrlSearchParams(location.search);
    this.parentPage_ = params['parentPage'] || this.PARENT_PAGE;
    this.gaiaOrigin_ = params['gaiaOrigin'] || this.GAIA_PAGE_ORIGIN;
    this.gaiaUrlPath_ = params['gaiaUrlPath'] || '';
    this.inputLang_ = params['hl'];
    this.inputEmail_ = params['email'];
    this.testEmail_ = params['test_email'];
    this.testPassword_ = params['test_password'];

    document.addEventListener('DOMContentLoaded', this.onPageLoad.bind(this));
  },

  isGaiaMessage_: function(msg) {
    return msg.origin == this.gaiaOrigin_ ||
           msg.origin == this.GAIA_PAGE_ORIGIN;
  },

  isInternalMessage_: function(msg) {
    return msg.origin == this.THIS_EXTENSION_ORIGIN;
  },

  getFrameUrl_: function() {
    var url = this.gaiaOrigin_;

    if (this.gaiaOrigin_ == 'https://www.google.com')
      url += '/accounts';

    if (this.gaiaUrlPath_ && this.gaiaUrlPath_ != '')
      url += this.gaiaUrlPath_;

    url += this.GAIA_PAGE_PATH;

    if (this.inputLang_)
      url += '&hl=' + encodeURIComponent(this.inputLang_);
    if (this.inputEmail_)
      url += '&Email=' + encodeURIComponent(this.inputEmail_);
    if (this.testEmail_)
      url += '&test_email=' + encodeURIComponent(this.testEmail_);
    if (this.testPassword_)
      url += '&test_pwd=' + encodeURIComponent(this.testPassword_);
    return url;
  },

  loadFrame_: function() {
    $('gaia-frame').src = this.getFrameUrl_();
  },

  onPageLoad: function(e) {
    window.addEventListener('message', this.onMessage.bind(this), false);
    this.loadFrame_();
  },

  onLoginUILoaded: function() {
    var msg = {
      'method': 'loginUILoaded'
    };
    window.parent.postMessage(msg, this.parentPage_);
  },

  onMessage: function(e) {
    var msg = e.data;
    if (msg.method == 'attemptLogin' && this.isGaiaMessage_(e)) {
      this.email_ = msg.email;
      this.password_ = msg.password;
      this.attemptToken_ = msg.attemptToken;
    } else if (msg.method == 'clearOldAttempts' && this.isGaiaMessage_(e)) {
      this.email_ = null;
      this.password_ = null;
      this.attemptToken_ = null;
      this.onLoginUILoaded();
    } else if (msg.method == 'confirmLogin' && this.isInternalMessage_(e)) {
      if (this.attemptToken_ == msg.attemptToken) {
        var msg = {
          'method': 'completeLogin',
          'email': this.email_,
          'password': this.password_
        };
        window.parent.postMessage(msg, this.parentPage_);
      } else {
        console.log('#### Authenticator.onMessage: unexpected attemptToken!?');
      }
    } else {
      console.log('#### Authenticator.onMessage: unknown message + origin!?');
    }
  }
};

Authenticator.getInstance().initialize();
