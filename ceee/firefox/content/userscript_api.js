// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This file contains the implementation of the Chrome extension
 * user script APIs for the CEEE Firefox add-on.  This file is loaded by the
 * overlay.xul file, and requires that overlay.js has already been loaded.
 *
 * @supported Firefox 3.x
 */

/**
 * Constructor for user script management API object.
 * @param {!Object} instance A reference the the main CEEE instance for
 *     this top-level window.
 * @constructor
 */
var CEEE_UserScriptManager;

(function() {

/** A reference to the main CEEE instance for this top-level window. */
var ceee;

/**
 * A map of open ports for this web page for all extensions.  The key to the
 * map is a portId (number) that is local to this web page.  This portId is
 * assigned by the call to OpenChannelToExtension.
 *
 * The port is considered opened once ChromeFrame responds with the "channel
 * opened" event.  This event will contain another portId assigned on the
 * chrome side of the port, referred to here as the remote port Id.  Because
 * Chrome and ChromeFrame do not know about the portIds in this code, the
 * functions below map from portId to remotePortId and back as needed.
 */
var ports = CEEE_globals.ports;

/**
 * An array of "content_scripts", as defined by the Chrome Extension API.
 */
var scripts;

/**
 * Path relative to the add-on root where user-related scripts are found.
 * @const
 */
var USER_SCRIPTS_DIR = 'content/us';

/**
 * Create an nsIURL object from the given string spec.
 *
 * @param {!string} spec The string that should be converted into an nsURL
 *     object.
 */
function makeUrl(spec) {
  var uri = CEEE_ioService.newURI(spec, null, null);
  return uri && uri.QueryInterface(Components.interfaces.nsIURL);
}

/**
 * Create an nsIURL object from the given nsIFile object.
 *
 * @param {!nsIFile} file The file that should be converted into an nsURL
 *     object.
 */
function makeFileUrl(file) {
  var uri = CEEE_ioService.newFileURI(file);
  return uri && uri.QueryInterface(Components.interfaces.nsIURL);
}

/**
 * Is the given pattern a valid host pattern, as defined by the Chrome
 * URLPattern class?  There are only 3 possible values:
 *
 *  - '*', which matches all hosts
 *  - '<anychar except '*'>'+, which matches an exact domain
 *  - '*.<anychar except '*'>'+, which matches a subdomain
 *
 * Its assumed that the pattern does not contain an slashes (/).
 *
 * @param {!string} pattern String pattern to test.
 * @return true if the pattern is valid.
 */
function isValidHostPattern(pattern) {
  return pattern == '*' || pattern.indexOf('*') == -1 ||
      (pattern.charAt(0) == '*' && pattern.charAt(1) == '.' &&
          pattern.substr(2).indexOf('*') == -1);
}

/**
 * Does the given URL match one of the URL patterns given?
 *
 * @param {!Location} url URL string to match.
 * @param {Array} patterns Array of patterns to match.
 */
function matchesPattern(url, patterns) {
  // Some URLs may not have a host, like about:blank.  In this case, we can
  // never match.  In firefox, this is implemented with the nsSimpleURI class.
  // The implementation of this class will always throw an error when accessing
  // the host property, so we first check to see if the protocol is not
  // "about:" before checking the host.  nsSimpleURI is designed specifically
  // for about:
  var protocol = url.protocol;
  if (protocol == 'about:')
    return false;

  var host = url.host;
  var path = url.pathname + url.search;

  for (var i = 0; i < patterns.length; ++i) {
    var pattern = patterns[i];

    // Make sure protocols match.
    if (pattern.protocolPattern != protocol) {
      continue;
    }

    // Make sure hosts match.
    var hostPattern = pattern.hostPattern;
    if (hostPattern.length > 0) {
      if (hostPattern.charAt(0) != '*') {
        if (host != hostPattern) {
          continue;
        }
      } else {
        // Make sure the host ends with the pattern (minus initial *).
        var sub = hostPattern.substr(1);
        if (sub != host.match(sub)) {
          continue;
        }
      }
    }

    // Make sure paths match.
    var pathPattern = pattern.pathPattern;
    if (path && !pathPattern.test(path)) {
      continue;
    }

    // Found a match!
    ceee.logInfo('URL=' + url.toString() + ' matches with ' +
                 pattern.protocolPattern + ' ' + pattern.hostPattern + ' ' +
                 pattern.pathPattern);
    return true;
  }

  return false;
}

/**
 * Checks whether the given window is the top window.
 *
 * @param {!nsIDOMWindow} w The window to test.
 * @return {boolean} true if the given window is the top window.
 */
function isTopWindow(w) {
  return w.top == w.self;
}

/**
 * Get a File object for the given user script.  Its assumed that the user
 * script file name is relative to USER_SCRIPTS_DIR in the add-on.
 *
 * @param {!string} name Relative file name of the user script.
 * @return A File object that represents the user script.
 */
function getScriptFile(name) {
  var m = Components.classes['@mozilla.org/extensions/manager;1'].
      getService(Components.interfaces.nsIExtensionManager);
  var file = m.getInstallLocation(CEEE_globals.ADDON_ID)
              .getItemFile(CEEE_globals.ADDON_ID, USER_SCRIPTS_DIR);

  file.append(name);
  return file;
}

/**
 * Get a File object for the given user script.  Its assumed that the user
 * script file name is relative to the Chrome Extension root.
 *
 * @param {!string} name Relative file name of the user script.
 * @return A File object that represents the user script.
 */
function getUserScriptFile(name) {
  var file = ceee.getToolstripDir();
  if (file) {
    var parts = name.split('/');
    for (var i = 0; i < parts.length; ++i) {
      file.append(parts[i]);
    }

    if (!file.exists() || !file.isFile())
      file = null;
  }

  if (file)
    ceee.logInfo('User script: ' + file.path);

  return file;
}

/**
 * Evaluate a user-defined script file in the context of a web page, with
 * debugging in mind.  Instead of evaluating the script in a sandbox, which
 * Firebug cannot see, a <script> element is added to the page.
 *
 * Note that this depends on the Chrome Extension APIs being avaiable to page
 * code, which normally it is not.  See prepareSandbox() for how these APIs
 * are conditionally injected to be accessed from the page.
 *
 * For Firefox to allow loading of file URLs into remote pages, the security
 * manager needs to be told to allow this.  For more details about this see:
 *   - http://kb.mozillazine.org/Security_Policies
 *   - http://www.mozilla.org/projects/security/components/ConfigPolicy.html
 *
 * If this is not setup correctly, Firefox's error console will show a message
 * similar to this:
 *
 *  Security Error: Content at <remote-url> may not load or link to <file-url>.
 *
 * The security manager is tweaked appropriately in the options dialog, see
 * the code in options.xul.
 *
 * @param {nsIDOMWindow} w Content window into which to evaluate the script.
 * @param {!nsIFile} file The script to evaluate.
 */
function evalUserScriptFileForDebugging(w, file) {
  // Create the script from the given file object.
  var d = w.document;
  var script = d.createElement('script');
  script.type = 'text/javascript';
  script.src = makeFileUrl(file).spec;
  d.body.appendChild(script);
  ceee.logInfo('Debugging script: ' + file.path);
}

/**
 * Add the CSS code string to the document of the specified window.
 *
 * @param {!nsIDOMWindow} w Content window into which to evaluate the script.
 * @param {string} code The CSS code string to evaluate.
 */
function addCssCodeToPage(w, code) {
  var d = w.document;
  var coll = d.getElementsByTagName('head');
  if (!coll || coll.length != 1) {
    ceee.logError('Cannot find <head> in doc=' + w.location.toString());
    return;
  }

  var head = coll[0];
  var css = d.createElement('style');
  css.type = 'text/css';
  css.appendChild(d.createTextNode(code));
  head.appendChild(css);
}

/**
 * Add the CSS file to the document of the specified window.
 *
 * @param {!nsIDOMWindow} w Content window into which to evaluate the script.
 * @param {nsIFile} file The script to evaluate.
 */
function addCssToPage(w, file) {
  // Read the CSS from the file into memory.
  var data = {};
  try {
    var stream = ceee.openFileStream(file);
    stream.readString(-1, data);
    addCssCodeToPage(w, data.value);
    ceee.logInfo('Loaded CSS file: ' + file.path);
  } catch (ex) {
    // Note that for now, this will happen regularly if you have more
    // than one extension installed and the extension not being
    // treated as the "one and only" by CEEE happens to call
    // insertCss.
    ceee.logError('addCssToPage, exception: ' + ex.message);
  } finally {
    if (stream)
      stream.close();
  }
}

/**
 * Post an open channel message to ChromeFrame for the given port.
 *
 * @param {number} portId The internal port Id of the port to open.
 * @param {string} extId The extension Id associated with this port.
 * @param {string} name The name to assign to the port.
 * @param {!Object} tabInfo Information about the tab containing the
 *     content script that is opening the channel.
 */
function postOpenChannel(portId, extId, name, tabInfo) {
  var msg = {
    rqid: 0,  // 0 == open channel
    extid: extId,
    chname: name,
    connid: portId,
    tab: tabInfo
  };

  var cfh = ceee.getCfHelper();
  cfh.postMessage(CEEE_json.encode(msg), cfh.TARGET_PORT_REQUEST);
}

/**
 * Post a close channel message to ChromeFrame for the given port.
 *
 * @param {!Object} info Information about the port.
 */
function postCloseChannel(info) {
  // Don't send another close channel message if this port has alredy been
  // disconnected.
  if (info.disconnected)
    return;

  // If the remote port id is not valid, just return.  This could happen if
  // the port is closed before we get a response from ChromeFrame about the
  // port being opened.
  var remotePortId = info.remotePortId;
  if (!remotePortId || remotePortId == -1)
    return;

  var msg = {
    rqid: 3,  // 3 == close channel
    portid: remotePortId
  };

  var cfh = ceee.getCfHelper();
  cfh.postMessage(CEEE_json.encode(msg), cfh.TARGET_PORT_REQUEST);
  info.disconnected = true;
}

/**
 * Post a port message to ChromeFrame for the given port.
 *
 * @param {number} remotePortId The remote port Id of the port.
 * @param {string} msg The message to post.
 */
function postPortMessage(remotePortId, msg) {
  var msg2 = {
    rqid: 2,  // 2 == post message
    portid: remotePortId,
    data: msg
  };

  var cfh = ceee.getCfHelper();
  cfh.postMessage(CEEE_json.encode(msg2), cfh.TARGET_PORT_REQUEST);
}

/**
 * Find a port given its remote port Id.  The remote port Id is the Id assigned
 * to the port by chrome upon connection, which is different from the port Id
 * assigned by the FF CEEE and used as the key in the ports map.
 *
 * May want to review this code to see if we want to optimize the remote port
 * Id lookup.
 *
 * @param {number} remotePortId An integer value representing the Id.
 */
function findPortByRemoteId(remotePortId) {
  for (var portId in ports) {
    var info = ports[portId];
    if (remotePortId == info.remotePortId)
      return info;
  }
}

/**
 * Cleanup all resources used by the port with the given Id.
 *
 * @param {number} portId An integer value representing the FF CEEE port Id.
 */
function cleanupPort(portId) {
  var info = ports[portId];
  if (info) {
    postCloseChannel(info);
    delete ports[portId];
    ceee.logInfo('cleanupPort: cleaned up portId=' + portId);
  } else {
    ceee.logInfo('cleanupPort: Invalid portId=' + portId);
  }
}

/**
 * Evaluate a user-defined script in the context of a web page.
 *
 * @param {!Object} w The DOM window in which to run the script.
 * @param {string} script A string that represents the javascript code to
 *     run against a web page.
 * @param {string=} opt_filename An optional string that contains the name of
 *     the script being executed.
 * @private
 */
function evalUserScript(w, script, opt_filename) {
  evalInSandboxWithCatch(script, prepareSandbox(w), opt_filename);
}

/**
 * Prepare a sandbox for running user scripts.
 *
 * @param {!Object} w The content window in which the user scripts will run.
 * @return A Firefox sandbox properly initialized for running user scripts.
 */
function prepareSandbox(w) {
  // If the window already has a FF CEEE sandbox, use it instead of creating
  // a new one.
  if (w.ceeeSandbox) {
    ceee.logInfo('Reusing sandbox for ' + w.location.toString());
    return w.ceeeSandbox;
  }

  // We need to be careful to run the user script with limited privilege,
  // and in the scope of the given window.  We do this by creating a sandbox
  // around that window and evaluating the script there.
  //
  // There are security issues with using the object that is returned from
  // evaluating the expression in the sandbox.  For now, I ignore the return
  // value, see https://developer.mozilla.org/En/Components.utils.evalInSandbox.
  //
  // We wrap the window given argument because that object comes from the
  // unsafe environment of the web page being shown.  We want to expose our
  // script to a 'safe' environment by default.  However, just in case some
  // user scripts really want the original window, we'll pass that in too.
  //
  // To sum up, the code in this file is considered 'privileged' code, and must
  // be careful what it executes.  There are two sources of unsafe code: the
  // user script, which comes from the CEEE add-on, and the document+window
  // that come from the window argument.  We need to be careful with objects we
  // get from both of these places.
  var unsafeWindow = w;
  var safeWindow = new XPCNativeWrapper(unsafeWindow);

  // Create a sandbox for running the script and initialize the environment.
  // TODO(rogerta@chromium.org): for perfomance reasons, we may want
  // to cache the user scripts somehow.
  var s = Components.utils.Sandbox(safeWindow);
  s.window = safeWindow;
  s.document = safeWindow.document;
  s.__proto__ = s.window;

  // TODO(rogerta@chromium.org): Not sure if I need to set this member.
  // s.XPathResult = Components.interfaces.nsIDOMXPathResult;

  // TODO(rogerta@chromium.org): for performance reasons. maybe want
  // to concat all these files into one and eval once.  Or maybe load
  // all files into memory and eval from there instead of from file
  // each time.
  evalScriptFileInSandbox(getScriptFile('base.js'), s);
  evalScriptFileInSandbox(getScriptFile('json.js'), s);
  evalScriptFileInSandbox(getScriptFile('ceee_bootstrap.js'), s);
  evalScriptFileInSandbox(getScriptFile('event_bindings.js'), s);
  evalScriptFileInSandbox(getScriptFile('renderer_extension_bindings.js'), s);

  s.JSON = s.goog && s.goog.json;

  // Firebug may install it own console, so we won't override it here.
  s.window.console = s.window.console || s.console;

  // For test purposes, I will add the APIs to the actual page itself, so that
  // test code can call it.  This is to make the APIs available to javascript
  // already in the page, not to the inject scripts, which already have the
  // APIs.
  if (ceee.contentScriptDebugging) {
    w.wrappedJSObject.chrome = w.wrappedJSObject.chrome || s.chrome;

    // When running with Firebug enabled, the window object will already
    // have console property.  However, Firebug declares console with as getter
    // only, so the standard javascript pattern "foo = foo || bar;" causes an
    // exception to be thrown.  Therefore using an if() check.
    if (!w.wrappedJSObject.console)
      w.wrappedJSObject.console = s.console;

    w.wrappedJSObject.goog = w.wrappedJSObject.goog || s.goog;
    w.wrappedJSObject.JSON = w.wrappedJSObject.JSON || s.JSON;
    w.wrappedJSObject.ceee = w.wrappedJSObject.ceee || s.ceee;
  }

  // Make sure the chrome extension execution environment is properly setup
  // for the user script.
  // TODO(rogerta@chromium.org): for now, this assumes only one
  // extension at a time.
  var createExtensionScript = 'chrome.initExtension("';
  createExtensionScript += ceee.getToolstripExtensionId();
  createExtensionScript += '")';
  evalInSandboxWithCatch(createExtensionScript, s, 'createExtensionScript');

  w.ceeeSandbox = s;
  ceee.logInfo('Created new sandbox for ' + w.location.toString());
  return s;
}

/**
 * Evaluate a script file in the context of a sandbox.
 *
 * @param {!Object} file A Firefox nsIFile object containing the script.
 * @param {!Object} s A Firefox sandbox.
 */
function evalScriptFileInSandbox(file, s) {
  if (!file) {
    ceee.logError('Trying to eval a null script');
    return;
  }

  // Read the script from the file into memory, so that we can execute it.
  var data = {};
  try {
    var stream = ceee.openFileStream(file);
    stream.readString(-1, data);

    // Execute the script.
    evalInSandboxWithCatch(data.value, s, file.leafName);
  } finally {
    if (stream)
      stream.close();
  }
}

/**
 * Evaluate a script in the context of a sandbox.
 *
 * @param {string} script A string that represents the javascript code to
 *     run against a web page.
 * @param {!Object} s A Firefox sandbox.
 * @param {string=} opt_filename An optional string that contains the name of
 *     the script being executed.
 * @private
 */
function evalInSandboxWithCatch(script, s, opt_filename) {
  try {
    // To get around a bug in firefox where it incorrectly determines the line
    // number of exceptions thrown from inside a sandbox, create an error
    // here to be used later.
    var helper = new Error();
    evalInSandboxWrapper(script, s);
  } catch (ex) {
    var line = ex && ex.lineNumber;
    // If the line number from the exception is MAX_INT, then we need to look
    // harder for the line number.  See if the exception has a location
    // property, and if so, if it has a line number.
    if (4294967295 == line) {
      if (ex.location && ex.location.lineNumber) {
        line = ex.location.lineNumber;
      } else {
        line = 0;
      }
    }

    // Subtract the line number from the dummy exception created before the
    // call into the sandbox.  As explained above, this is to get around a
    // bug in the way that Firefox reports line numbers in sandboxes.
    if (line > helper.lineNumber) {
      line -= helper.lineNumber;
    } else {
      line = 'unknown-line';
    }

    // Build the message to log to the console.  If a 'filename' is specified,
    // add it to the message.
    var message = 'Error in ';
    if (opt_filename)
      message += opt_filename + ':';

    ceee.logError('Sandbox: ' + message + line + '::  ' + ex);
  }
}

/**
 * Constructor for user script manager object.
 *
 * @param {!Object} instance A reference the the main CEEE instance for
 *     this top-level window.
 */
function UserScriptManager(instance) {
  // The caller must specify the CEEE instance object.
  if (!instance)
    throw 'Must specify instance';

  ceee = instance;
  ceee.logInfo('UserScriptManager created');

  // Register all handlers.
  ceee.registerDomHandler('AttachEvent', this, this.AttachEvent);
  ceee.registerDomHandler('DetachEvent', this, this.DetachEvent);
  ceee.registerDomHandler('OpenChannelToExtension', this,
                          this.OpenChannelToExtension);
  ceee.registerDomHandler('PortAddRef', this, this.PortAddRef);
  ceee.registerDomHandler('PortRelease', this, this.PortRelease);
  ceee.registerDomHandler('CloseChannel', this, this.CloseChannel);
  ceee.registerDomHandler('PostMessage', this, this.PostMessage);
  ceee.registerDomHandler('LogToConsole', this, this.LogToConsole);
  ceee.registerDomHandler('ErrorToConsole', this, this.ErrorToConsole);
}

/**
 * Find all content scripts parts (javascript code and CSS) in the manifest
 * file and load them.
 *
 * @param {!Object} manifest An object representing a valid Chrome Extension
 *     manifest.
 */
UserScriptManager.prototype.loadUserScripts = function(manifest) {
  // If the user scripts have already been loaded, then nothing to do.  This
  // happens when the second top-level window is opened, and the extension
  // has already been parsed.
  if (scripts && scripts.length > 0)
    return;

  CEEE_globals.scripts = manifest.content_scripts || [];
  scripts = CEEE_globals.scripts;
  var count = (scripts && scripts.length) || 0;

  for (var i = 0; i < count; ++i) {
    var script = scripts[i];
    var matches = script.matches;
    if (!matches)
      continue;

    // Convert the URL strings in the matches property to two string: one for
    // matching host names, and another for matching paths.  These two parts
    // are matched independently, as described in the Chrome URLPattern class.
    var patterns = [];
    for (var j = 0; j < matches.length; ++j) {
      var spec = matches[j];
      var url = makeUrl(spec);

      // Make a pattern from the host glob.
      var hostPattern = url.host;
      if (!isValidHostPattern(hostPattern)) {
        ceee.logError('Invalid user script pattern: ' + hostPattern);
        continue;
      }

      // Make a regex from the path glob pattern.
      var pathPattern = url.path;

      // Replace all stars (*) with '.*'.
      // Replace all '?' with '\?'.
      pathPattern = pathPattern.replace(/\*/g, '.*');
      pathPattern = pathPattern.replace(/\?/g, '\\?');

      // The nsIURL object's scheme property is the protocol of the URL, but
      // without the trailing colon.  This will eventually be compared to a
      // windows.location object, that has a protocol property *with* the
      // trailing the colon.  So I am adding the colon here to make things
      // easier to compare later.
      var pattern = {
        protocolPattern: url.scheme + ':',
        hostPattern: hostPattern,
        pathPattern: new RegExp(pathPattern)
      };
      patterns.push(pattern);
    }

    script.patterns = patterns;

    // Set the default value for all_frames
    if (script.all_frames === undefined) {
      script.all_frames = false;
    }
  }

  ceee.logInfo('loadUserScripts: done count=' + count);
};

/**
 * Run all user scripts installed on the system that match the URL of the
 * document attached to the window specified in the w argument.
 *
 * @param {!Object} w The content window in which to run any matching user
 *     scripts.
 */
UserScriptManager.prototype.runUserScripts = function(w) {
  // Note that we read the userscript files from disk every time that a page
  // loads. This offers us the advantage of not having to reload scripts into
  // memory when an extension autoupdates in Chrome. If we decide to cache
  // these scripts in memory, then we will need a Chrome extension autoupdate
  // automation notification.

  ceee.logInfo('runUserScripts: starting');
  var s = prepareSandbox(w);

  // Run each user script whose pattern matches the URL of content window.
  for (var i = 0; i < scripts.length; ++i) {
    var script = scripts[i];
    if ((script.all_frames || isTopWindow(w)) &&
        matchesPattern(w.location, script.patterns)) {
      // First inject CSS script, in case the javascript code assume the styles
      // already exist.
      var css = script.css || [];
      var css_length = css.length;
      for (var j = 0; j < css_length; ++j) {
        var f = getUserScriptFile(css[j]);
        addCssToPage(w, f);
      }

      // Now inject the javascript code.
      var js = script.js || [];
      var js_length = js.length;
      for (var j = 0; j < js_length; ++j) {
        var f = getUserScriptFile(js[j]);
        if (ceee.contentScriptDebugging) {
          evalUserScriptFileForDebugging(w, f);
        } else {
          evalScriptFileInSandbox(f, s);
        }
      }
    }
  }

  ceee.logInfo('runUserScripts: done');
};

/**
 * Execute a script file in the context of the given window.
 *
 * @param {!Object} w Content window context to execute in.
 * @param {!Object} scriptDef A script definition object as defined by the
 *     Chrome Extensions API.
 */
UserScriptManager.prototype.executeScript = function(w, scriptDef) {
  if (!ceee.getToolstripDir()) {
    var self = this;
    ceee.runWhenExtensionsInited(w, 'executeScript', function() {
        self.executeScript(w, scriptDef);
      });
    ceee.logInfo('Deferred executeScript');
    return;
  }
  ceee.logInfo('Executing executeScript');

  var s = prepareSandbox(w);

  if (scriptDef.code && scriptDef.file)
    throw new Error('Code and file should not be specified at the same ' +
        'time in the second argument.');

  if (scriptDef.code) {
    evalInSandboxWithCatch(scriptDef.code, s, 'executeScripts.scriptDef.js');
  } else if (scriptDef.file) {
    try {
      evalScriptFileInSandbox(getUserScriptFile(scriptDef.file), s);
    } catch (ex) {
      // Note that for now, this will happen regularly if you have more
      // than one extension installed and the extension not being
      // treated as the "one and only" by CEEE happens to call
      // insertCss.
      ceee.logError('executeScript, exception: ' + ex.message);
    }
  }
};

/**
 * Insert a CSS file or script in the context of the given window.
 *
 * @param {!Window} w Content window context to execute in.
 * @param {!Object} details A script definition object as defined by the
 *     Chrome Extensions API.
 */
UserScriptManager.prototype.insertCss = function(w, details) {
  if (!ceee.getToolstripDir()) {
    var self = this;
    ceee.runWhenExtensionsInited(w, 'insertCss', function() {
        self.insertCss(w, details);
      });
    ceee.logInfo('Deferred insertCss');
    return;
  }
  ceee.logInfo('Executing insertCss');

  if (details.code && details.file)
    throw new Error('Code and file should not be specified at the same ' +
                    'time in the second argument.');

  if (details.code) {
    addCssCodeToPage(w, details.code);
  } else if (details.file) {
    addCssToPage(w, getUserScriptFile(details.file));
  } else {
    throw new Error('No source code or file specified.');
  }
};

/**
 * Perform any cleanup required when a content window is unloaded.
 *
 * @param {!Object} w Content window being unloaded.
 */
UserScriptManager.prototype.onContentUnloaded = function(w) {
  var script = 'ceee.hidden.dispatchOnUnload();';
  evalUserScript(w, script, 'onContentUnloaded_cleanup');
  this.cleanupPortsForWindow(w);
  delete w.ceeeSandbox;
};

/**
 * Cleanup port information for a closing window.
 *
 * @param {!Object} w Content window being closed.
 */
UserScriptManager.prototype.cleanupPortsForWindow = function(w) {
  var toCleanup = [];

  // First look though all the ports, looking for those that need to be cleaned
  // up.  Remember those for later.
  for (portId in ports) {
    var info = ports[portId];
    if (w == info.contentWindow)
      toCleanup.push(portId);
  }

  // Now loop though all ports that need to be cleaned up and do it.
  for (var i = 0; i < toCleanup.length; ++i) {
    var portId = toCleanup[i];
    cleanupPort(portId);
  }
};

/**
 * Handle the response from a port request.  Port requests are created via
 * the functions below, like OpenChannelToExtension and PostMessage.
 *
 * Response of Post Message Request from Userscript Port to Extenstion
 * -------------------------------------------------------------------
 * The data property of the evt argument has the form:
 *
 *    {
 *      'rqid': 1,  // 1 == channel opened
 *      'connid': <portId>,  // the key to ports map
 *      'portid': <remoteId>  // the port Id assigned on the extension side
 *    }
 *
 * Post Message Request from Extenstion to Userscript Port
 * -------------------------------------------------------
 * The data property of the evt argument has the form:
 *
 *    {
 *      'rqid': 1,  // 2 == post message
 *      'portid': <remoteId>  // the port Id assigned on the extension side
 *      'data': <message>  // message being posted.  A string
 *    }
 *
 * @param {!Object} evt An event object sent from ChromeFrame.
 * @private
 */
UserScriptManager.prototype.onPortEvent = function(evt) {
  var data = CEEE_json.decode(evt.data);
  if (!data)
    return;

  if (data.rqid == 1) {  // 1 == CHANNEL_OPENED
    var info = ports[data.connid];
    if (info) {
      info.remotePortId = data.portid;

      if (info.remotePortId != -1) {
        ceee.logInfo('onPortEvent: CHANNEL_OPENED portId=' + info.portId +
                         ' <-> remotePortId=' + info.remotePortId);

        // If this port has any pending messages, post them all now.
        var q = info.queue;
        if (q) {
          ceee.logInfo('onPortEvent: posting deferred messages for portId='
              + info.portId);
          while (q.length > 0) {
            var msg = q.shift();
            postPortMessage(info.remotePortId, msg);
          }
          delete info.queue;
        }
      } else {
        ceee.logError('onPortEvent: bad remote port for portId=' +
            info.portId);
        var q = info.queue;
        if (q) {
          while (q.length > 0) {
            q.shift();
          }
          delete info.queue;
        }
      }
    } else {
      ceee.logError('onPortEvent: No port object found, rqid=1 connid=' +
          data.connid);
    }
  } else if (data.rqid == 2) {  // 2 == POST_MESSAGE
    // Fire the given message to the appropriate port in the correct content
    // window.
    var info = findPortByRemoteId(data.portid);
    if (info) {
      var msgt = data.data.replace(/\\/gi, '\\\\');
      var msg = msgt.replace(/"/gi, '\\"');
      var script = 'ceee.hidden.Port.dispatchOnMessage("{msg}",{portId});';
      script = script.replace(/{msg}/, msg);
      script = script.replace(/{portId}/, info.portId);

      evalUserScript(info.contentWindow, script, 'onPortEvent<post_message>');
    } else {
      ceee.logError('Could not find port for remote id=' + data.portid);
    }
  } else if (data.rqid == 3) {  // 3 == CHANNEL_CLOSED
    var info = findPortByRemoteId(data.portid);
    if (info) {
      var script = 'ceee.hidden.Port.dispatchOnDisconnect({portId});';
      script = script.replace(/{portId}/, info.portId);
      evalUserScript(info.contentWindow, script, 'onPortEvent<channel_closed>');
      cleanupPort(info.portId);
    }
  } else {
    ceee.logError('onPortEvent: unexptected rqid=' + data.rqid);
  }
};

UserScriptManager.prototype.AttachEvent = function(w, cmd, data) {
  //alert('AttachEvent '+ CEEE_json.encode(data.args));
};

UserScriptManager.prototype.DetachEvent = function(w, cmd, data) {
  //alert('DetachEvent '+ CEEE_json.encode(data.args));
};

UserScriptManager.prototype.OpenChannelToExtension = function(w, cmd, data) {
  var args = data.args;
  if (args.length != 3) {
    this.logError_('OpenChannelToExtension: invalid arguments');
    return;
  }

  var portId = CEEE_globals.getNextPortId();

  // Save some information about this port.
  var info = {};
  info.refCount = 0;
  info.portId = portId;
  info.sourceExtId = args[0];
  info.targetExtId = args[1];
  info.name = args[2];
  info.contentWindow = w;
  info.disconnected = false;
  ports[portId] = info;
  var tabInfo = null;
  var mainBrowser = document.getElementById(CEEE_globals.MAIN_BROWSER_ID);
  var container = mainBrowser && mainBrowser.tabContainer;
  if (container) {
    // Build a tab info object that corresponds to the sender tab.
    // Note that the content script may be running in a child window to the one
    // that corresponds to the tab; we use the top-level window here to get the
    // correct tab information.
    var index = mainBrowser.getBrowserIndexForDocument(w.top.document);
    var tab = container.getItemAtIndex(index);
    tabInfo = ceee.getTabModule().buildTabValue(mainBrowser, tab);
  }
  postOpenChannel(portId, info.targetExtId, info.name, tabInfo);
  return portId;
};

UserScriptManager.prototype.PortAddRef = function(w, cmd, data) {
  var args = data.args;
  if (args.length != 1) {
    this.logError_('PortAddRef: invalid arguments');
    return;
  }

  var portId = args[0];
  var info = ports[portId];
  if (info)
    info.refCount++;
};

UserScriptManager.prototype.PortRelease = function(w, cmd, data) {
  var args = data.args;
  if (args.length != 1) {
    this.logError_('PortRelease: invalid arguments');
    return;
  }

  var portId = args[0];
  var info = ports[portId];
  if (info) {
    if (--info.refCount == 0)
      cleanupPort(portId);
  }
};

UserScriptManager.prototype.CloseChannel = function(w, cmd, data) {
  var args = data.args;
  if (args.length != 1) {
    this.logError_('CloseChannel: invalid arguments');
    return;
  }

  var portId = args[0];
  var info = ports[portId];
  if (info) {
    // We tell the other side that the port is closed, but we don't cleanup
    // the info object just yet.  It will get cleaned up either when the
    // reference count goes to zero, or during the cleanup phase of a window
    // unload.
    postCloseChannel(info);
  }
};

UserScriptManager.prototype.PostMessage = function(w, cmd, data) {
  if (data.args.length != 2) {
    this.logError_('PostMessage: invalid arguments');
    return;
  }

  var portId = data.args[0];
  var msg = data.args[1];
  var info = ports[portId];
  if (info) {
    // Before posting or queuing the message, turn it into a string.
    if (typeof msg == 'object') {
      if (typeof msg != 'string')
        msg = CEEE_json.encode(msg);
    } else {
      msg = msg.toString();
    }

    // If remotePortId is -1, this means there was a problem opening this port.
    // Don't even bother trying to post a message in this case.
    if (info.remotePortId != -1) {
      if (typeof info.remotePortId != 'undefined') {
        postPortMessage(info.remotePortId, msg);
      } else {
        // Queue up message for port.  This message will eventually be sent
        // when the port is fully opened.  Messages are queued in the order in
        // which they are posted.
        info.queue = info.queue || [];
        info.queue.push(msg);
        ceee.logInfo('PostMessage: deferred for portId=' + portId);
      }
    }
  } else {
    ceee.logError('PostMessage: No port object found portId=' + portId +
        ' doc=' + w.document.location);
  }
};

UserScriptManager.prototype.LogToConsole = function(w, cmd, data) {
  if (data.args.length == 0) {
    ceee.logError('LogToConsole: needs one argument');
    return;
  }

  ceee.logInfo('Userscript says: ' + data.args[0]);
};

UserScriptManager.prototype.ErrorToConsole = function(w, cmd, data) {
  if (data.args.length == 0) {
    ceee.logError('ErrorToConsole: needs one argument');
    return;
  }

  ceee.logError('Userscript says: ' + data.args[0]);
};

// Make the constructor visible outside this anonymous block.
CEEE_UserScriptManager = UserScriptManager;

// This wrapper is purposefully the last function in this file, as far down
// as possible, for code coverage reasons.  It seems that code executed by
// evalInSandbox() is counted as code executing in this file, and the line
// number reported to coverage is the line number of the evalInSandbox() call
// plus the line inside script.  This messes up all the coverage data for all
// lines in this file after the call to evalInSandbox().  To minimize the
// problem, the call to Components.utils.evalInSandbox() is being placed as far
// to the end of this file as possible.
function evalInSandboxWrapper(script, s) {
  Components.utils.evalInSandbox(script, s);
}
})();
