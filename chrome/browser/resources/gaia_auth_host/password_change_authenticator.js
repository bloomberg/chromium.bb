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

  const BLANK_PAGE_URL = 'about:blank';

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

      this.webviewEventManager_.addEventListener(
          this.webview_, 'contentload', this.onContentLoad_.bind(this));
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
    completeAuth_() {
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
     * Invoked when a new document is loaded.
     * @private
     */
    onContentLoad_(e) {
      const currentUrl = this.webview_.src;
      // TODO(rsorokin): Implement more robust check.
      if (currentUrl.lastIndexOf('status=0') != -1) {
        this.completeAuth_();
      }
    }
  }

  return {Authenticator: Authenticator};
});
