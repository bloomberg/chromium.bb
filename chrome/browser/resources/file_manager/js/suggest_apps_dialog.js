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
var WEBVIEW_WIDTH = 735;
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
 * @extends {FileManagerDialogBase}
 */
function SuggestAppsDialog(parentNode) {
  FileManagerDialogBase.call(this, parentNode);

  this.frame_.id = 'suggest-app-dialog';

  this.spinner_ = this.document_.createElement('div');
  this.spinner_.className = 'spinner';

  this.spinnerWrapper_ = this.document_.createElement('div');
  this.spinnerWrapper_.className = 'spinner-container';
  this.spinnerWrapper_.style.width = WEBVIEW_WIDTH + 'px';
  this.spinnerWrapper_.style.height = WEBVIEW_HEIGHT + 'px';
  this.spinnerWrapper_.appendChild(this.spinner_);
  this.frame_.insertBefore(this.spinnerWrapper_, this.text_.nextSibling);

  this.webviewContainer_ = this.document_.createElement('div');
  this.webviewContainer_.id = 'webview-container';
  this.webviewContainer_.style.width = WEBVIEW_WIDTH + 'px';
  this.webviewContainer_.style.height = WEBVIEW_HEIGHT + 'px';
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

  this.webview_ = null;
  this.accessToken_ = null;
  this.widgetUrl_ = CWS_WIDGET_URL;
  this.widgetOrigin_ = CWS_WIDGET_ORIGIN;

  this.extension_ = null;
  this.mime_ = null;
  this.installingItemId_ = null;
  this.state_ = SuggestAppsDialog.State.UNINITIALIZED;

  this.initializationTask_ = new AsyncUtil.Group();
  this.initializationTask_.add(this.retrieveAuthorizeToken_.bind(this));
  this.initializationTask_.run();
}

SuggestAppsDialog.prototype = {
  __proto__: FileManagerDialogBase.prototype
};

/**
 * @enum {string}
 * @const
 */
SuggestAppsDialog.State = {
  UNINITIALIZED: 'SuggestAppsDialog.State.UNINITIALIZED',
  INITIALIZING: 'SuggestAppsDialog.State.INITIALIZING',
  INITIALIZE_FAILED_CLOSING:
      'SuggestAppsDialog.State.INITIALIZE_FAILED_CLOSING',
  INITIALIZED: 'SuggestAppsDialog.State.INITIALIZED',
  INSTALLING: 'SuggestAppsDialog.State.INSTALLING',
  INSTALLED_CLOSING: 'SuggestAppsDialog.State.INSTALLED_CLOSING',
  CANCELED_CLOSING: 'SuggestAppsDialog.State.CANCELED_CLOSING'
};
Object.freeze(SuggestAppsDialog.State);

/**
 * @enum {string}
 * @const
 */
SuggestAppsDialog.Result = {
  // Install is done. The install app should be opened.
  INSTALL_SUCCESSFUL: 'SuggestAppsDialog.Result.INSTALL_SUCCESSFUL',
  // User cancelled the suggest app dialog. No message should be shown.
  USER_CANCELL: 'SuggestAppsDialog.Result.USER_CANCELL',
  // Failed to load the widget. Error message should be shown.
  FAILED: 'SuggestAppsDialog.Result.FAILED'
};
Object.freeze(SuggestAppsDialog.Result);

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
  if (this.accessToken_) {
    callback();
    return;
  }

  // Fetch or update the access token.
  chrome.fileBrowserPrivate.requestWebStoreAccessToken(
      function(accessToken) {
        // In case of error, this.accessToken_ will be set to null.
        this.accessToken_ = accessToken;
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
  if (this.state_ != SuggestAppsDialog.State.UNINITIALIZED) {
    console.error('Invalid state.');
    return;
  }

  this.extension_ = extension;
  this.mimeType_ = mime;
  this.onDialogClosed_ = onDialogClosed;
  this.state_ = SuggestAppsDialog.State.INITIALIZING;

  // Makes it sure that the initialization is completed.
  this.initializationTask_.run(function() {
    if (!this.accessToken_) {
      this.state_ = SuggestAppsDialog.State.INITIALIZE_FAILED_CLOSING;
      this.onHide_();
      return;
    }

    var title = str('SUGGEST_DIALOG_TITLE');

    var show =
        FileManagerDialogBase.prototype.showTitleOnlyDialog.call(this, title);
    if (!show) {
      console.error('SuggestAppsDialog can\'t be shown');
      this.state_ = SuggestAppsDialog.State.UNINITIALIZED;
      this.onHide();
      return;
    }

    this.webviewContainer_.innerHTML =
        '<webview id="cws-widget" partition="persist:cwswidgets"></webview>';

    this.webview_ = this.container_.querySelector('#cws-widget');
    this.webview_.style.width = WEBVIEW_WIDTH + 'px';
    this.webview_.style.height = WEBVIEW_HEIGHT + 'px';
    this.webview_.request.onBeforeSendHeaders.addListener(
        this.authorizeRequest_.bind(this),
        {urls: [this.widgetOrigin_ + '/*']},
        ['blocking', 'requestHeaders']);
    this.webview_.addEventListener('newwindow', function(event) {
      // Discard the window object and reopen in an external window.
      event.window.discard();
      util.visitURL(event.targetUrl);
      event.preventDefault();
    });

    this.frame_.classList.add('show-spinner');

    this.webviewClient_ = new CWSContainerClient(
        this.webview_,
        extension, mime,
        WEBVIEW_WIDTH, WEBVIEW_HEIGHT,
        this.widgetUrl_, this.widgetOrigin_);
    this.webviewClient_.addEventListener(CWSContainerClient.Events.LOADED,
                                         this.onWidgetLoaded_.bind(this));
    this.webviewClient_.addEventListener(CWSContainerClient.Events.LOAD_FAILED,
                                         this.onWidgetLoadFailed_.bind(this));
    this.webviewClient_.addEventListener(
        CWSContainerClient.Events.REQUEST_INSTALL,
        this.onInstallRequest_.bind(this));
    this.webviewClient_.load();
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
 * Called when the widget is loaded successfuly.
 * @param {Event} event Evnet.
 * @private
 */
SuggestAppsDialog.prototype.onWidgetLoaded_ = function(event) {
  this.frame_.classList.remove('show-spinner');
  this.state_ = SuggestAppsDialog.State.INITIALIZED;

  this.webview_.focus();
};

/**
 * Called when the widget is failed to load.
 * @param {Event} event Evnet.
 * @private
 */
SuggestAppsDialog.prototype.onWidgetLoadFailed_ = function(event) {
  this.frame_.classList.remove('show-spinner');
  this.state_ = SuggestAppsDialog.State.INITIALIZE_FAILED_CLOSING;

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

  this.frame_.classList.add('show-spinner');
  this.state_ = SuggestAppsDialog.State.INSTALLING;
};

/**
 * Called when the installation is completed from the app installer.
 * @param {AppInstaller.Result} result Result of the installation.
 * @param {string} error Detail of the error.
 * @private
 */
SuggestAppsDialog.prototype.onInstallCompleted_ = function(result, error) {
  var success = (result === AppInstaller.Result.SUCCESS);

  this.frame_.classList.remove('show-spinner');
  this.state_ = success ?
                SuggestAppsDialog.State.INSTALLED_CLOSING :
                SuggestAppsDialog.State.INITIALIZED;  // Back to normal state.
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
SuggestAppsDialog.prototype.hide = function(opt_originalOnHide) {
  switch (this.state_) {
    case SuggestAppsDialog.State.INSTALLING:
      // Install is being aborted. Send the failure result.
      // Cancels the install.
      if (this.webviewClient_)
        this.webviewClient_.onInstallCompleted(false, this.installingItemId_);
      this.installingItemId_ = null;

      // Assumes closing the dialog as canceling the install.
      this.state_ = SuggestAppsDialog.State.CANCELED_CLOSING;
      break;
    case SuggestAppsDialog.State.INSTALLED_CLOSING:
    case SuggestAppsDialog.State.INITIALIZE_FAILED_CLOSING:
      // Do nothing.
      break;
    case SuggestAppsDialog.State.INITIALIZED:
      this.state_ = SuggestAppsDialog.State.CANCELED_CLOSING;
      break;
    default:
      this.state_ = SuggestAppsDialog.State.CANCELED_CLOSING;
      console.error('Invalid state.');
  }

  if (this.webviewClient_) {
    this.webviewClient_.dispose();
    this.webviewClient_ = null;
  }

  this.webviewContainer_.innerHTML = '';
  this.extension_ = null;
  this.mime_ = null;

  FileManagerDialogBase.prototype.hide.call(
      this,
      this.onHide_.bind(this, opt_originalOnHide));
};

/**
 * @param {function()=} opt_originalOnHide Original onHide function passed to
 *     SuggestAppsDialog.hide().
 * @private
 */
SuggestAppsDialog.prototype.onHide_ = function(opt_originalOnHide) {
  // Calls the callback after the dialog hides.
  if (opt_originalOnHide)
    opt_originalOnHide();

  var result;
  switch (this.state_) {
    case SuggestAppsDialog.State.INSTALLED_CLOSING:
      result = SuggestAppsDialog.Result.INSTALL_SUCCESSFUL;
      break;
    case SuggestAppsDialog.State.INITIALIZE_FAILED_CLOSING:
      result = SuggestAppsDialog.Result.FAILED;
      break;
    case SuggestAppsDialog.State.CANCELED_CLOSING:
      result = SuggestAppsDialog.Result.USER_CANCELL;
      break;
    default:
      result = SuggestAppsDialog.Result.USER_CANCELL;
      console.error('Invalid state.');
  }
  this.state_ = SuggestAppsDialog.State.UNINITIALIZED;

  this.onDialogClosed_(result);
};
