// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Logic for interrogating / manipulating the local Chrome Frame
 * installation.
 *
 **/
goog.provide('google.cf.ChromeFrame');

/**
 * Constructs an object for interacting with Chrome Frame through the provided
 * Window instance.
 * @constructor
 * @param {Window=} opt_windowInstance The browser window through which to
 *     interact with Chrome Frame. Defaults to the global window object.
 **/
google.cf.ChromeFrame = function(opt_windowInstance) {
  /**
   * @private
   * @const
   * @type {!Window}
   **/
  this.window_ = opt_windowInstance || window;
};

/**
 * The name of the Chrome Frame ActiveX control.
 * @const
 * @type {string}
 **/
google.cf.ChromeFrame.CONTROL_NAME = 'ChromeTab.ChromeFrame';

/**
 * Determines whether ChromeFrame is supported on the current platform.
 * @return {boolean} Whether Chrome Frame is supported on the current platform.
 */
google.cf.ChromeFrame.prototype.isPlatformSupported = function() {
  var ua = this.window_.navigator.userAgent;
  var ieRe = /MSIE (\S+); Windows NT/;

  if (!ieRe.test(ua))
    return false;

  // We also only support Win2003/XPSP2 or better. See:
  //  http://msdn.microsoft.com/en-us/library/ms537503%28VS.85%29.aspx .
  // 'SV1' indicates SP2, only bail if not SP2 or Win2K3
  if (parseFloat(ieRe.exec(ua)[1]) < 6 && ua.indexOf('SV1') < 0)
    return false;

  return true;
};

/**
 * Determines whether Chrome Frame is the currently active renderer.
 * @return {boolean} Whether Chrome Frame is rendering the current page.
 */
google.cf.ChromeFrame.prototype.isActiveRenderer = function () {
  return this.window_.externalHost ? true : false;
};

/**
 * Determines if Google Chrome Frame is activated or locally available for
 * activation in the current browser instance. In the latter case, will trigger
 * its activation.
 * @return {boolean} True if Google Chrome Frame already was active or was
 *     locally available (and now active). False otherwise.
 */
google.cf.ChromeFrame.prototype.activate = function() {
  // Look for CF in the User Agent before trying more expensive checks
  var ua = this.window_.navigator.userAgent.toLowerCase();
  if (ua.indexOf('chromeframe') >= 0)
    return true;

  if (typeof this.window_['ActiveXObject'] != 'undefined') {
    try {
      var chromeFrame =  /** @type {ChromeTab.ChromeFrame} */(
          new this.window_.ActiveXObject(google.cf.ChromeFrame.CONTROL_NAME));
      if (chromeFrame) {
        // Register the BHO if it hasn't happened already.
        chromeFrame.registerBhoIfNeeded();
        return true;
      }
    } catch (e) {
      // Squelch
    }
  }
  return false;
};
