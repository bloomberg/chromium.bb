// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Manage the installation of apps.
 *
 * @param {string} itemId Item id to be installed.
 * @constructor
 * @extends {cr.EventType}
 */
function AppInstaller(itemId) {
  this.itemId_ = itemId;
  this.callback_ = null;

  Object.seal(this);
}

AppInstaller.prototype = {
};

/**
 * Type of result.
 *
 * @enum {number}
 * @const
 */
AppInstaller.Result = {
  SUCCESS: 0,
  CANCELLED: 1,
  ERROR: 2
};
/**
 * Error message for user cancellation. This must be match with the constant
 * 'kUserCancelledError' in C/B/extensions/webstore_standalone_installer.cc.
 * @type {string}
 * @const
 * @private
 */
AppInstaller.USER_CANCELLED_ERROR_STR_ = 'User cancelled install';

/**
 * Start an installation.
 * @param {function(boolean, string)} callback Called when the installation is
 *     finished.
 */
AppInstaller.prototype.install = function(callback) {
  this.callback_ = callback;
  // TODO(yoshiki): Calls the API method to install an app.
  // Until then, the installation is always failed.
  setTimeout(this.onInstallCompleted_.bind(this, {result: false}), 1000);
};

/**
 * Called when the installation is completed.
 *
 * @param {boolean} status True if the installation is success, false if failed.
 * @private
 */
AppInstaller.prototype.onInstallCompleted_ = function(status) {
  var result = AppInstaller.Result.SUCCESS;
  if (!status.result) {
    result = (status.error == AppInstaller.USER_CANCELLED_ERROR_STR_) ?
             AppInstaller.Result.CANCELLED :
             AppInstaller.Result.ERROR;
  }
  this.callback_(result, status.error);
  this.callback_ = null;
};
