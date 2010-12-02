// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This file contains a helper object to wrap an instance of
 * ChromeFrame for a given top-level window in Firefox.  The main functions of
 * this object are to initialize ChromeFrame and properly handle queueing of
 * messages sent to.
 *
 * @supported Firefox 3.x
 */

var CEEE_CfHelper;

(function() {

/**
 * Constructor for ChromeFrame helper object.
 * @param {!Object} instance A reference the the main ceee instance for
 *     this top-level window.
 * @constructor
 */
function CfHelper(instance) {
  // The caller must specify the ceee instance object.
  if (!instance)
    throw 'Must specify instance';

  /** A reference to the main ceee instance for this top-level window. */
  this.ceee_ = instance;

  /** Reference to the ChromeFrame <embed> element in the DOM. */
  this.cf_ = null;

  /** True when ChromeFrame is ready for use. */
  this.isReady_ = false;

  /** True if we found no enabled extensions and tried installing one. */
  this.alreadyTriedInstalling_ = false;

  /**
   * Queue of pending messages to be sent to ChromeFrame once it becomes ready.
   */
  this.queue_ = [];

  /**
   * A reference to the parent element (the toolbar) of the cf_ node.
   */
  this.parent_ = null;
}

/** Origin for use with postPrivateMessage. @const */
CfHelper.prototype.ORIGIN_EXTENSION = '__priv_xtapi';

/** Target for event notifications. @const */
CfHelper.prototype.TARGET_EVENT_REQUEST = '__priv_evtreq';

/** Target for responses to Chrome Extension API requests. @const */
CfHelper.prototype.TARGET_API_RESPONSE = '__priv_xtres';

/** Target for message port requests. @const */
CfHelper.prototype.TARGET_PORT_REQUEST = '__priv_prtreq';

/** Target for message port requests. @const */
CfHelper.prototype.TARGET_PORT_RESPONSE = '__priv_prtres';

/**
 * Preference that holds the path of the CRX file that was last installed
 * by the CEEE. @const
 */
CfHelper.prototype.LAST_INSTALL_PATH = 'last_install_path';

/**
 * Preference that holds the modified time of the CRX file that was last
 * installed by the CEEE. @const
 */
CfHelper.prototype.LAST_INSTALL_TIME = 'last_install_time';

/** @return {Object} The ChromeFrame <embed> element. */
CfHelper.prototype.get = function() {
  return this.cf_;
};

/** @return {boolean} True if ChromeFrame is ready for use. */
CfHelper.prototype.isReady = function() {
  return this.isReady_;
};

/** Post all pending messages we have queued up to ChromeFrame.
 * @private
 */
CfHelper.prototype.postPendingMessages_ = function() {
  this.ceee_.logInfo('CfHelper.postPendingMessages');

  while (this.queue_.length > 0) {
    var pair = this.queue_.shift();
    this.postMessage(pair[0], pair[1]);
  }
};

/**
 * Posts a message to ChromeFrame.
 *
 * @param {!string} message The message to be sent.
 * @param {!string} target The target of the message.
 */
CfHelper.prototype.postMessage = function(message, target) {
  if (typeof message != 'string' || typeof target != 'string') {
    this.ceee_.logError(
        'CfHelper.postMessage: message or target is not a string');
    return;
  }

  if (!this.isReady_) {
    this.ceee_.logInfo('CfHelper.postMessage defering msg=' + message);
    // Queue the message and target for later.
    this.queue_.push([message, target]);
    return;
  }

  this.ceee_.logInfo('CfHelper.postMessage msg=' + message);

  // We use private messages so that we can specify the origin.
  //
  // Messages must flow through the master CF instance.
  CEEE_globals.masterCf.get().postPrivateMessage(message,
                                                 this.ORIGIN_EXTENSION,
                                                 target);
};

/**
 * Create an <embed> element to hold the ChromeFrame and return it.
 *
 * @param {!HTMLElement} parent The DOM element that will hold the 'embed'
 *     element.
 * @param {string} idValue Value of id attribute for created element.
 * @param {!function()} onCfReady A callback for ChromeFrame becomes ready.
 * @param {!function(Object, Object)} onCfMessage A callback to handle messages
 *     posted from the ChromeFrame guest page to the ChromeFrame host page.
 * @return {HTMLElement} The <embed> element holding the ChromeFrame.
 */
CfHelper.prototype.create = function(parent, idValue, onCfReady, onCfMessage) {
  // Create the ChromeFrame instance using the correct extension path and
  // add it to the XUL DOM.
  this.parent_ = parent;
  var doc = parent.ownerDocument;
  this.cf_ = doc.createElementNS('http://www.w3.org/1999/xhtml', 'embed');
  if (!this.cf_) {
    this.ceee_.logError('Unable to create ChromeFrame');
    return null;
  }

  this.cf_.id = idValue;
  this.cf_.setAttribute('type', 'application/chromeframe');
  // TODO(cindylau@chromium.org): The dimensions should not be hardcoded here.
  // We should receive UI size info from extensions in the future, if possible.
  this.cf_.setAttribute('width', '1440');
  this.cf_.setAttribute('height', '31');
  this.cf_.setAttribute('flex', '1');
  this.cf_.setAttribute('privileged_mode', '1');
  this.cf_.setAttribute('chrome_extra_arguments',
                        '--enable-experimental-extension-apis');
  // TODO(joi@chromium.org) Remove this once the "*" default is removed from CF.
  this.cf_.setAttribute('chrome_functions_automated', '');
  this.cf_.style.visibility = 'hidden';

  // Add ChromeFrame to the DOM.  This *must* be done before setting the
  // event listeners or things will just silently fail.
  parent.appendChild(this.cf_);

  // Setup all event handlers.
  this.cf_.onprivatemessage = onCfMessage;

  // OnMessage handles messages via window.externalHost. We currently only
  // need this to match the IE workaround for windows.getCurrent.
  var originCF = this.cf_;
  this.cf_.onmessage = function(event) {
    if (!event || !event.origin || !event.data) {
      return;
    }
    if (event.data == 'ceee_getCurrentWindowId') {
      var windowId = CEEE_mozilla_windows.getWindowId(window);
      originCF.postMessage('ceee_getCurrentWindowId ' + windowId,
                           event.origin);
    }
  };

  var impl = this;
  this.cf_.addEventListener('readystatechanged',
      function() {impl.onReadyStateChange_();}, false);
  this.cf_.addEventListener('load',
      function() {impl.onCFContentLoad_(onCfReady);}, false);

  return this.cf_;
};

// HTTP request states
/**
 * HTTP request state. open() has not been called yet.
 * @const
 */
var READY_STATE_UNINITIALIZED = 0;

/**
 * HTTP request state. send() has not been called yet.
 * @const
 */
var READY_STATE_LOADING = 1;

/**
 * HTTP request state. headers and status are available.
 * @const
 */
var READY_STATE_LOADED = 2;

/**
 * HTTP request state. responseText holds partial data.
 * @const
 */
var READY_STATE_INTERACTIVE = 3;

/**
 * HTTP request state. The operation is complete.
 * @const
 */
var READY_STATE_COMPLETED = 4;

/**
 * Handle 'embed' element ready state events.
 * @private
 */
CfHelper.prototype.onReadyStateChange_ = function() {
  this.ceee_.logInfo('CfHelper.readystatechange: state=' +
      this.cf_.readystate);
  if (this.cf_.readystate == READY_STATE_UNINITIALIZED) {
    this.cf_.style.visibility = 'hidden';
  } else if (this.cf_.readystate == READY_STATE_COMPLETED) {
    this.ceee_.logInfo('CfHelper.readystatechange: window=' +
                       CEEE_mozilla_windows.getWindowId(window) +
                       ' cf=' + this.cf_.sessionid);

    // Do this before we even load the extension at the other end so
    // that extension automation is set up before any background pages
    // in the extension load.
    CEEE_globals.masterCf.onChromeFrameReady(this.cf_,
        this.ceee_.logInfo);

    // TODO(ibazarny@google.com): Not every chrome frame needs to load
    //     extensions. However, I don't know how to make it
    //     work. Should be something like:
    //if (!loadExtensions) {
    //  this.isReady_ = true;
    //  this.postPendingMessages_();
    //  return;
    //}
    var lastInstallPath =
        CEEE_globals.getCharPreference(this.LAST_INSTALL_PATH);
    var lastInstallTime =
        CEEE_globals.getCharPreference(this.LAST_INSTALL_TIME);

    // If the extension to load is a CRX file (and not a directory) and
    // its a new path or its an old path with a later modification time,
    // install it now.
    var extensionFile = this.ceee_.getCrxToInstall();
    var extensionPath = extensionFile && extensionFile.path;
    var impl = this;
    if (this.ceee_.isCrx(extensionPath) &&
        (lastInstallPath != extensionPath ||
            lastInstallTime < extensionFile.lastModifiedTime)) {
      this.ceee_.logInfo('Installing extension ' + extensionPath);
      // Install and save the path and file time.
      this.cf_.installExtension(extensionPath, function(res) {
        impl.cf_.getEnabledExtensions(function(extensionDirs) {
          impl.onGetEnabledExtensionsComplete_(extensionDirs);
        });
      });
    } else {
      // We are not installing a CRX file.  Before deciding what we should do,
      // we need to find out what extension may already be installed.
      this.cf_.getEnabledExtensions(function(extensionDirs) {
        impl.onGetEnabledExtensionsComplete_(extensionDirs);
      });
    }
  }
};

/**
 * Process response from 'getEnabledExtensions'.
 * @param {string} extensionDirs tab-separate list of extension dirs. Only first
 *     entry is handled.
 * @private
 */
CfHelper.prototype.onGetEnabledExtensionsComplete_ = function(extensionDirs) {
  this.ceee_.logInfo('OnGetEnabledExtensions: ' + extensionDirs);

  var extensions = extensionDirs.split('\t');
  var extensionFile = this.ceee_.getCrxToInstall();
  var impl = this;
  if (extensions.length > 0 && extensions[0] != '') {
    this.ceee_.logInfo('Got enabled extension: ' + extensions[0]);
    if (extensionFile) {
      CEEE_globals.setCharPreference(
          this.LAST_INSTALL_PATH, extensionFile.path);
      CEEE_globals.setCharPreference(
          this.LAST_INSTALL_TIME,
          extensionFile.lastModifiedTime.toString());
    }
    this.onLoadExtension_(extensions[0]);
  } else if (extensionFile && !this.ceee_.isCrx(extensionFile.path)) {
    this.ceee_.logInfo('Loading exploded path:' + extensionFile.path);
    this.cf_.loadExtension(extensionFile.path, function() {
      impl.onLoadExtension_(extensionFile.path);
    });
  } else if (!this.alreadyTriedInstalling_ && extensionFile) {
    this.ceee_.logInfo('Attempting to do first-time .crx install.');
    this.alreadyTriedInstalling_ = true;
    this.cf_.installExtension(extensionFile.path, function(res) {
      impl.cf_.getEnabledExtensions(function(extensionDirs) {
        impl.onGetEnabledExtensionsComplete_(extensionDirs);
      });
    });
  } else {
    // Remove the toolbar entirely. The direct parent is the toolbaritem
    // (check overlay.xul for more information), so we to remove our parentNode.
    var toolbar = this.parent_.parentNode;
    toolbar.parentNode.removeChild(toolbar);
    this.ceee_.logInfo('No extension installed.');
  }
};

/**
 * Process response from 'loadExtension' call.
 * @param {string} baseDir Extension home dir.
 * @private
 */
CfHelper.prototype.onLoadExtension_ = function (baseDir) {
  var extensions = [baseDir];
  this.ceee_.logInfo('OnLoadExtension: ' + baseDir);
  this.ceee_.initExtensions(extensions);

  var url = this.ceee_.getToolstripUrl();
  if (url) {
    // Navigate ChromeFrame to the URL we want for the toolstrip.
    // NOTE: CF expects a full URL.
    this.cf_.src = url;
    this.ceee_.logInfo('OnLoadExtension: extension URL=' + url);
  } else {
    // Remove the toolbar entirely. The direct parent is the toolbaritem
    // (check overlay.xul for more information), so we to remove our parentNode.
    var toolbar = this.parent_.parentNode;
    toolbar.parentNode.removeChild(toolbar);
    this.ceee_.logInfo('No extension URL');
  }
};

/**
 * Called once the content of the ChromeFrame is loaded and ready.
 * @param {!function()} onCfReady A callback for ChromeFrame becomes ready.
 * @private
 */
CfHelper.prototype.onCFContentLoad_ = function(onCfReady) {
  this.cf_.style.visibility = 'visible';
  this.isReady_ = true;
  this.postPendingMessages_();
  onCfReady();
};

// Make the constructor visible outside this anonymous block.
CEEE_CfHelper = CfHelper;

})();
