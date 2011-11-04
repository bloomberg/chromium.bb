// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Implements CFInstall.check and isAvailable in a way that is
 * compatible with previous versions. Also supports a more streamlined
 * method, 'require'.
 *
 **/

goog.provide('google.cf.installer.CFInstall');

goog.require('google.cf.ChromeFrame');
goog.require('google.cf.installer.InlineDelegate');
goog.require('google.cf.installer.Installer');
goog.require('google.cf.installer.OverlayDelegate');
goog.require('google.cf.installer.Prompt');

/**
 * Instantiates the CFInstall object. Normally there will be only one, at
 * Window.CFInstall .
 * @constructor
 */
google.cf.installer.CFInstall = function() {
  this.prompt_ = new google.cf.installer.Prompt();
  this.chromeFrame_ = new google.cf.ChromeFrame();
  this.installer_ = new google.cf.installer.Installer(
      this.prompt_, this.chromeFrame_);
  this['setDownloadPageUrl'] =
      goog.bind(this.prompt_.setDownloadPageUrl, this.prompt_);
  this['setImplementationUrl'] =
      goog.bind(this.prompt_.setImplementationUrl, this.prompt_);
  this['setSameDomainResourceUri'] =
      goog.bind(this.prompt_.setSameDomainResourceUri, this.prompt_);
  this['setInteractionDelegate'] =
      goog.bind(this.prompt_.setInteractionDelegate, this.prompt_);
  this['require'] = goog.bind(this.installer_.require, this.installer_);
  this['isAvailable'] =
      goog.bind(this.chromeFrame_.activate, this.chromeFrame_);
};

/**
 * TODO(user): This cookie is not currently set anywhere.
 * @return {boolean} Whether the user has previously declined Chrome Frame.
 * @private
 */
google.cf.installer.CFInstall.isDeclined_ = function() {
  return document.cookie.indexOf("disableGCFCheck=1") >=0;
};

/**
 * Checks to see if Chrome Frame is available, if not, prompts the user to
 * install. Once installation is begun, a background timer starts,
 * checkinging for a successful install every 2 seconds. Upon detection of
 * successful installation, the current page is reloaded, or if a
 * 'destination' parameter is passed, the page navigates there instead.
 * @param {Object} args A bag of configuration properties. Respected
 *     properties are: 'mode', 'url', 'destination', 'node', 'onmissing',
 *     'preventPrompt', 'oninstall', 'preventInstallDetection', 'cssText', and
 *     'className'.
 */
google.cf.installer.CFInstall.prototype['check'] = function(args) {
  args = args || {};

  if (!this.chromeFrame_.isPlatformSupported())
    return;

  if (this.chromeFrame_.activate())
    return;

  if (args['onmissing'])
    args['onmissing']();

  if (google.cf.installer.CFInstall.isDeclined_() || args['preventPrompt'])
    return;

  // NOTE: @slightlyoff, I'm doing away with the window.open option here. Sites
  // that were using it will now use the popup.

  // In the case of legacy installation parameters, supply a compatible
  // InteractionDelegate.
  if (args['mode'] == 'inline' || args['node'] || args['id'] ||
      args['cssText'] || args['className']) {
    if (!args['mode'] || args['mode'] == 'inline') {
      this.prompt_.setInteractionDelegate(
        new google.cf.installer.InlineDelegate(args));
    } else {
      this.prompt_.setInteractionDelegate(
        new google.cf.installer.OverlayDelegate(args));
    }
  }

  if (args['src'])
    this.prompt_.setDownloadPageUrl(args['src']);

  var onSuccess = function() {
    if (this.chromeFrame_.activate() && !args['preventInstallDetection']) {
      if (args['oninstall'])
        args['oninstall']();
      window.location.assign(args['destination'] || window.location);
    }
  };

  this.prompt_.open(onSuccess, undefined);
};

// In compiled mode, this binary is wrapped in an anonymous function which
// receives the outer scope as its only parameter. In non-compiled mode, the
// outer scope is window.

// Create a single instance of CFInstall and place it in the outer scope
// (presumably the global window object).
try {
  arguments[0]['CFInstall'] = new google.cf.installer.CFInstall();
} catch (e) {
  window['CFInstall'] = new google.cf.installer.CFInstall();
}
