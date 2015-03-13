// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview An UI component to authenciate to Chrome. The component hosts
 * IdP web pages in a webview. A client who is interested in monitoring
 * authentication events should pass a listener object of type
 * cr.login.GaiaAuthHost.Listener as defined in this file. After initialization,
 * call {@code load} to start the authentication flow.
 */
cr.define('cr.login', function() {
  'use strict';

  // TODO(rogerta): should use gaia URL from GaiaUrls::gaia_url() instead
  // of hardcoding the prod URL here.  As is, this does not work with staging
  // environments.
  var IDP_ORIGIN = 'https://accounts.google.com/';
  var IDP_PATH = 'ServiceLogin?skipvpage=true&sarp=1&rm=hide';
  var CONTINUE_URL =
      'chrome-extension://mfffpogegjflfpflabcdkioaeobkgjik/success.html';
  var SIGN_IN_HEADER = 'google-accounts-signin';
  var EMBEDDED_FORM_HEADER = 'google-accounts-embedded';
  var SAML_HEADER = 'google-accounts-saml';
  var LOCATION_HEADER = 'location';
  var SET_COOKIE_HEADER = 'set-cookie';
  var OAUTH_CODE_COOKIE = 'oauth_code';
  var SERVICE_ID = 'chromeoslogin';

  /**
   * The source URL parameter for the constrained signin flow.
   */
  var CONSTRAINED_FLOW_SOURCE = 'chrome';

  /**
   * Enum for the authorization mode, must match AuthMode defined in
   * chrome/browser/ui/webui/inline_login_ui.cc.
   * @enum {number}
   */
  var AuthMode = {
    DEFAULT: 0,
    OFFLINE: 1,
    DESKTOP: 2
  };

  /**
   * Enum for the authorization type.
   * @enum {number}
   */
  var AuthFlow = {
    DEFAULT: 0,
    SAML: 1
  };

  /**
   * Supported Authenticator params.
   * @type {!Array<string>}
   * @const
   */
  var SUPPORTED_PARAMS = [
    'gaiaUrl',       // Gaia url to use;
    'gaiaPath',      // Gaia path to use without a leading slash;
    'hl',            // Language code for the user interface;
    'email',         // Pre-fill the email field in Gaia UI;
    'service',       // Name of Gaia service;
    'continueUrl',   // Continue url to use;
    'frameUrl',      // Initial frame URL to use. If empty defaults to gaiaUrl.
    'constrained',   // Whether the extension is loaded in a constrained window;
    'clientId'       // Chrome client id;
  ];

  /**
   * Initializes the authenticator component.
   * @param {webview|string} webview The webview element or its ID to host IdP
   *     web pages.
   * @constructor
   */
  function Authenticator(webview) {
    this.webview_ = typeof webview == 'string' ? $(webview) : webview;
    assert(this.webview_);

    this.email_ = null;
    this.password_ = null;
    this.gaiaId_ = null,
    this.sessionIndex_ = null;
    this.chooseWhatToSync_ = false;
    this.skipForNow_ = false;
    this.authFlow_ = AuthFlow.DEFAULT;
    this.loaded_ = false;
    this.idpOrigin_ = null;
    this.continueUrl_ = null;
    this.continueUrlWithoutParams_ = null;
    this.initialFrameUrl_ = null;
    this.reloadUrl_ = null;
    this.trusted_ = true;
    this.oauth_code_ = null;

    this.webview_.addEventListener('droplink', this.onDropLink_.bind(this));
    this.webview_.addEventListener(
        'newwindow', this.onNewWindow_.bind(this));
    this.webview_.addEventListener(
        'contentload', this.onContentLoad_.bind(this));
    this.webview_.addEventListener(
        'loadstop', this.onLoadStop_.bind(this));
    this.webview_.addEventListener(
        'loadcommit', this.onLoadCommit_.bind(this));
    this.webview_.request.onCompleted.addListener(
        this.onRequestCompleted_.bind(this),
        {urls: ['<all_urls>'], types: ['main_frame']},
        ['responseHeaders']);
    this.webview_.request.onHeadersReceived.addListener(
        this.onHeadersReceived_.bind(this),
        {urls: ['<all_urls>'], types: ['main_frame', 'xmlhttprequest']},
        ['responseHeaders']);
    window.addEventListener(
        'message', this.onMessageFromWebview_.bind(this), false);
    window.addEventListener(
        'focus', this.onFocus_.bind(this), false);
    window.addEventListener(
        'popstate', this.onPopState_.bind(this), false);
  }

  // TODO(guohui,xiyuan): no need to inherit EventTarget once we deprecate the
  // old event-based signin flow.
  Authenticator.prototype = Object.create(cr.EventTarget.prototype);

  /**
   * Loads the authenticator component with the given parameters.
   * @param {AuthMode} authMode Authorization mode.
   * @param {Object} data Parameters for the authorization flow.
   */
  Authenticator.prototype.load = function(authMode, data) {
    this.idpOrigin_ = data.gaiaUrl || IDP_ORIGIN;
    this.continueUrl_ = data.continueUrl || CONTINUE_URL;
    this.continueUrlWithoutParams_ =
        this.continueUrl_.substring(0, this.continueUrl_.indexOf('?')) ||
        this.continueUrl_;
    this.isConstrainedWindow_ = data.constrained == '1';
    this.isMinuteMaidChromeOS = data.isMinuteMaidChromeOS;

    this.initialFrameUrl_ = this.constructInitialFrameUrl_(data);
    this.reloadUrl_ = data.frameUrl || this.initialFrameUrl_;
    this.authFlow_ = AuthFlow.DEFAULT;

    this.webview_.src = this.reloadUrl_;

    this.loaded_ = false;
  };

  /**
   * Reloads the authenticator component.
   */
  Authenticator.prototype.reload = function() {
    this.webview_.src = this.reloadUrl_;
    this.authFlow_ = AuthFlow.DEFAULT;
    this.loaded_ = false;
  };

  Authenticator.prototype.constructInitialFrameUrl_ = function(data) {
    var url = this.idpOrigin_ + (data.gaiaPath || IDP_PATH);

    if (this.isMinuteMaidChromeOS) {
      if (data.chromeType)
        url = appendParam(url, 'chrometype', data.chromeType);
      if (data.clientId)
        url = appendParam(url, 'client_id', data.clientId);
      if (data.enterpriseDomain)
        url = appendParam(url, 'managedomain', data.enterpriseDomain);
    } else {
      url = appendParam(url, 'continue', this.continueUrl_);
      url = appendParam(url, 'service', data.service || SERVICE_ID);
    }
    if (data.hl)
      url = appendParam(url, 'hl', data.hl);
    if (data.email)
      url = appendParam(url, 'Email', data.email);
    if (this.isConstrainedWindow_)
      url = appendParam(url, 'source', CONSTRAINED_FLOW_SOURCE);
    return url;
  };

  /**
   * Invoked when a main frame request in the webview has completed.
   * @private
   */
  Authenticator.prototype.onRequestCompleted_ = function(details) {
    var currentUrl = details.url;

    if (currentUrl.lastIndexOf(this.continueUrlWithoutParams_, 0) == 0) {
      if (currentUrl.indexOf('ntp=1') >= 0)
        this.skipForNow_ = true;

      this.onAuthCompleted_();
      return;
    }

    if (currentUrl.indexOf('https') != 0)
      this.trusted_ = false;

    if (this.isConstrainedWindow_) {
      var isEmbeddedPage = false;
      if (this.idpOrigin_ && currentUrl.lastIndexOf(this.idpOrigin_) == 0) {
        var headers = details.responseHeaders;
        for (var i = 0; headers && i < headers.length; ++i) {
          if (headers[i].name.toLowerCase() == EMBEDDED_FORM_HEADER) {
            isEmbeddedPage = true;
            break;
          }
        }
      }
      if (!isEmbeddedPage) {
        this.dispatchEvent(new CustomEvent('resize', {detail: currentUrl}));
        return;
      }
    }

    this.updateHistoryState_(currentUrl);

  };

  /**
    * Manually updates the history. Invoked upon completion of a webview
    * navigation.
    * @param {string} url Request URL.
    * @private
    */
  Authenticator.prototype.updateHistoryState_ = function(url) {
    if (history.state && history.state.url != url)
      history.pushState({url: url}, '');
    else
      history.replaceState({url: url});
  };

  /**
   * Invoked when the sign-in page takes focus.
   * @param {object} e The focus event being triggered.
   * @private
   */
  Authenticator.prototype.onFocus_ = function(e) {
    this.webview_.focus();
  };

  /**
   * Invoked when the history state is changed.
   * @param {object} e The popstate event being triggered.
   * @private
   */
  Authenticator.prototype.onPopState_ = function(e) {
    var state = e.state;
    if (state && state.url)
      this.webview_.src = state.url;
  };

  /**
   * Invoked when headers are received in the main frame of the webview. It
   * 1) reads the authenticated user info from a signin header,
   * 2) signals the start of a saml flow upon receiving a saml header.
   * @return {!Object} Modified request headers.
   * @private
   */
  Authenticator.prototype.onHeadersReceived_ = function(details) {
    var currentUrl = details.url;
    if (currentUrl.lastIndexOf(this.idpOrigin_, 0) != 0)
      return;

    var headers = details.responseHeaders;
    for (var i = 0; headers && i < headers.length; ++i) {
      var header = headers[i];
      var headerName = header.name.toLowerCase();
      if (headerName == SIGN_IN_HEADER) {
        var headerValues = header.value.toLowerCase().split(',');
        var signinDetails = {};
        headerValues.forEach(function(e) {
          var pair = e.split('=');
          signinDetails[pair[0].trim()] = pair[1].trim();
        });
        // Removes "" around.
        this.email_ = signinDetails['email'].slice(1, -1);
        this.gaiaId_ = signinDetails['obfuscatedid'].slice(1, -1);
        this.sessionIndex_ = signinDetails['sessionindex'];
      } else if (headerName == SAML_HEADER) {
        this.authFlow_ = AuthFlow.SAML;
      } else if (headerName == LOCATION_HEADER) {
        // If the "choose what to sync" checkbox was clicked, then the continue
        // URL will contain a source=3 field.
        var location = decodeURIComponent(header.value);
        this.chooseWhatToSync_ = !!location.match(/(\?|&)source=3($|&)/);
      } else if (this.isMinuteMaidChromeOS && headerName == SET_COOKIE_HEADER) {
        var headerValue = header.value;
        if (headerValue.indexOf(OAUTH_CODE_COOKIE + '=', 0) == 0) {
          this.oauth_code_ =
              headerValue.substring(OAUTH_CODE_COOKIE.length + 1).split(';')[0];
        }
      }
    }
  };

  /**
   * Invoked when an HTML5 message is received from the webview element.
   * @param {object} e Payload of the received HTML5 message.
   * @private
   */
  Authenticator.prototype.onMessageFromWebview_ = function(e) {
    // The event origin does not have a trailing slash.
    if (e.origin != this.idpOrigin_.substring(0, this.idpOrigin_.length - 1)) {
      return;
    }

    var msg = e.data;
    if (msg.method == 'attemptLogin') {
      this.email_ = msg.email;
      this.password_ = msg.password;
      this.chooseWhatToSync_ = msg.chooseWhatToSync;
    } else if (msg.method == 'dialogShown') {
      this.dispatchEvent(new Event('dialogShown'));
    } else if (msg.method == 'dialogHidden') {
      this.dispatchEvent(new Event('dialogHidden'));
    } else {
      console.warning('Unrecognized message from GAIA: ' + msg.method);
    }
  };

  /**
   * Invoked to process authentication completion.
   * @private
   */
  Authenticator.prototype.onAuthCompleted_ = function() {
    if (!this.email_ && !this.skipForNow_) {
      this.webview_.src = this.initialFrameUrl_;
      return;
    }

    this.dispatchEvent(
        new CustomEvent('authCompleted',
          // TODO(rsorokin): get rid of the stub values.
                        {detail: {email: this.email_ || '',
                                  gaiaId: this.gaiaId_ || '',
                                  password: this.password_ || '',
                                  authCode: this.oauth_code_,
                                  usingSAML: this.authFlow_ == AuthFlow.SAML,
                                  chooseWhatToSync: this.chooseWhatToSync_,
                                  skipForNow: this.skipForNow_,
                                  sessionIndex: this.sessionIndex_ || '',
                                  trusted: this.trusted_}}));
  };

  /**
   * Invoked when a link is dropped on the webview.
   * @private
   */
  Authenticator.prototype.onDropLink_ = function(e) {
    this.dispatchEvent(new CustomEvent('dropLink', {detail: e.url}));
  };

  /**
   * Invoked when the webview attempts to open a new window.
   * @private
   */
  Authenticator.prototype.onNewWindow_ = function(e) {
    this.dispatchEvent(new CustomEvent('newWindow', {detail: e}));
  };

  /**
   * Invoked when a new document is loaded.
   * @private
   */
  Authenticator.prototype.onContentLoad_ = function(e) {
    // Posts a message to IdP pages to initiate communication.
    var currentUrl = this.webview_.src;
    if (currentUrl.lastIndexOf(this.idpOrigin_) == 0) {
      var msg = {
        'method': 'handshake'
      };
      this.webview_.contentWindow.postMessage(msg, currentUrl);
    }
  };

  /**
   * Invoked when the webview finishes loading a page.
   * @private
   */
  Authenticator.prototype.onLoadStop_ = function(e) {
    if (!this.loaded_) {
      this.loaded_ = true;
      this.dispatchEvent(new Event('ready'));
      // Focus webview after dispatching event when webview is already visible.
      this.webview_.focus();
    }
  };

  /**
   * Invoked when the webview navigates withing the current document.
   * @private
   */
  Authenticator.prototype.onLoadCommit_ = function(e) {
    // TODO(rsorokin): Investigate whether this breaks SAML.
    if (this.oauth_code_) {
      this.skipForNow_ = true;
      this.onAuthCompleted_();
    }
  };

  Authenticator.AuthFlow = AuthFlow;
  Authenticator.AuthMode = AuthMode;
  Authenticator.SUPPORTED_PARAMS = SUPPORTED_PARAMS;

  return {
    // TODO(guohui, xiyuan): Rename GaiaAuthHost to Authenticator once the old
    // iframe-based flow is deprecated.
    GaiaAuthHost: Authenticator
  };
});
