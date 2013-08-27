// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * @param {HTMLElement} parentNode Node to be parent for this dialog.
 * @constructor
 * @extends {cr.ui.dialogs.BaseDialog}
 * @implements {ShareClient.Observer}
 */
function ShareDialog(parentNode) {
  this.queue_ = new AsyncUtil.Queue();
  this.onQueueTaskFinished_ = null;
  this.shareClient_ = null;
  this.spinner_ = null;
  this.spinnerWrapper_ = null;
  this.webViewWrapper_ = null;
  this.webView_ = null;
  this.failureTimeout_ = null;

  cr.ui.dialogs.BaseDialog.call(this, parentNode);
}

/**
 * Timeout for loading the share dialog before giving up.
 * @type {number}
 * @const
 */
ShareDialog.FAILURE_TIMEOUT = 5000;

/**
 * Wraps a Web View element and adds authorization headers to it.
 * @param {string} urlPattern Pattern of urls to be authorized.
 * @param {WebView} webView Web View element to be wrapped.
 * @constructor
 */
ShareDialog.WebViewAuthorizer = function(urlPattern, webView) {
  this.urlPattern_ = urlPattern;
  this.webView_ = webView;
  this.initialized_ = false;
  this.accessToken_ = null;
};

/**
 * Initializes the web view by installing hooks injecting the authorization
 * headers.
 * @param {function()} callback Completion callback.
 */
ShareDialog.WebViewAuthorizer.prototype.initialize = function(callback) {
  if (this.initialized_) {
    callback();
    return;
  }

  var registerInjectionHooks = function() {
    this.webView_.removeEventListener('loadstop', registerInjectionHooks);
    this.webView_.request.onBeforeSendHeaders.addListener(
      this.authorizeRequest_.bind(this),
      {urls: [this.urlPattern_]},
      ['blocking', 'requestHeaders']);
    this.initialized_ = true;
    callback();
  }.bind(this);

  this.webView_.addEventListener('loadstop', registerInjectionHooks);
  this.webView_.setAttribute('src', 'data:text/html,');
};

/**
 * Authorizes the web view by fetching the freshest access tokens.
 * @param {function()} callback Completion callback.
 */
ShareDialog.WebViewAuthorizer.prototype.authorize = function(callback) {
  // Fetch or update the access token.
  chrome.fileBrowserPrivate.requestAccessToken(false,  // force_refresh
      function(inAccessToken) {
        this.accessToken_ = inAccessToken;
        callback();
      }.bind(this));
};

/**
 * Injects headers into the passed request.
 * @param {Event} e Request event.
 * @return {{requestHeaders: HttpHeaders}} Modified headers.
 * @private
 */
ShareDialog.WebViewAuthorizer.prototype.authorizeRequest_ = function(e) {
  e.requestHeaders.push({
    name: 'Authorization',
    value: 'Bearer ' + this.accessToken_
  });
  return {requestHeaders: e.requestHeaders};
};

ShareDialog.prototype = {
  __proto__: cr.ui.dialogs.BaseDialog.prototype
};

/**
 * One-time initialization of DOM.
 * @private
 */
ShareDialog.prototype.initDom_ = function() {
  cr.ui.dialogs.BaseDialog.prototype.initDom_.call(this);
  this.frame_.classList.add('share-dialog-frame');

  this.spinnerWrapper_ = this.document_.createElement('div');
  this.spinnerWrapper_.className = 'spinner-container';
  this.frame_.appendChild(this.spinnerWrapper_);

  this.spinner_ = this.document_.createElement('div');
  this.spinner_.className = 'spinner';
  this.spinnerWrapper_.appendChild(this.spinner_);

  this.webViewWrapper_ = this.document_.createElement('div');
  this.webViewWrapper_.className = 'share-dialog-webview-wrapper';
  this.cancelButton_.hidden = true;
  this.okButton_.hidden = true;
  this.frame_.insertBefore(this.webViewWrapper_,
                           this.frame_.querySelector('.cr-dialog-buttons'));
};

/**
 * @override
 */
ShareDialog.prototype.onResized = function(width, height, callback) {
  if (width && height) {
    this.webViewWrapper_.style.width = width + 'px';
    this.webViewWrapper_.style.height = height + 'px';
    this.webView_.style.width = width + 'px';
    this.webView_.style.height = height + 'px';
  }
  setTimeout(callback, 0);
};

/**
 * @override
 */
ShareDialog.prototype.onClosed = function() {
  this.hide();
};

/**
 * @override
 */
ShareDialog.prototype.onLoaded = function() {
  if (this.failureTimeout_) {
    clearTimeout(this.failureTimeout_);
    this.failureTimeout_ = null;
  }

  this.okButton_.hidden = false;
  this.spinnerWrapper_.hidden = true;
  this.webViewWrapper_.classList.add('loaded');
  this.webView_.focus();
};

/**
 * @override
 */
ShareDialog.prototype.onLoadFailed = function() {
  this.hide();
  setTimeout(this.onFailure_.bind(this),
             cr.ui.dialogs.BaseDialog.ANIMATE_STABLE_DURATION);
};

/**
 * @override
 */
ShareDialog.prototype.hide = function() {
  if (this.shareClient_) {
    this.shareClient_.dispose();
    this.shareClient_ = null;
  }
  this.webViewWrapper_.textContent = '';
  this.onQueueTaskFinished_();
  this.onQueueTaskFinished_ = null;
  if (this.failureTimeout_) {
    clearTimeout(this.failureTimeout_);
    this.failureTimeout_ = null;
  }
  cr.ui.dialogs.BaseDialog.prototype.hide.call(this);
};

/**
 * Shows the dialog.
 * @param {FileEntry} entry Entry to share.
 * @param {function()} onFailure Failure callback.
 */
ShareDialog.prototype.show = function(entry, onFailure) {
  this.queue_.run(function(callback) {
    this.onQueueTaskFinished_ = callback;
    this.onFailure_ = onFailure;
    this.spinnerWrapper_.hidden = false;
    this.webViewWrapper_.style.width = '';
    this.webViewWrapper_.style.height = '';

    var onError = function() {
      // Already closed, therefore ignore.
      if (!this.onQueueTaskFinished_)
        return;
      onFailure();
      this.hide();
    }.bind(this);

    // If the embedded share dialog is not started within some time, then
    // give up and show an error message.
    this.failureTimeout_ = setTimeout(function() {
      onError();
    }, ShareDialog.FAILURE_TIMEOUT);

    // TODO(mtomasz): Move to initDom_() once and reuse <webview> once it gets
    // fixed. See: crbug.com/260622.
    this.webView_ = util.createChild(
        this.webViewWrapper_, 'share-dialog-webview', 'webview');
    this.webView_.setAttribute('tabIndex', '-1');
    this.webViewAuthorizer_ = new ShareDialog.WebViewAuthorizer(
        !window.IN_TEST ? (ShareClient.SHARE_TARGET + '/*') : '<all_urls>',
        this.webView_);
    this.webView_.addEventListener('newwindow', function(e) {
      // Discard the window object and reopen in an external window.
      e.window.discard();
      util.visitURL(e.targetUrl);
      e.preventDefault();
    });
    cr.ui.dialogs.BaseDialog.prototype.show.call(this, '', null, null, null);

    // Initialize and authorize the Web View tag asynchronously.
    var group = new AsyncUtil.Group();

    // Fetches an url to the sharing dialog.
    var shareUrl;
    var getShareUrlClosure = function(callback) {
      chrome.fileBrowserPrivate.getShareUrl(
          entry.toURL(),
          function(inShareUrl) {
            if (!chrome.runtime.lastError)
              shareUrl = inShareUrl;
            callback();
          });
    };

    group.add(getShareUrlClosure);
    group.add(this.webViewAuthorizer_.initialize.bind(this.webViewAuthorizer_));
    group.add(this.webViewAuthorizer_.authorize.bind(this.webViewAuthorizer_));

    // Loads the share widget once all the previous async calls are finished.
    group.run(function() {
      if (!shareUrl) {
        onError();
        return;
      }
      // Already closed, therefore ignore.
      if (!this.onQueueTaskFinished_)
        return;
      this.shareClient_ = new ShareClient(this.webView_,
                                          shareUrl,
                                          this);
      this.shareClient_.load();
    }.bind(this));
  }.bind(this));
};

/**
 * Tells whether the share dialog is being shown or not.
 * @return {boolean} True if shown, false otherwise.
 */
ShareDialog.prototype.isShowing = function() {
  return this.container_.classList.contains('shown');
};
