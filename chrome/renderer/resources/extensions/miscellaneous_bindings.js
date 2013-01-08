// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This contains unprivileged javascript APIs for extensions and apps.  It
// can be loaded by any extension-related context, such as content scripts or
// background pages. See user_script_slave.cc for script that is loaded by
// content scripts only.

  require('json_schema');
  require('event_bindings');
  var lastError = require('lastError');
  var miscNatives = requireNative('miscellaneous_bindings');
  var CloseChannel = miscNatives.CloseChannel;
  var PortAddRef = miscNatives.PortAddRef;
  var PortRelease = miscNatives.PortRelease;
  var PostMessage = miscNatives.PostMessage;
  var BindToGC = miscNatives.BindToGC;

  var chromeHidden = requireNative('chrome_hidden').GetChromeHidden();

  var processNatives = requireNative('process');
  var manifestVersion = processNatives.GetManifestVersion();
  var extensionId = processNatives.GetExtensionId();

  // The reserved channel name for the sendRequest/sendMessage APIs.
  // Note: sendRequest is deprecated.
  chromeHidden.kRequestChannel = "chrome.extension.sendRequest";
  chromeHidden.kMessageChannel = "chrome.runtime.sendMessage";
  chromeHidden.kNativeMessageChannel = "chrome.runtime.sendNativeMessage";

  // Map of port IDs to port object.
  var ports = {};

  // Map of port IDs to chromeHidden.onUnload listeners. Keep track of these
  // to free the onUnload listeners when ports are closed.
  var portReleasers = {};

  // Change even to odd and vice versa, to get the other side of a given
  // channel.
  function getOppositePortId(portId) { return portId ^ 1; }

  // Port object.  Represents a connection to another script context through
  // which messages can be passed.
  function PortImpl(portId, opt_name) {
    this.portId_ = portId;
    this.name = opt_name;
    this.onDisconnect = new chrome.Event();
    this.onMessage = new chrome.Event();
  }

  // Sends a message asynchronously to the context on the other end of this
  // port.
  PortImpl.prototype.postMessage = function(msg) {
    // JSON.stringify doesn't support a root object which is undefined.
    if (msg === undefined)
      msg = null;
    PostMessage(this.portId_, chromeHidden.JSON.stringify(msg));
  };

  // Disconnects the port from the other end.
  PortImpl.prototype.disconnect = function() {
    CloseChannel(this.portId_, true);
    this.destroy_();
  };

  PortImpl.prototype.destroy_ = function() {
    var portId = this.portId_;

    this.onDisconnect.destroy_();
    this.onMessage.destroy_();

    PortRelease(portId);
    chromeHidden.onUnload.removeListener(portReleasers[portId]);

    delete ports[portId];
    delete portReleasers[portId];
  };

  chromeHidden.Port = {};

  // Returns true if the specified port id is in this context. This is used by
  // the C++ to avoid creating the javascript message for all the contexts that
  // don't care about a particular message.
  chromeHidden.Port.hasPort = function(portId) {
    return portId in ports;
  };

  // Hidden port creation function.  We don't want to expose an API that lets
  // people add arbitrary port IDs to the port list.
  chromeHidden.Port.createPort = function(portId, opt_name) {
    if (ports[portId]) {
      throw new Error("Port '" + portId + "' already exists.");
    }
    var port = new PortImpl(portId, opt_name);
    ports[portId] = port;
    portReleasers[portId] = PortRelease.bind(this, portId);
    chromeHidden.onUnload.addListener(portReleasers[portId]);

    PortAddRef(portId);
    return port;
  };

  // Helper function for dispatchOnRequest.
  function handleSendRequestError(isSendMessage, responseCallbackPreserved,
                                  sourceExtensionId, targetExtensionId) {
    var errorMsg;
    var eventName = (isSendMessage  ?
        "chrome.runtime.onMessage" : "chrome.extension.onRequest");
    if (isSendMessage && !responseCallbackPreserved) {
      errorMsg =
          "The " + eventName + " listener must return true if you want to" +
          " send a response after the listener returns ";
    } else {
      errorMsg =
          "Cannot send a response more than once per " + eventName +
          " listener per document";
    }
    errorMsg += " (message was sent by extension " + sourceExtensionId;
    if (sourceExtensionId != targetExtensionId)
      errorMsg += " for extension " + targetExtensionId;
    errorMsg += ").";
    lastError.set(errorMsg);
    console.error("Could not send response: " + errorMsg);
  }

  // Helper function for dispatchOnConnect
  function dispatchOnRequest(portId, channelName, sender,
                             sourceExtensionId, targetExtensionId,
                             isExternal) {
    var isSendMessage = channelName == chromeHidden.kMessageChannel;
    var requestEvent = (isSendMessage ?
        (isExternal ?
            chrome.runtime.onMessageExternal : chrome.runtime.onMessage) :
        (isExternal ?
            chrome.extension.onRequestExternal : chrome.extension.onRequest));
    if (requestEvent.hasListeners()) {
      var port = chromeHidden.Port.createPort(portId, channelName);
      port.onMessage.addListener(function(request) {
        var responseCallbackPreserved = false;
        var responseCallback = function(response) {
          if (port) {
            port.postMessage(response);
            port.destroy_();
            port = null;
          } else {
            // We nulled out port when sending the response, and now the page
            // is trying to send another response for the same request.
            handleSendRequestError(isSendMessage, responseCallbackPreserved,
                                   sourceExtensionId, targetExtensionId);
          }
        };
        // In case the extension never invokes the responseCallback, and also
        // doesn't keep a reference to it, we need to clean up the port. Do
        // so by attaching to the garbage collection of the responseCallback
        // using some native hackery.
        BindToGC(responseCallback, function() {
          if (port) {
            port.destroy_();
            port = null;
          }
        });
        if (!isSendMessage) {
          requestEvent.dispatch(request, sender, responseCallback);
        } else {
          var rv = requestEvent.dispatch(request, sender, responseCallback);
          responseCallbackPreserved =
              rv && rv.results && rv.results.indexOf(true) > -1;
          if (!responseCallbackPreserved && port) {
            // If they didn't access the response callback, they're not
            // going to send a response, so clean up the port immediately.
            port.destroy_();
            port = null;
          }
        }
      });
      return true;
    }
    return false;
  }

  // Called by native code when a channel has been opened to this context.
  chromeHidden.Port.dispatchOnConnect = function(portId, channelName, tab,
                                                 sourceExtensionId,
                                                 targetExtensionId) {
    // Only create a new Port if someone is actually listening for a connection.
    // In addition to being an optimization, this also fixes a bug where if 2
    // channels were opened to and from the same process, closing one would
    // close both.
    if (targetExtensionId != extensionId)
      return false;  // not for us
    if (ports[getOppositePortId(portId)])
      return false;  // this channel was opened by us, so ignore it

    // Determine whether this is coming from another extension, so we can use
    // the right event.
    var isExternal = sourceExtensionId != extensionId;

    if (tab)
      tab = chromeHidden.JSON.parse(tab);
    var sender = {tab: tab, id: sourceExtensionId};

    // Special case for sendRequest/onRequest and sendMessage/onMessage.
    if (channelName == chromeHidden.kRequestChannel ||
        channelName == chromeHidden.kMessageChannel) {
      return dispatchOnRequest(portId, channelName, sender,
                               sourceExtensionId, targetExtensionId,
                               isExternal);
    }

    var connectEvent = (isExternal ?
        chrome.runtime.onConnectExternal : chrome.runtime.onConnect);
    if (connectEvent.hasListeners()) {
      var port = chromeHidden.Port.createPort(portId, channelName);
      port.sender = sender;
      if (manifestVersion < 2)
        port.tab = port.sender.tab;

      connectEvent.dispatch(port);
      return true;
    }
    return false;
  };

  // Called by native code when a channel has been closed.
  chromeHidden.Port.dispatchOnDisconnect = function(
      portId, connectionInvalid) {
    var port = ports[portId];
    if (port) {
      // Update the renderer's port bookkeeping, without notifying the browser.
      CloseChannel(portId, false);
      if (connectionInvalid) {
        var errorMsg =
            "Could not establish connection. Receiving end does not exist.";
        lastError.set(errorMsg);
        console.error("Port error: " + errorMsg);
      }
      try {
        port.onDisconnect.dispatch(port);
      } finally {
        port.destroy_();
        lastError.clear();
      }
    }
  };

  // Called by native code when a message has been sent to the given port.
  chromeHidden.Port.dispatchOnMessage = function(msg, portId) {
    var port = ports[portId];
    if (port) {
      if (msg) {
        msg = chromeHidden.JSON.parse(msg);
      }
      port.onMessage.dispatch(msg, port);
    }
  };

  // Shared implementation used by tabs.sendMessage and runtime.sendMessage.
  chromeHidden.Port.sendMessageImpl = function(port, request,
                                               responseCallback) {
    if (port.name != chromeHidden.kNativeMessageChannel)
      port.postMessage(request);

    if (port.name == chromeHidden.kMessageChannel && !responseCallback) {
      // TODO(mpcomplete): Do this for the old sendRequest API too, after
      // verifying it doesn't break anything.
      // Go ahead and disconnect immediately if the sender is not expecting
      // a response.
      port.disconnect();
      return;
    }

    // Ensure the callback exists for the older sendRequest API.
    if (!responseCallback)
      responseCallback = function() {};

    port.onDisconnect.addListener(function() {
      // For onDisconnects, we only notify the callback if there was an error
      try {
        if (chrome.runtime.lastError)
          responseCallback();
      } finally {
        port = null;
      }
    });
    port.onMessage.addListener(function(response) {
      try {
        responseCallback(response);
      } finally {
        port.disconnect();
        port = null;
      }
    });
  };

  function sendMessageUpdateArguments(functionName) {
    // Align missing (optional) function arguments with the arguments that
    // schema validation is expecting, e.g.
    //   extension.sendRequest(req)     -> extension.sendRequest(null, req)
    //   extension.sendRequest(req, cb) -> extension.sendRequest(null, req, cb)
    var args = Array.prototype.splice.call(arguments, 1);  // skip functionName
    var lastArg = args.length - 1;

    // responseCallback (last argument) is optional.
    var responseCallback = null;
    if (typeof(args[lastArg]) == 'function')
      responseCallback = args[lastArg--];

    // request (second argument) is required.
    var request = args[lastArg--];

    // targetId (first argument, extensionId in the manfiest) is optional.
    var targetId = null;
    if (lastArg >= 0)
      targetId = args[lastArg--];

    if (lastArg != -1)
      throw new Error('Invalid arguments to ' + functionName + '.');
    return [targetId, request, responseCallback];
  }

exports.sendMessageUpdateArguments = sendMessageUpdateArguments;
