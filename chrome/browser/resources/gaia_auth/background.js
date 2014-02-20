// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * A background script of the auth extension that bridges the communication
 * between the main and injected scripts.
 *
 * Here is an overview of the communication flow when SAML is being used:
 * 1. The main script sends the |startAuth| signal to this background script,
 *    indicating that the authentication flow has started and SAML pages may be
 *    loaded from now on.
 * 2. A script is injected into each SAML page. The injected script sends three
 *    main types of messages to this background script:
 *    a) A |pageLoaded| message is sent when the page has been loaded. This is
 *       forwarded to the main script as |onAuthPageLoaded|.
 *    b) If the SAML provider supports the credential passing API, the API calls
 *       are sent to this backgroudn script as |apiCall| messages. These
 *       messages are forwarded unmodified to the main script.
 *    c) The injected script scrapes passwords. They are sent to this background
 *       script in |updatePassword| messages. The main script can request a list
 *       of the scraped passwords by sending the |getScrapedPasswords| message.
 */

/**
 * BackgroundBridge allows the main script and the injected script to
 * collaborate. It forwards credentials API calls to the main script and
 * maintains a list of scraped passwords.
 */
function BackgroundBridge() {
}

BackgroundBridge.prototype = {
  // Continue URL that is set from main auth script.
  continueUrl_: null,

  // Whether the extension is loaded in a constrained window.
  // Set from main auth script.
  isConstrainedWindow_: null,

  // Email of the newly authenticated user based on the gaia response header
  // 'google-accounts-signin'.
  email_: null,

  // Session index of the newly authenticated user based on the gaia response
  // header 'google-accounts-signin'.
  sessionIndex_: null,

  // Gaia URL base that is set from main auth script.
  gaiaUrl_: null,

  // Whether auth flow has started. It is used as a signal of whether the
  // injected script should scrape passwords.
  authStarted_: false,

  passwordStore_: {},

  channelMain_: {},
  channelInjected_: {},

  run: function() {
    chrome.runtime.onConnect.addListener(this.onConnect_.bind(this));

    // Workarounds for loading SAML page in an iframe.
    chrome.webRequest.onHeadersReceived.addListener(
        function(details) {
          if (!this.authStarted_)
            return;

          var headers = details.responseHeaders;
          for (var i = 0; headers && i < headers.length; ++i) {
            if (headers[i].name.toLowerCase() == 'x-frame-options') {
              headers.splice(i, 1);
              break;
            }
          }
          return {responseHeaders: headers};
        }.bind(this),
        {urls: ['<all_urls>'], types: ['sub_frame']},
        ['blocking', 'responseHeaders']);
  },

  onConnect_: function(port) {
    if (port.name == 'authMain')
      this.setupForAuthMain_(port);
    else if (port.name == 'injected')
      this.setupForInjected_(port);
    else
      console.error('Unexpected connection, port.name=' + port.name);
  },

  /**
   * Sets up the communication channel with the main script.
   */
  setupForAuthMain_: function(port) {
    var currentChannel = new Channel();
    currentChannel.init(port);

    // Registers for desktop related messages.
    currentChannel.registerMessage(
        'initDesktopFlow', this.onInitDesktopFlow_.bind(this));

    // Registers for SAML related messages.
    currentChannel.registerMessage(
        'setGaiaUrl', this.onSetGaiaUrl_.bind(this));
    currentChannel.registerMessage(
        'resetAuth', this.onResetAuth_.bind(this));
    currentChannel.registerMessage(
        'startAuth', this.onAuthStarted_.bind(this));
    currentChannel.registerMessage(
        'getScrapedPasswords',
        this.onGetScrapedPasswords_.bind(this));

    currentChannel.send({
      'name': 'channelConnected'
    });
    this.channelMain_[this.getTabIdFromPort_(port)] = currentChannel;
  },

  /**
   * Sets up the communication channel with the injected script.
   */
  setupForInjected_: function(port) {
    var currentChannel = new Channel();
    currentChannel.init(port);

    var tabId = this.getTabIdFromPort_(port);
    currentChannel.registerMessage(
        'apiCall', this.onAPICall_.bind(this, tabId));
    currentChannel.registerMessage(
        'updatePassword', this.onUpdatePassword_.bind(this));
    currentChannel.registerMessage(
        'pageLoaded', this.onPageLoaded_.bind(this, tabId));

    this.channelInjected_[this.getTabIdFromPort_(port)] = currentChannel;
  },

  getTabIdFromPort_: function(port) {
    return port.sender.tab ? port.sender.tab.id : -1;
  },

  /**
   * Handler for 'initDesktopFlow' signal sent from the main script.
   * Only called in desktop mode.
   */
  onInitDesktopFlow_: function(msg) {
    this.gaiaUrl_ = msg.gaiaUrl;
    this.continueUrl_ = msg.continueUrl;
    this.isConstrainedWindow_ = msg.isConstrainedWindow;

    var urls = [];
    var filter = {urls: urls, types: ['sub_frame']};
    var optExtraInfoSpec = [];
    if (msg.isConstrainedWindow) {
      urls.push('<all_urls>');
      optExtraInfoSpec.push('responseHeaders');
    } else {
      urls.push(this.continueUrl_ + '*');
    }

    chrome.webRequest.onCompleted.addListener(
        this.onRequestCompletedInDesktopMode_.bind(this),
        filter, optExtraInfoSpec);
    chrome.webRequest.onHeadersReceived.addListener(
        this.onHeadersReceivedInDesktopMode_.bind(this),
        {urls: [this.gaiaUrl_ + '*'], types: ['sub_frame']},
        ['responseHeaders']);
  },

  /**
   * Event listener for webRequest.onCompleted in desktop mode.
   */
  onRequestCompletedInDesktopMode_: function(details) {
    var msg = null;
    if (details.url.lastIndexOf(this.continueUrl_, 0) == 0) {
      var skipForNow = false;
      if (details.url.indexOf('ntp=1') >= 0) {
        skipForNow = true;
      }
      msg = {
        'name': 'completeLogin',
        'email': this.email_,
        'sessionIndex': this.sessionIndex_,
        'skipForNow': skipForNow
      };
    } else if (this.isConstrainedWindow_) {
      var headers = details.responseHeaders;
      for (var i = 0; headers && i < headers.length; ++i) {
        if (headers[i].name.toLowerCase() == 'google-accounts-embedded') {
          return;
        }
      }
      msg = {
        'name': 'switchToFullTab',
        'url': details.url
      };
    }

    if (msg != null)
      this.channelMain_[details.tabId].send(msg);
  },

  /**
   * Event listener for webRequest.onHeadersReceived in desktop mode.
   */
  onHeadersReceivedInDesktopMode_: function(details) {
    var headers = details.responseHeaders;
    for (var i = 0; headers && i < headers.length; ++i) {
      if (headers[i].name.toLowerCase() == 'google-accounts-signin') {
        var headerValues = headers[i].value.toLowerCase().split(',');
        var signinDetails = {};
        headerValues.forEach(function(e) {
          var pair = e.split('=');
          signinDetails[pair[0].trim()] = pair[1].trim();
        });
        this.email_ = signinDetails['email'].slice(1, -1); // Remove "" around.
        this.sessionIndex_ = signinDetails['sessionindex'];
        return;
      }
    }
  },

  /**
   * Handler for 'setGaiaUrl' signal sent from the main script.
   */
  onSetGaiaUrl_: function(msg) {
    this.gaiaUrl_ = msg.gaiaUrl;

    // Set request header to let Gaia know that saml support is on.
    chrome.webRequest.onBeforeSendHeaders.addListener(
        function(details) {
          details.requestHeaders.push({
            name: 'X-Cros-Auth-Ext-Support',
            value: 'SAML'
          });
          return {requestHeaders: details.requestHeaders};
        },
        {urls: [this.gaiaUrl_ + '*'], types: ['sub_frame']},
        ['blocking', 'requestHeaders']);
  },

  /**
   * Handler for 'resetAuth' signal sent from the main script.
   */
  onResetAuth_: function() {
    this.authStarted_ = false;
    this.passwordStore_ = {};
  },

  /**
   * Handler for 'authStarted' signal sent from the main script.
   */
  onAuthStarted_: function() {
    this.authStarted_ = true;
    this.passwordStore_ = {};
  },

  /**
   * Handler for 'getScrapedPasswords' request sent from the main script.
   * @return {Array.<string>} The array with de-duped scraped passwords.
   */
  onGetScrapedPasswords_: function() {
    var passwords = {};
    for (var property in this.passwordStore_) {
      passwords[this.passwordStore_[property]] = true;
    }
    return Object.keys(passwords);
  },

  onAPICall_: function(tabId, msg) {
    if (tabId in this.channelMain_) {
      this.channelMain_[tabId].send(msg);
    }
  },

  onUpdatePassword_: function(msg) {
    if (!this.authStarted_)
      return;

    this.passwordStore_[msg.id] = msg.password;
  },

  onPageLoaded_: function(tabId, msg) {
    if (tabId in this.channelMain_) {
      this.channelMain_[tabId].send({name: 'onAuthPageLoaded', url: msg.url});
    }
  }
};

var backgroundBridge = new BackgroundBridge();
backgroundBridge.run();
