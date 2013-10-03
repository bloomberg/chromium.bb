// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Authenticator class wraps the communications between Gaia and its host.
 */
function Authenticator() {
}

/**
 * Gaia auth extension url origin.
 * @type {string}
 */
Authenticator.THIS_EXTENSION_ORIGIN =
    'chrome-extension://mfffpogegjflfpflabcdkioaeobkgjik';

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

  samlPageLoaded_: false,
  samlSupportChannel_: null,

  GAIA_URL: 'https://accounts.google.com/',
  GAIA_PAGE_PATH: 'ServiceLogin?service=chromeoslogin' +
      '&skipvpage=true&sarp=1&rm=hide',
  PARENT_PAGE: 'chrome://oobe/',
  CONTINUE_URL: Authenticator.THIS_EXTENSION_ORIGIN + '/success.html',

  initialize: function() {
    var params = getUrlSearchParams(location.search);
    this.parentPage_ = params.parentPage || this.PARENT_PAGE;
    this.gaiaUrl_ = params.gaiaUrl || this.GAIA_URL;
    this.inputLang_ = params.hl;
    this.inputEmail_ = params.email;
    this.continueUrl_ = params.continueUrl || this.CONTINUE_URL;
    this.inlineMode_ = params.inlineMode;

    document.addEventListener('DOMContentLoaded', this.onPageLoad.bind(this));
    document.addEventListener('enableSAML', this.onEnableSAML_.bind(this));
  },

  isGaiaMessage_: function(msg) {
    // Not quite right, but good enough.
    return this.gaiaUrl_.indexOf(msg.origin) == 0 ||
           this.GAIA_URL.indexOf(msg.origin) == 0;
  },

  isInternalMessage_: function(msg) {
    return msg.origin == Authenticator.THIS_EXTENSION_ORIGIN;
  },

  isParentMessage_: function(msg) {
    return msg.origin == this.parentPage_;
  },

  getFrameUrl_: function() {
    var url = this.gaiaUrl_;

    url += this.GAIA_PAGE_PATH + '&continue=' +
        encodeURIComponent(this.continueUrl_);

    if (this.inputLang_)
      url += '&hl=' + encodeURIComponent(this.inputLang_);
    if (this.inputEmail_)
      url += '&Email=' + encodeURIComponent(this.inputEmail_);
    return url;
  },

  /** Callback when all loads in the gaia webview is complete. */
  onWebviewLoadstop_: function(gaiaFrame) {
    if (gaiaFrame.src.lastIndexOf(
        this.gaiaUrl_ + this.GAIA_PAGE_PATH, 0) == 0) {
      gaiaFrame.executeScript({file: 'inline_injected.js'}, function() {
        // Send an initial message to gaia so that it has an JavaScript
        // reference to the embedder.
        gaiaFrame.contentWindow.postMessage('', gaiaFrame.src);
      });
      this.onLoginUILoaded();
    } else if (gaiaFrame.src.lastIndexOf(this.continueUrl_, 0) == 0) {
      // Detect when login is finished by the load stop event of the continue
      // URL. Cannot reuse the login complete flow in success.html, because
      // webview does not support extension pages yet.
      gaiaFrame.hidden = true;
      var msg = {'method': 'completeLogin'};
      window.parent.postMessage(msg, this.parentPage_);
    }
  },

  loadFrame_: function() {
    var gaiaFrame = $('gaia-frame');
    gaiaFrame.src = this.getFrameUrl_();
    if (this.inlineMode_) {
      gaiaFrame.addEventListener(
          'loadstop', this.onWebviewLoadstop_.bind(this, gaiaFrame));
    }
  },

  completeLogin: function(username, password) {
    var msg = {
      'method': 'completeLogin',
      'email': username,
      'password': password
    };
    window.parent.postMessage(msg, this.parentPage_);
    if (this.samlSupportChannel_)
      this.samlSupportChannel_.send({name: 'resetAuth'});
  },

  onPageLoad: function(e) {
    window.addEventListener('message', this.onMessage.bind(this), false);
    this.loadFrame_();
  },

  /**
   * Invoked when 'enableSAML' event is received to initialize SAML support.
   */
  onEnableSAML_: function() {
    this.samlPageLoaded_ = false;

    this.samlSupportChannel_ = new Channel();
    this.samlSupportChannel_.connect('authMain');
    this.samlSupportChannel_.registerMessage(
        'onAuthPageLoaded', this.onAuthPageLoaded_.bind(this));
    this.samlSupportChannel_.send({
      name: 'setGaiaUrl',
      gaiaUrl: this.gaiaUrl_
    });
  },

  /**
   * Invoked when the background page sends 'onHostedPageLoaded' message.
   * @param {!Object} msg Details sent with the message.
   */
  onAuthPageLoaded_: function(msg) {
    this.samlPageLoaded_ = msg.url.indexOf(this.gaiaUrl_) != 0;
    window.parent.postMessage({
      'method': 'authPageLoaded',
      'isSAML': this.samlPageLoaded_
    }, this.parentPage_);
  },

  onLoginUILoaded: function() {
    var msg = {
      'method': 'loginUILoaded'
    };
    window.parent.postMessage(msg, this.parentPage_);
  },

  onConfirmLogin_: function() {
    if (!this.samlPageLoaded_) {
      this.completeLogin(this.email_, this.password_);
      return;
    }

    this.samlSupportChannel_.sendWithCallback(
        {name: 'getScrapedPasswords'},
        function(passwords) {
          if (passwords.length == 0) {
            window.parent.postMessage(
                {method: 'noPassword', email: this.email_},
                this.parentPage_);
          } else {
            window.parent.postMessage({method: 'confirmPassword'},
                                      this.parentPage_);
          }
        }.bind(this));
  },

  onVerifyConfirmedPassword_: function(password) {
    this.samlSupportChannel_.sendWithCallback(
        {name: 'getScrapedPasswords'},
        function(passwords) {
          for (var i = 0; i < passwords.length; ++i) {
            if (passwords[i] == password) {
              this.completeLogin(this.email_, passwords[i]);
              return;
            }
          }
          window.parent.postMessage({method: 'confirmPassword'},
                                    this.parentPage_);
        }.bind(this));
  },

  onMessage: function(e) {
    var msg = e.data;
    if (msg.method == 'attemptLogin' && this.isGaiaMessage_(e)) {
      this.email_ = msg.email;
      this.password_ = msg.password;
      this.attemptToken_ = msg.attemptToken;
      this.samlPageLoaded_ = false;
      if (this.samlSupportChannel_)
        this.samlSupportChannel_.send({name: 'startAuth'});
    } else if (msg.method == 'clearOldAttempts' && this.isGaiaMessage_(e)) {
      this.email_ = null;
      this.password_ = null;
      this.attemptToken_ = null;
      this.samlPageLoaded_ = false;
      this.onLoginUILoaded();
      if (this.samlSupportChannel_)
        this.samlSupportChannel_.send({name: 'resetAuth'});
    } else if (msg.method == 'confirmLogin' && this.isInternalMessage_(e)) {
      if (this.attemptToken_ == msg.attemptToken)
        this.onConfirmLogin_();
      else
        console.error('Authenticator.onMessage: unexpected attemptToken!?');
    } else if (msg.method == 'verifyConfirmedPassword' &&
               this.isParentMessage_(e)) {
      this.onVerifyConfirmedPassword_(msg.password);
    } else {
      console.error('Authenticator.onMessage: unknown message + origin!?');
    }
  }
};

Authenticator.getInstance().initialize();
