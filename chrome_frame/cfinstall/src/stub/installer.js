// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 **/

goog.provide('google.cf.installer.Installer');

goog.require('google.cf.ChromeFrame');
goog.require('google.cf.installer.Prompt');

/**
 * @constructor
 */
google.cf.installer.Installer = function(prompt, chromeFrame) {
  this.prompt_ = prompt;
  this.chromeFrame_ = chromeFrame;
};

google.cf.installer.Installer.prototype.require =
    function(opt_onInstall, opt_onFailure) {
  if (this.chromeFrame_.isActiveRenderer())
    return;

  if (!this.chromeFrame_.isPlatformSupported()) {
    if (opt_onFailure)
      opt_onFailure();
    return;
  }

  var successHandler = opt_onInstall || function() {
    window.location.assign(window.location.href);
  };

  if (this.chromeFrame_.activate()) {
    successHandler();
    return;
  }

  this.prompt_.open(successHandler, opt_onFailure);
};
