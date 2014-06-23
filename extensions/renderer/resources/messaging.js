// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// chrome.runtime.messaging API implementation.

  // TODO(kalman): factor requiring chrome out of here.
  var chrome = requireNative('chrome').GetChrome();
  var Event = require('event_bindings').Event;
  var lastError = require('lastError');
  var logActivity = requireNative('activityLogger');
  var logging = requireNative('logging');
  var messagingNatives = requireNative('messaging_natives');
  var processNatives = requireNative('process');
  var unloadEvent = require('unload_event');
  var utils = require('utils');
  var messagingUtils = require('messaging_utils');

  // The reserved channel name for the sendRequest/send(Native)Message APIs.
  // Note: sendRequest is deprecated.
  var kRequestChannel = "chrome.extension.sendRequest";
  var kMessageChannel = "chrome.runtime.sendMessage";
  var kNativeMessageChannel = "chrome.runtime.sendNativeMessage";

  // Map of port IDs to port object.
  var ports = {};

  // Map of port IDs to unloadEvent listeners. Keep track of these to free the
  // unloadEvent listeners when ports are closed.
  var portReleasers = {};

  // Change even to odd and vice versa, to get the other side of a given
  // channel.
  function getOppositePortId(portId) { return portId ^ 1; }

  // Port object.  Represents a connection to another script context through
  // which messages can be passed.
  function PortImpl(portId, opt_name) {
    this.portId_ = portId;
    this.name = opt_name;

    var portSchema = {name: 'port', $ref: 'runtime.Port'};
    var options = {unmanaged: true};
    this.onDisconnect = new Event(null, [portSchema], options);
    this.onMessage = new Event(
        null,
        [{name: 'message', type: 'any', optional: true}, portSchema],
        options);
    this.onDestroy_ = null;
  }

  // Sends a message asynchronously to the context on the other end of this
  // port.
  PortImpl.prototype.postMessage = function(msg) {
    // JSON.stringify doesn't support a root object which is undefined.
    if (msg === undefined)
      msg = null;
    msg = $JSON.stringify(msg);
    if (msg === undefined) {
      // JSON.stringify can fail with unserializable objects. Log an error and
      // drop the message.
      //
      // TODO(kalman/mpcomplete): it would be better to do the same validation
      // here that we do for runtime.sendMessage (and variants), i.e. throw an
      // schema validation Error, but just maintain the old behaviour until
      // there's a good reason not to (http://crbug.com/263077).
      console.error('Illegal argument to Port.postMessage');
      return;
    }
    messagingNatives.PostMessage(this.portId_, msg);
  };

  // Disconnects the port from the other end.
  PortImpl.prototype.disconnect = function() {
    messagingNatives.CloseChannel(this.portId_, true);
    this.destroy_();
  };

  PortImpl.prototype.destroy_ = function() {
    var portId = this.portId_;

    if (this.onDestroy_)
      this.onDestroy_();
    privates(this.onDisconnect).impl.destroy_();
    privates(this.onMessage).impl.destroy_();

    messagingNatives.PortRelease(portId);
    unloadEvent.removeListener(portReleasers[portId]);

    delete ports[portId];
    delete portReleasers[portId];
  };

  // Returns true if the specified port id is in this context. This is used by
  // the C++ to avoid creating the javascript message for all the contexts that
  // don't care about a particular message.
  function hasPort(portId) {
    return portId in ports;
  };

  // Hidden port creation function.  We don't want to expose an API that lets
  // people add arbitrary port IDs to the port list.
  function createPort(portId, opt_name) {
    if (ports[portId])
      throw new Error("Port '" + portId + "' already exists.");
    var port = new Port(portId, opt_name);
    ports[portId] = port;
    portReleasers[portId] = $Function.bind(messagingNatives.PortRelease,
                                           this,
                                           portId);
    unloadEvent.addListener(portReleasers[portId]);
    messagingNatives.PortAddRef(portId);
    return port;
  };

  // Helper function for dispatchOnRequest.
  function handleSendRequestError(isSendMessage,
                                  responseCallbackPreserved,
                                  sourceExtensionId,
                                  targetExtensionId,
                                  sourceUrl) {
    var errorMsg = [];
    var eventName = isSendMessage ? "runtime.onMessage" : "extension.onRequest";
    if (isSendMessage && !responseCallbackPreserved) {
      $Array.push(errorMsg,
          "The chrome." + eventName + " listener must return true if you " +
          "want to send a response after the listener returns");
    } else {
      $Array.push(errorMsg,
          "Cannot send a response more than once per chrome." + eventName +
          " listener per document");
    }
    $Array.push(errorMsg, "(message was sent by extension" + sourceExtensionId);
    if (sourceExtensionId != "" && sourceExtensionId != targetExtensionId)
      $Array.push(errorMsg, "for extension " + targetExtensionId);
    if (sourceUrl != "")
      $Array.push(errorMsg, "for URL " + sourceUrl);
    lastError.set(eventName, errorMsg.join(" ") + ").", null, chrome);
  }

  // Helper function for dispatchOnConnect
  function dispatchOnRequest(portId, channelName, sender,
                             sourceExtensionId, targetExtensionId, sourceUrl,
                             isExternal) {
    var isSendMessage = channelName == kMessageChannel;
    var requestEvent = null;
    if (isSendMessage) {
      if (chrome.runtime) {
        requestEvent = isExternal ? chrome.runtime.onMessageExternal
                                  : chrome.runtime.onMessage;
      }
    } else {
      if (chrome.extension) {
        requestEvent = isExternal ? chrome.extension.onRequestExternal
                                  : chrome.extension.onRequest;
      }
    }
    if (!requestEvent)
      return false;
    if (!requestEvent.hasListeners())
      return false;
    var port = createPort(portId, channelName);

    function messageListener(request) {
      var responseCallbackPreserved = false;
      var responseCallback = function(response) {
        if (port) {
          port.postMessage(response);
          privates(port).impl.destroy_();
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
      messagingNatives.BindToGC(responseCallback, function() {
        if (port) {
          privates(port).impl.destroy_();
          port = null;
        }
      });
      var rv = requestEvent.dispatch(request, sender, responseCallback);
      if (isSendMessage) {
        responseCallbackPreserved =
            rv && rv.results && $Array.indexOf(rv.results, true) > -1;
        if (!responseCallbackPreserved && port) {
          // If they didn't access the response callback, they're not
          // going to send a response, so clean up the port immediately.
          privates(port).impl.destroy_();
          port = null;
        }
      }
    }

    privates(port).impl.onDestroy_ = function() {
      port.onMessage.removeListener(messageListener);
    };
    port.onMessage.addListener(messageListener);

    var eventName = isSendMessage ? "runtime.onMessage" : "extension.onRequest";
    if (isExternal)
      eventName += "External";
    logActivity.LogEvent(targetExtensionId,
                         eventName,
                         [sourceExtensionId, sourceUrl]);
    return true;
  }

  // Called by native code when a channel has been opened to this context.
  function dispatchOnConnect(portId,
                             channelName,
                             sourceTab,
                             sourceExtensionId,
                             targetExtensionId,
                             sourceUrl,
                             tlsChannelId) {
    // Only create a new Port if someone is actually listening for a connection.
    // In addition to being an optimization, this also fixes a bug where if 2
    // channels were opened to and from the same process, closing one would
    // close both.
    var extensionId = processNatives.GetExtensionId();

    // messaging_bindings.cc should ensure that this method only gets called for
    // the right extension.
    logging.CHECK(targetExtensionId == extensionId);

    if (ports[getOppositePortId(portId)])
      return false;  // this channel was opened by us, so ignore it

    // Determine whether this is coming from another extension, so we can use
    // the right event.
    var isExternal = sourceExtensionId != extensionId;

    var sender = {};
    if (sourceExtensionId != '')
      sender.id = sourceExtensionId;
    if (sourceUrl)
      sender.url = sourceUrl;
    if (sourceTab)
      sender.tab = sourceTab;
    if (tlsChannelId !== undefined)
      sender.tlsChannelId = tlsChannelId;

    // Special case for sendRequest/onRequest and sendMessage/onMessage.
    if (channelName == kRequestChannel || channelName == kMessageChannel) {
      return dispatchOnRequest(portId, channelName, sender,
                               sourceExtensionId, targetExtensionId, sourceUrl,
                               isExternal);
    }

    var connectEvent = null;
    if (chrome.runtime) {
      connectEvent = isExternal ? chrome.runtime.onConnectExternal
                                : chrome.runtime.onConnect;
    }
    if (!connectEvent)
      return false;
    if (!connectEvent.hasListeners())
      return false;

    var port = createPort(portId, channelName);
    port.sender = sender;
    if (processNatives.manifestVersion < 2)
      port.tab = port.sender.tab;

    var eventName = (isExternal ?
        "runtime.onConnectExternal" : "runtime.onConnect");
    connectEvent.dispatch(port);
    logActivity.LogEvent(targetExtensionId,
                         eventName,
                         [sourceExtensionId]);
    return true;
  };

  // Called by native code when a channel has been closed.
  function dispatchOnDisconnect(portId, errorMessage) {
    var port = ports[portId];
    if (port) {
      // Update the renderer's port bookkeeping, without notifying the browser.
      messagingNatives.CloseChannel(portId, false);
      if (errorMessage)
        lastError.set('Port', errorMessage, null, chrome);
      try {
        port.onDisconnect.dispatch(port);
      } finally {
        privates(port).impl.destroy_();
        lastError.clear(chrome);
      }
    }
  };

  // Called by native code when a message has been sent to the given port.
  function dispatchOnMessage(msg, portId) {
    var port = ports[portId];
    if (port) {
      if (msg)
        msg = $JSON.parse(msg);
      port.onMessage.dispatch(msg, port);
    }
  };

  // Shared implementation used by tabs.sendMessage and runtime.sendMessage.
  function sendMessageImpl(port, request, responseCallback) {
    if (port.name != kNativeMessageChannel)
      port.postMessage(request);

    if (port.name == kMessageChannel && !responseCallback) {
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

    // Note: make sure to manually remove the onMessage/onDisconnect listeners
    // that we added before destroying the Port, a workaround to a bug in Port
    // where any onMessage/onDisconnect listeners added but not removed will
    // be leaked when the Port is destroyed.
    // http://crbug.com/320723 tracks a sustainable fix.

    function disconnectListener() {
      // For onDisconnects, we only notify the callback if there was an error.
      if (chrome.runtime && chrome.runtime.lastError)
        responseCallback();
    }

    function messageListener(response) {
      try {
        responseCallback(response);
      } finally {
        port.disconnect();
      }
    }

    privates(port).impl.onDestroy_ = function() {
      port.onDisconnect.removeListener(disconnectListener);
      port.onMessage.removeListener(messageListener);
    };
    port.onDisconnect.addListener(disconnectListener);
    port.onMessage.addListener(messageListener);
  };

  function sendMessageUpdateArguments(functionName, hasOptionsArgument) {
    // skip functionName and hasOptionsArgument
    var args = $Array.slice(arguments, 2);
    var alignedArgs = messagingUtils.alignSendMessageArguments(args,
        hasOptionsArgument);
    if (!alignedArgs)
      throw new Error('Invalid arguments to ' + functionName + '.');
    return alignedArgs;
  }

var Port = utils.expose('Port', PortImpl, { functions: [
    'disconnect',
    'postMessage'
  ],
  properties: [
    'name',
    'onDisconnect',
    'onMessage'
  ] });

exports.kRequestChannel = kRequestChannel;
exports.kMessageChannel = kMessageChannel;
exports.kNativeMessageChannel = kNativeMessageChannel;
exports.Port = Port;
exports.createPort = createPort;
exports.sendMessageImpl = sendMessageImpl;
exports.sendMessageUpdateArguments = sendMessageUpdateArguments;

// For C++ code to call.
exports.hasPort = hasPort;
exports.dispatchOnConnect = dispatchOnConnect;
exports.dispatchOnDisconnect = dispatchOnDisconnect;
exports.dispatchOnMessage = dispatchOnMessage;
