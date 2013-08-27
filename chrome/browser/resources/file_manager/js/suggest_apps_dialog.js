// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * SuggestAppsDialog contains a list box to select an app to be opened the file
 * with. This dialog should be used as action picker for file operations.
 */

/**
 * The width of the widget (in pixel).
 * @type {number}
 * @const
 */
var WEBVIEW_WIDTH = 400;
/**
 * The height of the widget (in pixel).
 * @type {number}
 * @const
 */
var WEBVIEW_HEIGHT = 480;

/**
 * The URL of the widget.
 * @type {string}
 * @const
 */
var CWS_WIDGET_URL =
    'https://clients5.google.com/webstore/wall/cros-widget-container';
/**
 * The origin of the widget.
 * @type {string}
 * @const
 */
var CWS_WIDGET_ORIGIN = 'https://clients5.google.com';

/**
 * RegExp to extract the origin (shema, host and port) from the URL.
 * TODO(yoshiki): Remove this before ShareDialog launches or M31 branch cut.
 *
 * @type {RegExp}
 * @const
 */
var REGEXP_EXTRACT_HOST = /^https?:\/\/[\w\.\-]+(?:\:\d{1,5})?(?=\/)/;

/**
 * RegExp to check if the origin is google host or not.
 * Google hosts must be on https and default port.
 * TODO(yoshiki): Remove this before ShareDialog launches or M31 branch cut.
 *
 * @type {RegExp}
 * @const
 */
var REGEXP_GOOGLE_MATCH = /^https:\/\/[\w\.\-]+\.google\.com$/;

/**
 * RegExp to check if the origin is localhost or not.
 * TODO(yoshiki): Remove this before ShareDialog launches or M31 branch cut.
 *
 * @type {RegExp}
 * @const
 */
var REGEXP_LOCALHOST_MATCH = /^https?:\/\/localhost(?:\:\d{1,5})?$/;

/**
 * Creates dialog in DOM tree.
 *
 * @param {HTMLElement} parentNode Node to be parent for this dialog.
 * @constructor
 * @extends {cr.ui.dialogs.BaseDialog}
 */
function SuggestAppsDialog(parentNode) {
  cr.ui.dialogs.BaseDialog.call(this, parentNode);

  this.frame_.id = 'suggest-app-dialog';

  this.webviewContainer_ = this.document_.createElement('div');
  this.webviewContainer_.id = 'app-list';
  this.frame_.insertBefore(this.webviewContainer_, this.text_.nextSibling);

  this.buttons_ = this.document_.createElement('div');
  this.buttons_.id = 'buttons';
  this.frame_.appendChild(this.buttons_);

  this.webstoreButton_ = this.document_.createElement('div');
  this.webstoreButton_.id = 'webstore-button';
  this.webstoreButton_.innerHTML = str('SUGGEST_DIALOG_LINK_TO_WEBSTORE');
  this.webstoreButton_.addEventListener(
      'click', this.onWebstoreLinkClicked_.bind(this));
  this.buttons_.appendChild(this.webstoreButton_);

  this.initialFocusElement_ = this.webviewContainer_;

  this.accessToken_ = null;
  this.widgetUrl_ = CWS_WIDGET_URL;
  this.widgetOrigin_ = CWS_WIDGET_ORIGIN;

  // For development, we provide the feature to override the URL of the widget.
  // TODO(yoshiki): Remove this before ShareDialog launches or M31 branch cut.
  this.urlOverrided_ = false;
  chrome.storage.local.get(
      ['widgetUrlOverride'],
      function(items) {
        if (items['widgetUrlOverride']) {
          this.widgetUrl_ = items['widgetUrlOverride'];
          var match = REGEXP_EXTRACT_HOST.exec(this.widgetUrl_);
          // Overriding URL must be on either localhost or .google.com.
          if (!match ||
              (!REGEXP_GOOGLE_MATCH.test(match[0]) &&
               !REGEXP_LOCALHOST_MATCH.test(match[0])))
            throw new Error('The widget URL is invalid.');
          this.widgetOrigin_ = match[0];
          this.urlOverrided_ = true;
        }
      }.bind(this));

  this.extension_ = null;
  this.mime_ = null;
  this.installingItemId_ = null;
  this.state_ = SuggestAppsDialog.State.UNINITIALIZED;

  this.initializationTask_ = new AsyncUtil.Group();
  this.initializationTask_.add(this.retrieveAuthorizeToken_.bind(this));
  this.initializationTask_.run();
}

SuggestAppsDialog.prototype = {
  __proto__: cr.ui.dialogs.BaseDialog.prototype
};

/**
 * @enum {number}
 * @const
 */
SuggestAppsDialog.State = {
  UNINITIALIZED: 0,
  INITIALIZED: 1,
  INSTALLING: 2,
  INSTALLED: 3,
  INSTALL_FAILED: 4,
};

/**
 * @override
 */
SuggestAppsDialog.prototype.onInputFocus = function() {
  this.webviewContainer_.select();
};

/**
 * Injects headers into the passed request.
 *
 * @param {Event} e Request event.
 * @return {{requestHeaders: HttpHeaders}} Modified headers.
 * @private
 */
SuggestAppsDialog.prototype.authorizeRequest_ = function(e) {
  e.requestHeaders.push({
    name: 'Authorization',
    value: 'Bearer ' + this.accessToken_
  });
  return {requestHeaders: e.requestHeaders};
};

/**
 * Retrieves the authorize token. This method should be called in
 * initialization of the dialog.
 *
 * @param {function()} callback Called when the token is retrieved.
 * @private
 */
SuggestAppsDialog.prototype.retrieveAuthorizeToken_ = function(callback) {
  // TODO(yoshiki): Share the access token with ShareDialog.
  if (this.accessToken_) {
    callback();
    return;
  }

  // Fetch or update the access token.
  chrome.fileBrowserPrivate.requestAccessToken(
      false,  // force_refresh
      function(inAccessToken) {
        this.accessToken_ = inAccessToken;
        callback();
      }.bind(this));
};

/**
 * Shows dialog.
 *
 * @param {string} extension Extension of the file.
 * @param {string} mime Mime of the file.
 * @param {function(boolean)} onDialogClosed Called when the dialog is closed.
 *     The argument is the result of installation: true if an app is installed,
 *     false otherwise.
 */
SuggestAppsDialog.prototype.show = function(extension, mime, onDialogClosed) {
  this.extension_ = extension;
  this.mimeType_ = mime;

  // Makes it sure that the initialization is completed.
  this.initializationTask_.run(function() {
    var title = str('SUGGEST_DIALOG_TITLE');

    // TODO(yoshiki): Remove this before ShareDialog launches.
    if (this.urlOverrided_)
      title += ' [OVERRIDED]';

    cr.ui.dialogs.BaseDialog.prototype.showWithTitle.apply(
        this, [title, '', function() {}, null, null]);

    this.onDialogClosed_ = onDialogClosed;

    this.webviewContainer_.innerHTML =
        '<webview id="cws-widget" partition="persist:cwswidgets"></webview>';
    var webView = this.container_.querySelector('#cws-widget');
    webView.style.width = WEBVIEW_WIDTH + 'px';
    webView.style.height = WEBVIEW_HEIGHT + 'px';
    webView.request.onBeforeSendHeaders.addListener(
        this.authorizeRequest_.bind(this),
        {urls: [this.widgetOrigin_ + '/*']},
        ['blocking', 'requestHeaders']);
    webView.focus();

    this.webviewClient_ = new CWSContainerClient(
        webView,
        extension, mime,
        WEBVIEW_WIDTH, WEBVIEW_HEIGHT,
        this.widgetUrl_, this.widgetOrigin_);
    this.webviewClient_.addEventListener('install-request',
                                         this.onInstallRequest_.bind(this));
    this.webviewClient_.load();

    this.state_ = SuggestAppsDialog.State.INITIALIZED;
  }.bind(this));
};

/**
 * Called when the 'See more...' link is clicked to be navigated to Webstore.
 * @param {Event} e Evnet.
 * @private
 */
SuggestAppsDialog.prototype.onWebstoreLinkClicked_ = function(e) {
  var webStoreUrl =
      FileTasks.createWebStoreLink(this.extension_, this.mimeType_);
  chrome.windows.create({url: webStoreUrl});
  this.hide();
};

/**
 * Called when receiving the install request from the webview client.
 * @param {Event} e Evnet.
 * @private
 */
SuggestAppsDialog.prototype.onInstallRequest_ = function(e) {
  var itemId = e.itemId;
  this.installingItemId_ = itemId;

  this.appInstaller_ = new AppInstaller(itemId);
  this.appInstaller_.install(this.onInstallCompleted_.bind(this));

  this.state_ = SuggestAppsDialog.State.INSTALLING;
};

/**
 * Called when the installation is completed from the app installer.
 * @param {AppInstaller.Result} result Result of the installation.
 * @param {string} error Detail of the error.
 * @private
 */
SuggestAppsDialog.prototype.onInstallCompleted_ = function(result, error) {
  this.state_ = (result === AppInstaller.Result.SUCCESS) ?
                SuggestAppsDialog.State.INSTALLED :
                SuggestAppsDialog.State.INSTALL_FAILED;
  var success = (result === AppInstaller.Result.SUCCESS);
  this.webviewClient_.onInstallCompleted(success, this.installingItemId_);
  this.installingItemId_ = null;

  switch (result) {
  case AppInstaller.Result.SUCCESS:
    this.hide();
    break;
  case AppInstaller.Result.CANCELLED:
    // User cancelled the installation. Do nothing.
    break;
  case AppInstaller.Result.ERROR:
    fileManager.error.show(str('SUGGEST_DIALOG_INSTALLATION_FAILED'));
    break;
  }
};

/**
 * @override
 */
SuggestAppsDialog.prototype.hide = function() {
  // Install is being aborted. Send the failure result.
  if (this.state_ == SuggestAppsDialog.State.INSTALLING) {
    if (this.webviewClient_)
      this.webviewClient_.onInstallCompleted(false, this.installingItemId_);
    this.state_ = SuggestAppsDialog.State.INSTALL_FAILED;
    this.installingItemId_ = null;
  }

  if (this.webviewClient_) {
    this.webviewClient_.dispose();
    this.webviewClient_ = null;
  }

  this.webviewContainer_.innerHTML = '';
  this.extension_ = null;
  this.mime_ = null;

  cr.ui.dialogs.BaseDialog.prototype.hide.call(this);

  // Calls the callback after the dialog hides.
  setTimeout(function() {
    var installed = this.state_ == SuggestAppsDialog.State.INSTALLED;
    this.onDialogClosed_(installed);
  }.bind(this), 0);
};
