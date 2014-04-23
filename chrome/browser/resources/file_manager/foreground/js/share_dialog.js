// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * @param {HTMLElement} parentNode Node to be parent for this dialog.
 * @constructor
 * @extends {FileManagerDialogBase}
 * @implements {ShareClient.Observer}
 */
function ShareDialog(parentNode) {
  this.queue_ = new AsyncUtil.Queue();
  this.onQueueTaskFinished_ = null;
  this.shareClient_ = null;
  this.spinner_ = null;
  this.spinnerLayer_ = null;
  this.webViewWrapper_ = null;
  this.webView_ = null;
  this.failureTimeout_ = null;
  this.callback_ = null;

  FileManagerDialogBase.call(this, parentNode);
}

/**
 * Timeout for loading the share dialog before giving up.
 * @type {number}
 * @const
 */
ShareDialog.FAILURE_TIMEOUT = 10000;

/**
 * The result of opening the dialog.
 * @enum {string}
 * @const
 */
ShareDialog.Result = Object.freeze({
  // The dialog is closed normally. This includes user cancel.
  SUCCESS: 'success',
  // The dialog is closed by network error.
  NETWORK_ERROR: 'networkError',
  // The dialog is not opened because it is already showing.
  ALREADY_SHOWING: 'alreadyShowing'
});

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
  __proto__: FileManagerDialogBase.prototype
};

/**
 * One-time initialization of DOM.
 * @private
 */
ShareDialog.prototype.initDom_ = function() {
  FileManagerDialogBase.prototype.initDom_.call(this);
  this.frame_.classList.add('share-dialog-frame');

  this.spinnerLayer_ = this.document_.createElement('div');
  this.spinnerLayer_.className = 'spinner-layer';
  this.frame_.appendChild(this.spinnerLayer_);

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

  // Logs added temporarily to track crbug.com/288783.
  console.debug('Loaded.');

  this.okButton_.hidden = false;
  this.spinnerLayer_.hidden = true;
  this.webViewWrapper_.classList.add('loaded');
  this.webView_.focus();
};

/**
 * @override
 */
ShareDialog.prototype.onLoadFailed = function() {
  this.hideWithResult(ShareDialog.Result.NETWORK_ERROR);
};

/**
 * @override
 */
ShareDialog.prototype.hide = function(opt_onHide) {
  this.hideWithResult(ShareDialog.Result.SUCCESS, opt_onHide);
};

/**
 * Hide the dialog with the result and the callback.
 * @param {ShareDialog.Result} result Result passed to the closing callback.
 * @param {function()=} opt_onHide Callback called at the end of hiding.
 */
ShareDialog.prototype.hideWithResult = function(result, opt_onHide) {
  if (!this.isShowing())
    return;

  if (this.shareClient_) {
    this.shareClient_.dispose();
    this.shareClient_ = null;
  }

  this.webViewWrapper_.textContent = '';
  if (this.failureTimeout_) {
    clearTimeout(this.failureTimeout_);
    this.failureTimeout_ = null;
  }

  FileManagerDialogBase.prototype.hide.call(
      this,
      function() {
        if (opt_onHide)
          opt_onHide();
        this.callback_(result);
        this.callback_ = null;
      }.bind(this));
};

/**
 * Shows the dialog.
 * @param {FileEntry} entry Entry to share.
 * @param {function(boolean)} callback Callback to be called when the showing
 *     task is completed. The argument is whether to succeed or not. Note that
 *     cancel is regarded as success.
 */
ShareDialog.prototype.show = function(entry, callback) {
  // If the dialog is already showing, return the error.
  if (this.isShowing()) {
    callback(ShareDialog.Result.ALREADY_SHOWING);
    return;
  }

  // Initialize the variables.
  this.callback_ = callback;
  this.spinnerLayer_.hidden = false;
  this.webViewWrapper_.style.width = '';
  this.webViewWrapper_.style.height = '';

  // If the embedded share dialog is not started within some time, then
  // give up and show an error message.
  this.failureTimeout_ = setTimeout(function() {
    this.hideWithResult(ShareDialog.Result.NETWORK_ERROR);

    // Logs added temporarily to track crbug.com/288783.
    console.debug('Timeout. Web View points at: ' + this.webView_.src);
  }.bind(this), ShareDialog.FAILURE_TIMEOUT);

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
  var show = FileManagerDialogBase.prototype.showBlankDialog.call(this);
  if (!show) {
    // The code shoundn't get here, since already-showing was handled before.
    console.error('ShareDialog can\'t be shown.');
    return;
  }

  // Initialize and authorize the Web View tag asynchronously.
  var group = new AsyncUtil.Group();

  // Fetches an url to the sharing dialog.
  var shareUrl;
  group.add(function(inCallback) {
    chrome.fileBrowserPrivate.getShareUrl(
        entry.toURL(),
        function(inShareUrl) {
          if (!chrome.runtime.lastError)
            shareUrl = inShareUrl;
          inCallback();
        });
  });
  group.add(this.webViewAuthorizer_.initialize.bind(this.webViewAuthorizer_));
  group.add(this.webViewAuthorizer_.authorize.bind(this.webViewAuthorizer_));

  // Loads the share widget once all the previous async calls are finished.
  group.run(function() {
    // If the url is not obtained, return the network error.
    if (!shareUrl) {
       // Logs added temporarily to track crbug.com/288783.
       console.debug('URL not available.');

       this.hideWithResult(ShareDialog.Result.NETWORK_ERROR);
      return;
    }
    // Already inactive, therefore ignore.
    if (!this.isShowing())
      return;
    this.shareClient_ = new ShareClient(this.webView_,
                                        shareUrl,
                                        this);
    this.shareClient_.load();
  }.bind(this));
};

/**
 * Tells whether the share dialog is showing or not.
 *
 * @return {boolean} True since the show method is called and until the closing
 *     callback is invoked.
 */
ShareDialog.prototype.isShowing = function() {
  return !!this.callback_;
};
