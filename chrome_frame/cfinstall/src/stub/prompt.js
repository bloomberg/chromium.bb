// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Implements an in-site integration of the Chrome Frame
 * installation flow by loading its implementation on demand.
 **/

goog.provide('google.cf.installer.Prompt');

goog.require('google.cf.installer.InteractionDelegate');

/**
 * Constructs a prompt flow that loads its implementation from an external
 * Javascript file.
 * @constructor
 * @param {Window=} opt_windowInstance The window in which to load the external
 *     script. Defaults to the global window object.
 */
google.cf.installer.Prompt = function(opt_windowInstance) {
  this.window_ = opt_windowInstance || window;
};

/**
 * An optional adapter to customize the display of the installation flow.
 * @type {!google.cf.installer.InteractionDelegate|undefined}
 * @private
 */
google.cf.installer.Prompt.prototype.customInteractionDelegate_ = undefined;

/**
 * An optional URL to a resource in the same domain as the calling site that
 * will be used to transport installation results back to the client.
 */
google.cf.installer.Prompt.prototype.
    manuallySpecifiedSameDomainResourceUri_ = undefined;

/**
 * Customizes the display of the prompt via the given adapter.
 * @param {!google.cf.installer.InteractionDelegate} interactionDelegate The
 *     adapter.
 */
google.cf.installer.Prompt.prototype.setInteractionDelegate =
    function(interactionDelegate) {
  this.customInteractionDelegate_ = interactionDelegate;
};

/**
 * Specifies a resource in the same domain as the calling site that will be used
 * to carry installation results across domain boundaries.
 * @param {string} uri The resource URI to use.
 */
google.cf.installer.Prompt.prototype.setSameDomainResourceUri = function(uri) {
  this.manuallySpecifiedSameDomainResourceUri_ = uri;
};

/**
 * Holds the loaded installation flow execution method. See
 * CrossDomainInstall.execute (crossdomaininstall.js).
 * @type {?function(function(), function()=, string=,
 *                  google.cf.installer.InteractionDelegate=, string=)}
 * @private
 **/
google.cf.installer.Prompt.prototype.installProc_ = null;

/**
 * @define {string} Defines the default implementation location.
 **/
google.cf.installer.Prompt.DEFAULT_IMPLEMENTATION_URL =
    '//ajax.googleapis.com/ajax/libs/chrome-frame/2/CFInstall.impl.min.js';

/**
 * Defines the URL from which the full prompting logic will be retrieved.
 * @type {string}
 * @private
 **/
google.cf.installer.Prompt.prototype.implementationUrl_ =
    google.cf.installer.Prompt.DEFAULT_IMPLEMENTATION_URL;

/**
 * Defines a custom URL for the Chrome Frame download page.
 * @type {string|undefined}
 * @private
 */
google.cf.installer.Prompt.prototype.downloadPageUrl_ = undefined;

/**
 * Overrides the default URL for loading the implementation.
 * @param {string} url The custom URL.
 */
google.cf.installer.Prompt.prototype.setImplementationUrl = function(url) {
  this.implementationUrl_ = url;
};

/**
 * Overrides the default URL for the download page.
 * @param {string} url The custom URL.
 */
google.cf.installer.Prompt.prototype.setDownloadPageUrl = function(url) {
  this.downloadPageUrl_ = url;
};

/**
 * Initiates loading of the full implementation if not already initiated.
 * @param {function()=} opt_loadCompleteHandler A callback that will be notified
 *     when the loading of the script has completed, possibly unsuccessfully.
 **/
google.cf.installer.Prompt.prototype.loadInstallProc_ =
    function(opt_loadCompleteHandler) {
  var scriptParent = this.window_.document.getElementsByTagName('head')[0] ||
      this.window_.document.documentElement;
  var scriptEl = this.window_.document.createElement('script');
  scriptEl.src = this.implementationUrl_;
  scriptEl.type = 'text/javascript';

  if (opt_loadCompleteHandler) {
    var loadHandler = goog.bind(function() {
      scriptEl.onreadystatechange = null;
      scriptEl.onerror = null;
      scriptEl.onload = null;
      loadHandler = function(){};
      opt_loadCompleteHandler();
    }, this);

    scriptEl.onload = loadHandler;
    scriptEl.onerror = loadHandler;
    scriptEl.onreadystatechange = function() {
      if (scriptEl.readyState == 'loaded')
        loadHandler();
    };
  }

  scriptParent.appendChild(scriptEl);
};

/**
 * Invokes the installation procedure with our configured parameters and
 * the provided callbacks.
 * @param {function()} successHandler The method to invoke upon installation
 *     success.
 * @param {function()} opt_failureHandler The method to invoke upon installation
 *     failure.
 */
google.cf.installer.Prompt.prototype.invokeProc_ =
    function(successHandler, opt_failureHandler) {
  this.installProc_(successHandler,
                    opt_failureHandler,
                    this.downloadPageUrl_,
                    this.customInteractionDelegate_,
                    this.manuallySpecifiedSameDomainResourceUri_);
}

/**
 * Receives a handle to the flow implementation method and invokes it.
 * @param {function(function(), function()=, string=,
 *                  google.cf.installer.InteractionDelegate=, string=)}
 *     installProc The install execution method. See
 *     CrossDomainInstall.execute().
 * @param {function()} successHandler The method to invoke upon installation
 *     success.
 * @param {function()} opt_failureHandler The method to invoke upon installation
 *     failure.
 */
google.cf.installer.Prompt.prototype.onProcLoaded_ =
    function(installProc, successHandler, opt_failureHandler) {
  this.window_['CF_google_cf_xd_install_stub'] = undefined;
  this.installProc_ = installProc;
  this.invokeProc_(successHandler, opt_failureHandler);
};

/**
 * Loads and executes the in-line Google Chrome Frame installation flow.
 * Will typically execute asynchronously (as the flow behaviour is not yet
 * loaded).
 *
 * @param {function()} successHandler A function to invoke once Google
 *     Chrome Frame is successfully installed. Overrides the default
 *     behaviour, which is to reload the current page.
 * @param {function()=} opt_failureHandler A function to invoke if the
 *     installation fails. The default behaviour is to do nothing.
 */
google.cf.installer.Prompt.prototype.open = function(successHandler,
                                                     opt_failureHandler) {
  if (this.installProc_) {
    this.invokeProc_(successHandler, opt_failureHandler);
    return;
  }

  if (this.window_['CF_google_cf_xd_install_stub']) {
    if (opt_failureHandler)
      opt_failureHandler();
    return;
  }

  this.window_['CF_google_cf_xd_install_stub'] = goog.bind(
    function(installProc) {
      this.onProcLoaded_(installProc, successHandler, opt_failureHandler);
    }, this);

  var loadCompleteHandler = undefined;
  if (opt_failureHandler) {
    loadCompleteHandler = goog.bind(function() {
      if (!this.installProc_)
        opt_failureHandler();
    }, this);
  }

  this.loadInstallProc_(loadCompleteHandler);
};
