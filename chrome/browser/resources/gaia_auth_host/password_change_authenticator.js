// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// <include src="saml_handler.js">
// Note: webview_event_manager.js is already included by saml_handler.js.

/**
 * @fileoverview Support password change on with SAML provider.
 */

cr.define('cr.samlPasswordChange', function() {
  'use strict';

  /** @const */
  const oktaInjectedScriptName = 'oktaInjected';

  /**
   * The script to inject into Okta user settings page.
   * @type {string}
   */
  const oktaInjectedJs = String.raw`
      // <include src="okta_detect_success_injected.js">
  `;

  const BLANK_PAGE_URL = 'about:blank';

  /**
   * The different providers of password-change pages that we support, or are
   * working on supporting.
   * @enum {number}
   */
  const PasswordChangePageProvider = {
    UNKNOWN: 0,
    ADFS: 1,
    AZURE: 2,
    OKTA: 3,
    PING: 4,
  };

  /**
   * @param {URL?} url The url of the webpage that is being interacted with.
   * @return {PasswordChangePageProvider} The provider of the password change
   *         page, as detected based on the URL.
   */
  function detectProvider_(url) {
    if (!url) {
      return null;
    }
    if (url.pathname.match(/\/updatepassword\/?$/)) {
      return PasswordChangePageProvider.ADFS;
    }
    if (url.pathname.endsWith('/ChangePassword.aspx')) {
      return PasswordChangePageProvider.AZURE;
    }
    if (url.host.match(/\.okta\.com$/)) {
      return PasswordChangePageProvider.OKTA;
    }
    if (url.pathname.match('/password/chg/')) {
      return PasswordChangePageProvider.PING;
    }
    return PasswordChangePageProvider.UNKNOWN;
  }

  /**
   * @param {string?} str A string that should be a valid URL.
   * @return {URL?} A valid URL object, or null.
   */
  function safeParseUrl_(str) {
    try {
      return new URL(str);
    } catch (error) {
      console.error('Invalid url: ' + str);
      return null;
    }
  }

  /**
   * @param {Object} details The web-request details.
   * @return {boolean} True if we detect that a password change was successful.
   */
  function detectPasswordChangeSuccess(details) {
    const url = safeParseUrl_(details.url);
    if (!url) {
      return false;
    }

    // We count it as a success whenever "status=0" is in the query params.
    // This is what we use for ADFS, but for now, we allow it for every IdP, so
    // that an otherwise unsupported IdP can also send it as a success message.
    // TODO(https://crbug.com/930109): Consider removing this entirely, or,
    // using a more self-documenting parameter like 'passwordChanged=1'.
    if (url.searchParams.get('status') == '0') {
      return true;
    }

    const pageProvider = detectProvider_(url);
    // These heuristics work for the following SAML IdPs:
    if (pageProvider == PasswordChangePageProvider.ADFS) {
      return url.searchParams.get('status') == '0';
    }
    if (pageProvider == PasswordChangePageProvider.AZURE) {
      return url.searchParams.get('ReturnCode') == '0';
    }

    // We can't currently detect success for Okta or Ping just by inspecting the
    // URL or even response headers. To inspect the response body, we need
    // to inject scripts onto their page (see okta_detect_success_injected.js).

    return false;
  }

  /**
   * Initializes the authenticator component.
   */
  class Authenticator extends cr.EventTarget {
    /**
     * @param {webview|string} webview The webview element or its ID to host
     *     IdP web pages.
     */
    constructor(webview) {
      super();

      this.initialFrameUrl_ = null;
      this.webviewEventManager_ = WebviewEventManager.create();

      this.bindToWebview_(webview);

      window.addEventListener('focus', this.onFocus_.bind(this), false);
    }

    /**
     * Reinitializes saml handler.
     */
    resetStates() {
      this.samlHandler_.reset();
    }

    /**
     * Resets the webview to the blank page.
     */
    resetWebview() {
      if (this.webview_.src && this.webview_.src != BLANK_PAGE_URL) {
        this.webview_.src = BLANK_PAGE_URL;
      }
    }

    /**
     * Binds this authenticator to the passed webview.
     * @param {!Object} webview the new webview to be used by this
     *     Authenticator.
     * @private
     */
    bindToWebview_(webview) {
      assert(!this.webview_);
      assert(!this.samlHandler_);

      this.webview_ = typeof webview == 'string' ? $(webview) : webview;

      this.samlHandler_ =
          new cr.login.SamlHandler(this.webview_, true /* startsOnSamlPage */);
      this.webviewEventManager_.addEventListener(
          this.samlHandler_, 'authPageLoaded',
          this.onAuthPageLoaded_.bind(this));

      // Listen for completed main-frame requests to check for password-change
      // success.
      this.webviewEventManager_.addWebRequestEventListener(
          this.webview_.request.onCompleted,
          this.onCompleted_.bind(this),
          {urls: ['*://*/*'], types: ['main_frame']},
      );

      // Inject a custom script for detecting password change success in Okta.
      this.webview_.addContentScripts([{
        name: oktaInjectedScriptName,
        matches: ['*://*.okta.com/*'],
        js: {code: oktaInjectedJs},
        all_frames: true,
        run_at: 'document_start'
      }]);

      // Okta-detect-success-inject script signals success by posting a message
      // that says "passwordChangeSuccess", which we listen for:
      this.webviewEventManager_.addEventListener(
          window, 'message', this.onMessageReceived_.bind(this));
    }

    /**
     * Unbinds this Authenticator from the currently bound webview.
     * @private
     */
    unbindFromWebview_() {
      assert(this.webview_);
      assert(this.samlHandler_);

      this.webviewEventManager_.removeAllListeners();

      this.webview_ = undefined;
      this.samlHandler_.unbindFromWebview();
      this.samlHandler_ = undefined;
    }

    /**
     * Re-binds to another webview.
     * @param {Object} webview the new webview to be used by this Authenticator.
     */
    rebindWebview(webview) {
      this.unbindFromWebview_();
      this.bindToWebview_(webview);
    }

    /**
     * Loads the authenticator component with the given parameters.
     * @param {AuthMode} authMode Authorization mode.
     * @param {Object} data Parameters for the authorization flow.
     */
    load(data) {
      this.resetStates();
      this.initialFrameUrl_ = this.constructInitialFrameUrl_(data);
      this.samlHandler_.blockInsecureContent = true;
      this.webview_.src = this.initialFrameUrl_;
    }

    constructInitialFrameUrl_(data) {
      let url;
      url = data.passwordChangeUrl;
      if (data.userName) {
        url = appendParam(url, 'username', data.userName);
      }
      return url;
    }

    /**
     * Invoked when the sign-in page takes focus.
     * @param {object} e The focus event being triggered.
     * @private
     */
    onFocus_(e) {
      this.webview_.focus();
    }

    /**
     * Sends scraped password and resets the state.
     * @private
     */
    onPasswordChangeSuccess_() {
      const passwordsOnce = this.samlHandler_.getPasswordsScrapedTimes(1);
      const passwordsTwice = this.samlHandler_.getPasswordsScrapedTimes(2);

      this.dispatchEvent(new CustomEvent('authCompleted', {
        detail: {
          old_passwords: passwordsOnce,
          new_passwords: passwordsTwice,
        }
      }));
      this.resetStates();
    }

    /**
     * Invoked when |samlHandler_| fires 'authPageLoaded' event.
     * @private
     */
    onAuthPageLoaded_(e) {
      this.webview_.focus();
    }

    /**
     * Invoked when a new document loading completes.
     * @param {Object} details The web-request details.
     * @private
     */
    onCompleted_(details) {
      if (detectPasswordChangeSuccess(details)) {
        this.onPasswordChangeSuccess_();
      }

      // Okta_detect_success_injected.js needs to be contacted by the parent,
      // so that it can send messages back to the parent.
      const pageProvider = detectProvider_(safeParseUrl_(details.url));
      if (pageProvider == PasswordChangePageProvider.OKTA) {
        // Using setTimeout gives the page time to finish initializing.
        setTimeout(() => {
          this.webview_.contentWindow.postMessage('connect', details.url);
        }, 1000);
      }
    }

    /**
     * Invoked when the webview posts a message.
     * @param {Object} event The message event.
     * @private
     */
    onMessageReceived_(event) {
      if (event.data == 'passwordChangeSuccess') {
        const pageProvider = detectProvider_(safeParseUrl_(event.origin));
        if (pageProvider == PasswordChangePageProvider.OKTA) {
          this.onPasswordChangeSuccess_();
        }
      }
    }
  }

  return {
    Authenticator: Authenticator,
    detectPasswordChangeSuccess: detectPasswordChangeSuccess,
  };
});
