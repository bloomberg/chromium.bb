// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Implements the in-line Google Chrome Frame installation flow.
 * Displays a dialog containing the download page. Upon completion, registers
 * the Google Chrome Frame components in the current browser process.
 *
 **/
goog.provide('google.cf.installer.CrossDomainInstall');

goog.require('goog.net.xpc.CrossPageChannel');
goog.require('goog.style');
goog.require('goog.Uri');

goog.require('google.cf.installer.InteractionDelegate');
goog.require('google.cf.installer.DialogInteractionDelegate');

/**
 * @type {Object}
 **/
google.cf.installer.CrossDomainInstall = {};

/**
 * @define {string} Defines the default download page URL.
 **/
google.cf.installer.CrossDomainInstall.DEFAULT_DOWNLOAD_PAGE_URL =
    '//www.google.com/chromeframe/eula.html';

/**
 * Executes the in-line installation flow.
 * @param {function()} successHandler Invoked upon installation success. When
 *     invoked, Google Chrome Frame will be active in the current and all
 *     future-launched browser processes.
 * @param {function()=} opt_failureHandler Invoked upon installation failure or
 *     cancellation.
 * @param {string=} opt_url An alternative URL for the download page.
 * @param {google.cf.installer.InteractionDelegate=} opt_interactionDelegate An
 *     alternative UI implementation for the modal dialog.
 * @param {string=} opt_dummyResourceUri A manually-specified dummy resource URI
 *     that will be used to carry cross-domain responses.
 */
google.cf.installer.CrossDomainInstall.execute = function(
    successHandler, opt_failureHandler, opt_url, opt_interactionDelegate,
    opt_dummyResourceUri) {
  var url = new goog.Uri(
      opt_url ||
      google.cf.installer.CrossDomainInstall.DEFAULT_DOWNLOAD_PAGE_URL);

  if (!url.hasScheme())
    url = new goog.Uri(window.location.href).resolve(url);

  var interactionDelegate = opt_interactionDelegate ||
      new google.cf.installer.DialogInteractionDelegate();

  var cfg = {};

  // TODO(user): Probably need to import some of the link/image url
  // detection stuff from XDRPC.
  if (opt_dummyResourceUri) {
    var dummyUrl = new goog.Uri(opt_dummyResourceUri);
    if (!dummyUrl.hasScheme())
      dummyUrl = new goog.Uri(window.location.href).resolve(dummyUrl);

    cfg[goog.net.xpc.CfgFields.LOCAL_POLL_URI] = dummyUrl.toString();
  }

  cfg[goog.net.xpc.CfgFields.PEER_URI] = url.toString();

  var channel = new goog.net.xpc.CrossPageChannel(cfg);
  var iframe = channel.createPeerIframe(
      interactionDelegate.getIFrameContainer(),
      function(newIFrame) {
        newIFrame.setAttribute('frameBorder', '0');
        newIFrame.setAttribute('border', '0');
        interactionDelegate.customizeIFrame(newIFrame);
      });
  channel.registerService('dimensions', function(size) {
    goog.style.setContentBoxSize(iframe, new goog.math.Size(size['width'],
                                                            size['height']));
    interactionDelegate.show();
  }, true);  // true => deserialize messages into objects
  channel.registerService('result', function(obj) {
    channel.close();
    interactionDelegate.reset();
    var result = obj['result'];
    if (result)
      successHandler();
    else if (opt_failureHandler)
      opt_failureHandler();
  }, true);  // true => deserialize messages into objects
  // TODO(user): Perhaps listen to onload and set a timeout for connect.
  channel.connect();
};

// In compiled mode, this binary is wrapped in an anonymous function which
// receives the outer scope as its only parameter. In non-compiled mode, the
// outer scope is window.
// Look in the outer scope for the stub, and pass it the implementation.
try {
  if (arguments[0]['CF_google_cf_xd_install_stub']) {
    arguments[0]['CF_google_cf_xd_install_stub'](
        google.cf.installer.CrossDomainInstall.execute);
  }
} catch (e) {
  if (window['CF_google_cf_xd_install_stub']) {
    window['CF_google_cf_xd_install_stub'](
        google.cf.installer.CrossDomainInstall.execute);
  }
}
