// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This file contains the main code for the CEEE Firefox
 * add-on.  This add-on mostly implements the Google Chrome extension APIs in
 * Firefox.
 *
 * The CEEE extension is loaded into a ChromeFrame object (see the
 * html:embed element in overlay.xul) and catches Chrome extension API requests
 * using the ChromeFrame postMessage support.
 *
 * This add-on runs with privilege in Firefox, but the extension running inside
 * the ChromeFrame is considered 'unsafe' code.  Precautions are taken in the
 * code to make sure that unsafe code is never executed with the privilege
 * of the extension.  In practice though, the extension running inside Chrome
 * Frame is trusted code since there are mechanisms outside of this file that
 * will ensure this.
 *
 * @supported Firefox 3.x
 */

// Commenting out anonymous function wrapper for this whole file to allow for
// unit testing this code.
// (function(){

/**
 * Imports the symbol CEEE_globals into this scope.
 */
Components.utils['import']('resource://modules/global.js', window);
if (!CEEE_globals)
  Components.utils.reportError('CEEE: *** globals module not imported');
if (!CEEE_mozilla_windows)
  Components.utils.reportError('CEEE: *** mozilla windows module not imported');
if (!CEEE_mozilla_tabs)
  Components.utils.reportError('CEEE: *** mozilla tabs module not imported');

// Log from where we are running.
var ffCeeeLoc = Components.classes['@mozilla.org/extensions/manager;1']
    .getService(Components.interfaces.nsIExtensionManager)
    .getInstallLocation(CEEE_globals.ADDON_ID)
    .getItemLocation(CEEE_globals.ADDON_ID);
Application.console.log('CEEE: running from ' + ffCeeeLoc.path);

/**
 * Constructor for the object that implements the CEEE API.
 * @constructor
 */
function CEEE_Class() {
  /** @const */ this.TAB_STATUS_LOADING = 'loading';
  /** @const */ this.TAB_STATUS_COMPLETE = 'complete';

  // Internal constants used by implementation.
  /** @const */ this.API_EVENT_NAME_ = 'ceee-dom-api';
  /** @const */ this.API_ELEMENT_NAME_ = 'ceee-api-element';
  /** @const */ this.DATA_ATTRIBUTE_NAME_ = 'ceee-event-data';
  /** @const */ this.RETURN_ATTRIBUTE_NAME_ = 'ceee-event-return';
  /** @const */ this.ORIGIN_EXTENSION_ = '__priv_xtapi';

  /**
   * When true, this allows content scripts to be debugged using Firebug.
   */
  this.contentScriptDebugging = CEEE_globals.getBoolPreference('debug');

  /**
   * The dispatch table for the UI/user context API.
   * @type {Object}
   * @private
   */
  this.dispatch_ = {};

  /**
   * The dispatch table for the DOM context API.
   * @type {Object}
   * @private
   */
  this.dom_dispatch_ = {};

  /**
   * Reference to the API module implementing the CEEE windows API.
   * @type {Object}
   * @private
   */
  this.windowModule_ = null;

  /**
   * Reference to the API module implementing the CEEE tabs API.
   * @type {Object}
   * @private
   */
  this.tabsModule_ = null;

  /**
   * Reference to the API module implementing the CEEE cookies API.
   * @type {Object}
   * @private
   */
  this.cookiesModule_ = null;

  /**
   * Reference to the API module implementing the CEEE sidebar API.
   * @type {Object}
   * @private
   */
  this.sidebarModule_ = null;

  /**
   * Reference to the API module implementing the CEEE user scripts API.
   * @type {Object}
   * @private
   */
  this.userscriptsModule_ = null;

  /**
   * Array of functions to run once the extensions have been fully
   * initialized.  Each element of the array is itself an array with three
   * elements: a content window, a string description, and the function to call.
   * @type {Array.<{win: Object, desc: string, func: function()}>}
   * @private
   */
  this.runWhenExtensionsInited_ = [];
};

/**
 * Log an informational message to the Firefox error console.
 * @public
 */
CEEE_Class.prototype.logInfo = function(msg) {
  dump('[CEEE] ' + msg + '\n');
  Application.console.log('CEEE: ' + msg);
};

/**
 * Log an error message to the Firefox error console.
 * @public
 */
CEEE_Class.prototype.logError = function(msg) {
  var functionName = CEEE_Class.prototype.logError.caller.name + ':: ';
  dump('[CEEE] *** ' + functionName + msg + '\n');
  Components.utils.reportError('CEEE: *** ' + functionName + msg);
};

/**
 * Called once the CEEE host document is fully loaded in a top-level
 * firefox window.  This will be called for each top-level window.
 * @private
 */
CEEE_Class.prototype.onLoad_ = function() {
  // Initialize all of the imported API modules.
  this.windowModule_ = CEEE_initialize_windows(this);
  this.tabsModule_ = CEEE_initialize_tabs(this);
  this.cookiesModule_ = CEEE_initialize_cookies(this);

  // TODO(twiz@chromium.org) : Re-enable bookmarks support once the
  // Chrome Frame message passing system is more stable when a pop-up
  // is present. bb2279154.
  //CEEE_initialize_bookmarks(this);

  this.userscriptsModule_ = new CEEE_UserScriptManager(this);
  this.infobarsModule_ = CEEE_initialize_infobars(this,
      (/**@type {!Object}*/this.windowModule_));

  this.createChromeFrame_();

  // NOTE: this function must be called after createChromeFrame_() in order to
  // make sure that the chrome process started uses the correct command line
  // arguments.
  this.sidebarModule_ = CEEE_initialize_sidebar(this);

  // TODO(joi@chromium.org) We used to have functionality here to show
  // an error in case no extension was specified; this needs to be
  // done by putting some HTML into CF now.

  // Make a list of all registered functions - those are the ones we
  // wish to overtake in case our CF becomes the new "master" CF, the others
  // we will leave with Chrome.
  var functions = [];
  for (var key in this.dispatch_) {
    functions.push(key);
  }
  var csFunctions = functions.join(',');
  CEEE_globals.masterCf.init(csFunctions, this.logInfo);

  var impl = this;

  window.addEventListener('focus', function() {impl.onFocus_();},
                          false);
  window.addEventListener('unload', function() {impl.onUnload_();},
                          false);

  // TODO(rogerta@chromium.org): after setting the src property of
  // ChromeFrame, it was asked in a code review whether the "load"
  // event could race with the javascript below which sets the
  // onCfReady_ function, or any functions that it calls directly or
  // indirectly.  For example, is it possible for ChromeFrame to fire
  // the "load" event before userscripts_api.js runs, which sets the
  // openPendingChannels property?
  //
  // From my experience with Firefox 3.0.x, this has not happened.  The xul
  // file loads the javascript files in the order specified, and
  // userscripts_api.js always loads after this file.  If this does end up
  // being a problem, the solution would be create a new javascript file that
  // is called *after* all the other javascript files that set all properties,
  // that simply adds onLoad_ as a listener to the chrome window (i.e. the
  // last executable line in this file).

  // Listen for DOMContentLoaded in order to hook the windows when needed.
  // No need to listen for DOMFrameContentLoaded, because this is generated
  // for the <iframe> element in the parent document.  We still get
  // DOMContentLoaded for the actual document object even when a frame is
  // embedded.
  var ac = document.getElementById('appcontent');
  ac.addEventListener('DOMContentLoaded', function (evt) {
    impl.onDomContentLoaded_(evt.target);
  }, false);

  // Listen for tab events.
  var mainBrowser = document.getElementById(CEEE_globals.MAIN_BROWSER_ID);
  mainBrowser.addEventListener('load', function(evt) {
    impl.onContentLoaded_(evt.target);
  }, true);

  // For each tab already created, make to inject all required user scripts
  // once the extension get loaded.
  var tabCount = mainBrowser.tabContainer.itemCount;
  for (var i = 0; i < tabCount; ++i) {
    var doc = mainBrowser.getBrowserAtIndex(i).contentDocument;
    impl.onDomContentLoaded_(doc);
    impl.onContentLoaded_(doc);
  }

  // Notify the tabs module that the page has loaded.
  this.tabsModule_.onLoad();

  this.cookiesModule_.onLoad();
  this.sidebarModule_.onLoad();

  this.windowModule_.onWindowOpened_(window);
  this.logInfo('onLoad_: done');
};

/**
 * Adds a function to a collection of functions to execute once extensions
 * have been initialized.
 * @param w {Object} Content window to run in.
 * @param desc {string} Description of function.
 * @param f {function()} Function to defer.
 */
CEEE_Class.prototype.runWhenExtensionsInited = function(w, desc, f) {
  this.logInfo('Deferring "' + desc + '" for window=' + w.location.href);
  this.runWhenExtensionsInited_.push({win: w, desc: desc, func: f});
};

/**
 * Called once we know which extensions are loaded.
 * @param extensions {Array.<string>} Base directories (paths, not
 * File objects) of extensions loaded.
 */
CEEE_Class.prototype.initExtensions = function(extensions) {
  this.logInfo('initExtensions starting');
  // Initialize only once.
  if (!this.toolstripUrl_ && extensions.length > 0) {
    var baseDir = Components.classes['@mozilla.org/file/local;1']
        .getService(Components.interfaces.nsILocalFile);
    baseDir.initWithPath(extensions[0]);

    // Will set attributes read from the manifest file.
    // TODO(joi@chromium.org) Generalize to multiple extensions.
    this.loadToolstripManifest(baseDir);
    if (!this.toolstripUrl_) {
      this.logInfo('Apparently, no extensions are loaded.');
    } else {
      this.toolstripDir_ = baseDir;

      for (var i = 0; i < this.runWhenExtensionsInited_.length; ++i) {
        var obj = this.runWhenExtensionsInited_[i];
        var w = obj.win;
        var windowName = (w && w.location && w.location.href) || '?';
        this.logInfo('Running "' + obj.desc + '" in window=' + windowName);
        obj.func();
      }
      this.runWhenExtensionsInited_ = [];
    }
  }
  this.logInfo('initExtensions done');
};

/**
 * Create the ChromeFrame element that goes into the toolstrip.
 * @private
 */
CEEE_Class.prototype.createChromeFrame_ = function() {
  var impl = this;
  var onCfReady = function() {impl.onCfReady_();};
  var onCfMessage = function(evt, target) {impl.onCfMessage_(evt, target);};

  this.cfHelper_ = new CEEE_CfHelper(this);
  var parent = document.getElementById('ceee-browser-item');
  var cf = this.cfHelper_.create(parent, CEEE_globals.CHROME_FRAME_ID,
                                 onCfReady, onCfMessage);
};

/** Returns a helper object for working with ChromeFrame. */
CEEE_Class.prototype.getCfHelper = function() {
  return this.cfHelper_;
};

/** Returns a helper object for working with windows. */
CEEE_Class.prototype.getWindowModule = function() {
  return this.windowModule_;
};

/** Returns a helper object for working with tabs. */
CEEE_Class.prototype.getTabModule = function() {
  return this.tabsModule_;
};

/** Returns a helper object for working with cookies. */
CEEE_Class.prototype.getCookieModule = function() {
  return this.cookiesModule_;
};

/** Returns a helper object for working with sidebar. */
CEEE_Class.prototype.getSidebarModule = function() {
  return this.sidebarModule_;
};

/** Returns a helper object for working with user scripts. */
CEEE_Class.prototype.getUserScriptsModule = function() {
  return this.userscriptsModule_;
};

/**
 * Called when the CEEE host window gets focus.
 * @private
 */
CEEE_Class.prototype.onFocus_ = function() {
  this.windowModule_.onWindowFocused_(window);
};

/**
 * Called when the CEEE host document is unloaded.  This will be called for
 * each top-level window.
 * @private
 */
CEEE_Class.prototype.onUnload_ = function() {
  CEEE_globals.masterCf.onChromeFrameGoing(this.cfHelper_.get(),
                                           this.logInfo);
  this.windowModule_.onWindowRemoved_(window);
  this.cookiesModule_.onUnload();
  this.sidebarModule_.onUnload();
  this.logInfo('onUnload_: done');
};

/**
 * Called when the page inside ChromeFrame is loaded and ready.  Before this
 * call, we cannot assume that ChromeFrame is ready to accept any postMessage
 * calls.
 * @private
 */
CEEE_Class.prototype.onCfReady_ = function() {
  this.logInfo('onCfReady_: done');
};

/**
 * Called when the main html content of a document is loaded, but before any
 * referenced resources, like images and so on, are loaded.  This corresponds
 * to the DOMContentLoaded event in Firefox.
 *
 * @param doc The document of the page that fired the event.
 * @private
 */
CEEE_Class.prototype.onDomContentLoaded_ = function(doc) {
  var impl = this;

  // The target of the event is the document being loaded.  The defaultView
  // points to the window of this document.  Remember that this window has
  // been wrapped to make it safe.
  var w = doc.defaultView;
  if (w) {
    w.addEventListener(this.API_EVENT_NAME_, function(evt2) {
      impl.onDomApiCall_(w, evt2);
    }, false);

    // Attach a handler for onHashChange so that we can fire tab status changes
    // when that particular event occurs.
    w.addEventListener('hashchange', function() {
      impl.onHashChange_(doc);
    }, false);

    // TODO(rogerta@chromium.org): I hook unload in order to perform
    // cleanup for this content window, specifically to cleanup data
    // for the ports.  However, chrome extensions allow user scripts
    // to post messages during unload, and this cleanup causes those
    // messages to be lost.  I need to find another event that
    // guarantees to fire after all unload listeners to perform the
    // cleanup.
    w.addEventListener('unload', function() {
      impl.userscriptsModule_.onContentUnloaded(w);
      impl.logInfo('onContentUnloaded_: done');
    }, false);

    if (this.toolstripDir_) {
      // Run all user scripts that match this page.
      this.userscriptsModule_.runUserScripts(w);
    } else {
      this.runWhenExtensionsInited(w, 'runUserScripts', function() {
        impl.userscriptsModule_.runUserScripts(w);
      });
    }

    // TODO(rogerta@chromium.org): It would be cool to be able detect
    // if the content is for an embedded frame, but I have not figured
    // out how to do that.  It seems that in some cases we get
    // DOMContentLoaded even for embedded frames.  Not a big deal
    // since onTabStatusChanged_() handles it correctly.
    this.tabsModule_.onTabStatusChanged(doc, impl.TAB_STATUS_LOADING);
  } else {
    this.logError('onDomContentLoaded_: no default view for document');
  }

  this.logInfo('onDomContentLoaded_: done loc=' + doc.location);
};

/**
 * Called when the main html content of a document *and* all its referenced
 * resources, like images and so on, are loaded.  This corresponds
 * to the "onload" event in Firefox, and always happens after the
 * DOMContentLoaded event.
 *
 * @param doc The document of the page that fired the event.
 * @private
 */
CEEE_Class.prototype.onContentLoaded_ = function(doc) {
  // This event is also fired for images, but we don't want those.  Filter out
  // the document events as described in
  // https://developer.mozilla.org/en/Code_snippets/On_page_load.
  if (doc.nodeName == '#document') {
    // We only want to generate tab notifications for top-level documents,
    // not internal frames.
    if (doc.defaultView.frameElement) {
      this.logInfo('onContentLoaded_(frame): done loc=' + doc.location);
    } else {
      this.tabsModule_.onTabStatusChanged(doc, this.TAB_STATUS_COMPLETE);
      this.logInfo('onContentLoaded_: done loc=' + doc.location);
    }
  }
};

/**
 * Called when a hash change occurs in a given document.
 *
 * @param doc The document of the page that fired the event.
 */
CEEE_Class.prototype.onHashChange_ = function(doc) {
  this.tabsModule_.onTabStatusChanged(doc, this.TAB_STATUS_LOADING);
  this.tabsModule_.onTabStatusChanged(doc, this.TAB_STATUS_COMPLETE);
};

/**
 * @param path {string} A filesystem path or the empty string.
 * @return True if the specified path is a .crx (or empty).
 */
CEEE_Class.prototype.isCrx = function(path) {
  return (path == '' || /\.crx$/.test(path));
}

/**
 * Get the path of the .crx file to install.
 *
 * @return A File object that represents a .crx file to install with Chrome.
 */
CEEE_Class.prototype.getCrxToInstall = function() {
  if (!this.crxToInstall_) {
    // See if the user is overriding the location of the .crx file.
    var crx;
    var path = CEEE_globals.getCharPreference('path');
    if (path) {
      this.logInfo('getCrxToInstall: path preference exists, try it');
      crx = Components.classes['@mozilla.org/file/local;1']
          .getService(Components.interfaces.nsILocalFile);
      try {
        crx.initWithPath(path);

        // Make sure the file we read is valid, so that otherwise we
        // can use the first file found in the default directory.  We
        // allow paths from preferences to be paths to either a .crx
        // file or an exploded directory, but only search for .crx
        // files when scanning the directory below.
        if (!crx.exists() || (this.isCrx(path) && crx.isDirectory()) ||
            (!this.isCrx(path) && !crx.isDirectory())) {
          this.logError('getCrxToInstall: "' + path +
                        '" does not exist or is of the wrong type');
          crx = null;
        }
      } catch(e) {
        this.logError('getCrxToInstall: ' +
                      'extensions.ceee.path must be a valid absolute path');
        crx = null;
      }
    }

    // The user did not override the path to an extension to install.
    // Get the default from a well-known directory.
    if (!crx) {
      var env = Components.classes['@mozilla.org/process/environment;1'];
      var userEnv = env.getService(Components.interfaces['nsIEnvironment']);
      var PFILES32 = 'ProgramFiles(x86)';
      var PFILES = 'ProgramFiles';
      var sysPfiles = (userEnv.exists(PFILES32) && userEnv.get(PFILES32)) ||
                      (userEnv.exists(PFILES) && userEnv.get(PFILES)) ||
                      'C:\Program Files';
      path = sysPfiles + '\\Google\\CEEE\\Extensions';
      this.logInfo('getCrxToInstall: trying: ' + path);

      var dir = Components.classes['@mozilla.org/file/local;1']
          .getService(Components.interfaces.nsILocalFile);
      dir.initWithPath(path);

      // Find the first file with a .crx extension.  Failing that, find the
      // first directory.
      // TODO(rogerta@chromium.org): eventually we want to support
      // loading any number of extensions.
      if (dir && dir.exists() && dir.isDirectory()) {
        var e = dir.directoryEntries;
        while (e.hasMoreElements()) {
          var temp = e.getNext().QueryInterface(Components.interfaces.nsIFile);
          if (temp && !temp.isDirectory() && /\.crx$/.test(temp.path)) {
            crx = temp;
            break;
          }
        }

        // Fall back to using the first directory.
        if (!crx) {
          e = dir.directoryEntries;
          while (e.hasMoreElements()) {
            var temp = e.getNext().QueryInterface(
                Components.interfaces.nsIFile);
            if (temp && temp.isDirectory()) {
              crx = temp;
              break;
            }
          }
        }

        if (!crx)
          crx = null;
      } else {
        this.logError('Not a directory: ' + path);
      }
    }

    // Make sure the .crx or exploded directory is valid.
    if (!crx) {
      this.logInfo('getCrxToInstall: crx is undefined');
    } else if (!crx.exists()) {
      this.logError('getCrxToInstall: "' + crx.path +
                    '" does not exist');
      crx = null;
    } else {
      this.logInfo('getCrxToInstall: extension found at: ' + crx.path);
    }

    this.crxToInstall_ = crx;
  }

  // NOTE: its important to return a clone in case the caller modifies the
  // file object it receives.
  return this.crxToInstall_ ? this.crxToInstall_.clone() : null;
};

/**
 * Get a File object for the manifest of the Chrome extension registered.
 *
 * @param baseDir {File} A File object representing the base dir of
 * the extension.
 * @return A File object that represents the Chrome extension manifest.
 * @private
 */
CEEE_Class.prototype.getToolstripManifestFile_ = function(baseDir) {
  if (baseDir) {
    baseDir = baseDir.clone();
    baseDir.append('manifest.json');
    if (!baseDir.exists() || !baseDir.isFile())
      baseDir = null;
  }

  return baseDir;
};

/**
 * Computes the extension id from the given key.
 *
 * @private
 */
CEEE_Class.prototype.computeExtensionId_ = function(key) {
  if (key.length == 0)
    return null;

  // nsICryptoHash expects an array of base64 encoded bytes
  var encodedKey = window.atob(key);
  var encodedKeyArray = [];
  for (var i = 0; i < encodedKey.length; ++i)
   encodedKeyArray.push(encodedKey.charCodeAt(i));

  var cryptoHash = Components.classes['@mozilla.org/security/hash;1']
                    .createInstance(Components.interfaces.nsICryptoHash);
  cryptoHash.init(cryptoHash.SHA256);
  cryptoHash.update(encodedKeyArray, encodedKeyArray.length);
  var hashedKey = cryptoHash.finish(false);

  // Then the Id is constructed by offsetting the char 'a' with the nibble
  // values of the first 16 bytes of the Hex decoding of the hashed key.
  var base = 'a'.charCodeAt(0);
  var extensionId = '';
  for (var i = 0; i < 16; ++i) {
    var hexValue = ('0' + hashedKey.charCodeAt(i).toString(16)).slice(-2);
    extensionId += String.fromCharCode(base + parseInt(hexValue[0], 16));
    extensionId += String.fromCharCode(base + parseInt(hexValue[1], 16));
  }
  return extensionId;
};

/**
 * Load the toolstrip manifest file info and set it as attributes.
 *
 * @public
 */
CEEE_Class.prototype.loadToolstripManifest = function(baseDir) {
  if (!CEEE_json)
    return;

  var manifestFile = this.getToolstripManifestFile_(baseDir);
  if (!manifestFile) {
    this.logError('loadToolstripManifest: no manifest');
    return;
  }

  this.logInfo(
      'loadToolstripManifest: loading manifest: ' + manifestFile.path);

  try {
    var stream = Components.classes['@mozilla.org/network/file-input-stream;1'].
        createInstance(Components.interfaces.nsIFileInputStream);
    stream.init(manifestFile, -1, 0, 0);
    var manifestData = CEEE_json.decodeFromStream(stream, -1);
  } catch (ex) {
    this.logError('loadToolstripManifest: error reading manifest: ' + ex);
    return;
  } finally {
    if (stream)
      stream.close();
  }

  if (!manifestData) {
    this.logError('loadToolstripManifest: no manifest data');
    return;
  }

  if (!manifestData.key) {
    this.logError('loadToolstripManifest: key missing from manifest');
    return;
  }

  if (manifestData.toolstrips && manifestData.toolstrips.length > 0) {
    this.extensionId_ = this.computeExtensionId_(manifestData.key);
    if (this.extensionId_) {
      // TODO(mad@chromium.org): For now we only load the first one we find,
      // we may want to stack them at one point...
      this.toolstripUrl_ = 'chrome-extension://' + this.extensionId_ + '/' +
                          manifestData.toolstrips[0];
    }
  }

  // Extract all content scripts from the manifest.  This include javascript
  // code to run as well as CSS scripts to load.
  this.userscriptsModule_.loadUserScripts(manifestData);

  // Add more code here to extract more stuff from the manifest,
  // user scripts for example.
};

/**
 * Get the Chrome extension Id of the toolstrip.
 *
 * @return A string representing the Chrome extension Id of the toolstrip.
 * @public
 */
CEEE_Class.prototype.getToolstripExtensionId = function() {
  return this.extensionId_;
};

/**
 * Get the base directory of our one and only extension.  Valid only
 * after the extensions list has been loaded.
 *
 * @return A File object for the base directory of the extension.
 */
CEEE_Class.prototype.getToolstripDir = function() {
  return this.toolstripDir_.clone();
};

/**
 * Get the URL of the Toolstrip.
 *
 * @return A string that represents the URL of the HTML file to use for the
 *         toolstrip.
 */
CEEE_Class.prototype.getToolstripUrl = function() {
  // If the URL is overridden in the prefences, then use that.
  var url = CEEE_globals.getCharPreference('url');

  // If no override found, use the toolstrip's default URL, if any.
  if ((!url || 0 == url.length) && this.toolstripUrl_) {
    url = this.toolstripUrl_;
  }

  return url;
};

/**
 * Open a stream for reading from the given file.
 *
 * @param {!nsILocalFile} file The file to open for reading.
 * @return An nsIConverterInputStream that can be read from.
 * @public
 */
CEEE_Class.prototype.openFileStream = function(file) {
  var fstream = Components.classes['@mozilla.org/network/file-input-stream;1'].
      createInstance(Components.interfaces.nsIFileInputStream);
  fstream.init(file, -1, 0, 0);

  // Make sure to specify the size of the file, or the read will be truncated
  // the to default size, which is 8kb as of this writing.
  var stream =
      Components.classes['@mozilla.org/intl/converter-input-stream;1'].
          createInstance(Components.interfaces.nsIConverterInputStream);
  stream.init(fstream, 'UTF-8', file.fileSize, 0);

  return stream;
};

/**
 * Called in the following cases:
 * - the extension running inside ChromeFrame makes a Google Chrome extension
 *   API call.  In this case, the origin is '__priv_xtapi' and the target is
 *   '__priv_xtreq'
 * - a post message request to a given port from the extenstion.  In this case,
 *   the origin is '__priv_xtapi' and the target is '__priv_prtreq'.
 * - a response from a post message request to the extenstion.  In this case,
 *   the origin is '__priv_xtapi' and the target is '__priv_prtres'.
 *
 * Google Chrome extension API calls
 * ---------------------------------
 * This function is passed an object whose data property is a json-encoded
 * string describing the API call that was made. The json encoding is defined
 * as follows:
 *
 *     {
 *       'name': '<api-function-name>',
 *       'args': '[<argument-0>, <argument-1>, ...]',
 *       'rqid': <id>
 *     }
 *
 * Note that the args property is itself a json-encoded array of the APIs
 * arguments.  If the API has no arguments then args is 'null'.
 *
 * If the caller expects a response, then the rqid value must be
 * present in that response.  A response to an API call is done by
 * json-encoding a response object and calling postMesasge() on the ChromeFrame
 * instance.  The response object is defined as follows:
 *
 *     {
 *       'res': '<return-value>',
 *       'rqid': <id>
 *     }
 *
 * The <return-value> is itself a json-encoded string of whatever the API
 * is supposed to return.
 *
 * Port Request and Response
 * -------------------------
 * For a description of the format of the evt argument refer to the docs for
 * onPortEvent().
 *
 * @param {!Object} evt API event fired from the extension.
 * @param {!string} target The target of the message.
 * @private
 */
CEEE_Class.prototype.onCfMessage_ = function(evt, target) {
  // Do some sanity checking on the event that is passed to us.
  if (!evt || !CEEE_json)
    return;

  // if this is a response to a port request, give it special handling.
  if (target == this.cfHelper_.TARGET_PORT_REQUEST ||
      target == this.cfHelper_.TARGET_PORT_RESPONSE) {
    this.userscriptsModule_.onPortEvent(evt);
    return;
  }

  var data = CEEE_json.decode(evt.data);
  if (!data) {
    this.logError('onCfMessage_: no data');
    return;
  }

  var cmd = data.name;
  if (!cmd) {
    this.logError('onCfMessage_: no name');
    return;
  }

  if ('rqid' in data) {
    this.logInfo('onCfMessage_: cmd=' + cmd + ' rqid=' + data.rqid);
  }

  // All is good so far, so try to make the appropriate call.
  var value;
  var err;
  var handler = this.dispatch_[cmd];
  if (typeof handler == 'function') {
    try {
      value = handler.call(this, cmd, data);
    } catch(ex) {
      this.logError('onCfMessage_: exception thrown by handler cmd=' + cmd +
                    ' err=' + ex);
      err = ex.message;
      value = null;
    }
  } else {
    this.logError('onCfMessage_: dispatch entry is not a function');
  }

  // If the caller expects a response, then send it now.
  if ('rqid' in data) {
    var r = {};
    r.rqid = data.rqid;
    r.res = typeof value != 'undefined' ? CEEE_json.encode(value) : '';
    if (err)
      r.err = err;

    this.cfHelper_.postMessage(CEEE_json.encode(r),
                               this.cfHelper_.TARGET_API_RESPONSE);
  }
};

/**
 * Called when code in the DOM context makes a CEEE API call.  DOM context
 * refers to code running in the context of a regular web page loaded into one
 * of the Firefox main tabs.
 *
 * @param {!Object} w Window of the DOM context.
 * @param {!Object} evt A DOM event fired from plugin.
 * @private
 */
CEEE_Class.prototype.onDomApiCall_ = function(w, evt) {
  // Do some sanity checking on the event that is passed to us.
  if (!evt || !evt.target || !CEEE_json)
    return;

  var s = evt.target.getAttribute(this.DATA_ATTRIBUTE_NAME_);
  var data = CEEE_json.decode(s);
  if (!data) {
    this.logError('onDomApiCall_: no data');
    return;
  }

  var cmd = data.name;
  if (!cmd) {
    this.logError('onDomApiCall_: no name');
    return;
  }

  var handler = this.dom_dispatch_[cmd];
  var value;
  if (typeof handler == 'function') {
    try {
      value = handler.call(this, w, cmd, data);
    } catch(ex) {
      this.logError('onDomApiCall_: exception thrown by handler cmd=' + cmd +
                    ' err=' + ex);
      value = '';
    }
  } else {
    this.logError('onDomApiCall_: no dom_dispatch function for cmd=' + cmd);
  }

  if (value) {
    // The built-in Firefox json encoding does not work with primitive types,
    // so check for that explicitly and handle correctly.
    var s =
        typeof value == 'object' ? CEEE_json.encode(value) : value;
    evt.target.setAttribute(this.RETURN_ATTRIBUTE_NAME_, s);
  }
};

/**
 * Assign a handler associated with the string given in command.
 * @param {string} command
 * @param {Object} object
 * @param {function(Object, Object):Object} handler
 * @public
 */
CEEE_Class.prototype.registerExtensionHandler = function(command,
                                                         object,
                                                         handler) {
  this.dispatch_[command] =
      function() {return handler.apply(object, arguments);};
}

/**
 * Assign a handler associated with the DOM event corresponding to the string
 * given in command argument.
 * @param {string} command
 * @param {Object} object
 * @param {function(Object, Object):Object} handler
 * @public
 */
CEEE_Class.prototype.registerDomHandler = function(command, object, handler) {
  this.dom_dispatch_[command] =
      function() {return handler.apply(object, arguments);};
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

/**
 * Global CEEE initialization routine.
 * @return {Object} A reference to the initialized CEEE instance.
 * @public
 */
function CEEE_initialize() {
  var instance = new CEEE_Class();

  window.addEventListener('load', function() {instance.onLoad_();},
                          false);
  instance.logInfo('Started');

  if (instance.contentScriptDebugging)
    instance.logInfo('Content script debugging is enabled');

  return instance;
}

/**
* Global reference to the CEEE instance.
* @private
*/
var CEEE_instance_ = CEEE_initialize();

// })();
