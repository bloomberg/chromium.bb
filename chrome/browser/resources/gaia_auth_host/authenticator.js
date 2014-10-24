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

  var IDP_ORIGIN = 'https://accounts.google.com/';
  var IDP_PATH = 'ServiceLogin?skipvpage=true&sarp=1&rm=hide';
  var CONTINUE_URL =
      'chrome-extension://mfffpogegjflfpflabcdkioaeobkgjik/success.html';
  var SIGN_IN_HEADER = 'google-accounts-signin';
  var EMBEDDED_FORM_HEADER = 'google-accounts-embedded';
  var SAML_HEADER = 'google-accounts-saml';

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
   * Initializes the authenticator component.
   * @param {webview|string} webview The webview element or its ID to host IdP
   *     web pages.
   * @param {Authenticator.Listener=} opt_listener An optional listener for
   *     authentication events.
   * @constructor
   * @extends {cr.EventTarget}
   */
  function Authenticator(webview, opt_listener) {
    this.webview_ = typeof webview == 'string' ? $(webview) : webview;
    assert(this.webview_);

    this.listener_ = opt_listener || null;

    this.email_ = null;
    this.password_ = null;
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
  }

  // TODO(guohui,xiyuan): no need to inherit EventTarget once we deprecate the
  // old event-based signin flow.
  Authenticator.prototype = Object.create(cr.EventTarget.prototype);

  /**
   * An interface for receiving notifications upon authentication events.
   * @interface
   */
  Authenticator.Listener = function() {};

  /**
   * Invoked when authentication UI is ready.
   */
  Authenticator.Listener.prototype.onReady = function(e) {};

  /**
   * Invoked when authentication is completed successfully with credential data.
   * A credential data object looks like this:
   * <pre>
   * {@code
   * {
   *   email: 'xx@gmail.com',
   *   password: 'xxxx',  // May be null or empty.
   *   usingSAML: false,
   *   chooseWhatToSync: false,
   *   skipForNow: false,
   *   sessionIndex: '0'
   * }
   * }
   * </pre>
   * @param {Object} credentials A credential data object.
   */
  Authenticator.Listener.prototype.onSuccess = function(credentials) {};

  /**
   * Invoked when the requested URL does not fit the container.
   * @param {string} url Request URL.
   */
  Authenticator.Listener.prototype.onResize = function(url) {};

  /**
   * Invoked when a new window event is fired.
   * @param {Event} e Event object.
   */
  Authenticator.Listener.prototype.onNewWindow = function(e) {};

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

    this.initialFrameUrl_ = this.constructInitialFrameUrl_(data);
    this.reloadUrl_ = data.frameUrl || this.initialFrameUrl_;
    this.authFlow_ = AuthFlow.DEFAULT;

    this.webview_.src = this.reloadUrl_;
    this.webview_.addEventListener(
        'newwindow', this.onNewWindow_.bind(this));
    this.webview_.request.onCompleted.addListener(
        this.onRequestCompleted_.bind(this),
        {urls: ['*://*/*', this.continueUrlWithoutParams_ + '*'],
            types: ['main_frame']},
        ['responseHeaders']);
    this.webview_.request.onHeadersReceived.addListener(
        this.onHeadersReceived_.bind(this),
        {urls: [this.idpOrigin_ + '*'], types: ['main_frame']},
        ['responseHeaders']);
    window.addEventListener(
        'message', this.onMessage_.bind(this), false);
  };

  /**
   * Reloads the authenticator component.
   */
  Authenticator.prototype.reload = function() {
    this.webview_.src = this.reloadUrl_;
    this.authFlow_ = AuthFlow.DEFAULT;
  };

  Authenticator.prototype.constructInitialFrameUrl_ = function(data) {
    var url = this.idpOrigin_ + (data.gaiaPath || IDP_PATH);

    url = appendParam(url, 'continue', this.continueUrl_);
    url = appendParam(url, 'service', data.service);
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
      if (currentUrl.indexOf('ntp=1') >= 0) {
        this.skipForNow_ = true;
      }
      this.onAuthCompleted_();
      return;
    }

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
      if (!isEmbeddedPage && this.listener_) {
        this.listener_.onResize(currentUrl);
        return;
      }
    }

    if (currentUrl.lastIndexOf(this.idpOrigin_) == 0) {
      this.webview_.contentWindow.postMessage({}, currentUrl);
    }

    if (!this.loaded_) {
      this.loaded_ = true;
      if (this.listener_) {
        this.listener_.onReady();
      }
    }
  };

  /**
   * Invoked when headers are received in the main frame of the webview. It
   * 1) reads the authenticated user info from a signin header,
   * 2) signals the start of a saml flow upon receiving a saml header.
   * @return {!Object} Modified request headers.
   * @private
   */
  Authenticator.prototype.onHeadersReceived_ = function(details) {
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
        var email = signinDetails['email'].slice(1, -1);
        if (this.email_ != email) {
          this.email_ = email;
          // Clears the scraped password if the email has changed.
          this.password_ = null;
        }
        this.sessionIndex_ = signinDetails['sessionindex'];
      } else if (headerName == SAML_HEADER) {
        this.authFlow_ = AuthFlow.SAML;
      }
    }
  };

  /**
   * Invoked when an HTML5 message is received.
   * @param {object} e Payload of the received HTML5 message.
   * @private
   */
  Authenticator.prototype.onMessage_ = function(e) {
    if (e.origin != this.idpOrigin_) {
      return;
    }

    var msg = e.data;

    if (msg.method == 'attemptLogin') {
      this.email_ = msg.email;
      this.password_ = msg.password;
      this.chooseWhatToSync_ = msg.chooseWhatToSync;
    }
  };

  /**
   * Invoked to process authentication completion.
   * @private
   */
  Authenticator.prototype.onAuthCompleted_ = function() {
    if (!this.listener_) {
      return;
    }

    if (!this.email_ && !this.skipForNow_) {
      this.webview_.src = this.initialFrameUrl_;
      return;
    }

    this.listener_.onSuccess({email: this.email_,
                              password: this.password_,
                              usingSAML: this.authFlow_ == AuthFlow.SAML,
                              chooseWhatToSync: this.chooseWhatToSync_,
                              skipForNow: this.skipForNow_,
                              sessionIndex: this.sessionIndex_ || ''});
  };

  /**
   * Invoked when the webview attempts to open a new window.
   * @private
   */
  Authenticator.prototype.onNewWindow_ = function(e) {
    if (!this.listener_) {
      return;
    }

    this.listener_.onNewWindow(e);
  };

  Authenticator.AuthFlow = AuthFlow;
  Authenticator.AuthMode = AuthMode;

  return {
    // TODO(guohui, xiyuan): Rename GaiaAuthHost to Authenticator once the old
    // iframe-based flow is deprecated.
    GaiaAuthHost: Authenticator
  };
});
