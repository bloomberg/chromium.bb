// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Implements a compatibility layer for older versions of
 * CFInstall.js .
 **/

goog.provide('google.cf.installer.InlineDelegate');

goog.require('goog.style');

goog.require('google.cf.installer.InteractionDelegate');
goog.require('google.cf.installer.frame');

/**
 * Adds an unadorned iframe into the page, taking arguments to customize it.
 * @param {Object} args A map of user-specified properties to set
 * @constructor
 * @implements {google.cf.installer.InteractionDelegate}
 */
google.cf.installer.InlineDelegate = function(args) {
  this.args_ = args;
  this.args_.className = 'chromeFrameInstallDefaultStyle ' +
                         (this.args_.className || '');
};

/**
 * Indicates whether the overlay CSS has already been injected.
 * @type {boolean}
 * @private
 */
google.cf.installer.InlineDelegate.styleInjected_ = false;

/**
 * Generates the CSS for the overlay.
 * @private
 */
google.cf.installer.InlineDelegate.injectCss_ = function() {
  if (google.cf.installer.InlineDelegate.styleInjected_)
    return;

  goog.style.installStyles('.chromeFrameInstallDefaultStyle {' +
                             'width: 800px;' +
                             'height: 600px;' +
                             'position: absolute;' +
                             'left: 50%;' +
                             'top: 50%;' +
                             'margin-left: -400px;' +
                             'margin-top: -300px;' +
                           '}');
  google.cf.installer.InlineDelegate.styleInjected_ = true;
};

/**
 * @inheritDoc
 */
google.cf.installer.InlineDelegate.prototype.getIFrameContainer = function() {
  // TODO(user): This will append to the end of the container, whereas
  // prior behaviour was beginning...
  google.cf.installer.InlineDelegate.injectCss_();
  return google.cf.installer.frame.getParentNode(this.args_);
};

/**
 * @inheritDoc
 */
google.cf.installer.InlineDelegate.prototype.customizeIFrame =
    function(iframe) {
  google.cf.installer.frame.setProperties(iframe, this.args_);
};

/**
 * @inheritDoc
 */
google.cf.installer.InlineDelegate.prototype.show = function() {
  // TODO(user): Should start it out hidden and reveal/resize here.
};

/**
 * @inheritDoc
 */
google.cf.installer.InlineDelegate.prototype.reset =
    function() {
  // TODO(user): Should hide it here.
};
