// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This script contains privileged chrome extension related javascript APIs.
// It is loaded by pages whose URL has the chrome-extension protocol.

var chrome = chrome || {};
(function() {
  native function GetExtensionAPIDefinition();
  native function StartRequest();
  native function GetChromeHidden();
  native function GetNextRequestId();
  native function Print();

  native function GetCurrentPageActions(extensionId);
  native function GetExtensionViews();
  native function GetNextContextMenuId();
  native function GetNextTtsEventId();
  native function OpenChannelToTab();
  native function GetRenderViewId();
  native function SetIconCommon();
  native function GetUniqueSubEventName(eventName);
  native function GetLocalFileSystem(name, path);
  native function DecodeJPEG(jpegImage);
  native function CreateBlob(filePath);
  native function SendResponseAck(requestId);

  var chromeHidden = GetChromeHidden();

  if (!chrome)
    chrome = {};

  function forEach(dict, f) {
    for (key in dict) {
      if (dict.hasOwnProperty(key))
        f(key, dict[key]);
    }
  }

  // Validate arguments.
  chromeHidden.validationTypes = [];
  chromeHidden.validate = function(args, schemas) {
    if (args.length > schemas.length)
      throw new Error("Too many arguments.");

    for (var i = 0; i < schemas.length; i++) {
      if (i in args && args[i] !== null && args[i] !== undefined) {
        var validator = new chromeHidden.JSONSchemaValidator();
        validator.addTypes(chromeHidden.validationTypes);
        validator.validate(args[i], schemas[i]);
        if (validator.errors.length == 0)
          continue;

        var message = "Invalid value for argument " + (i + 1) + ". ";
        for (var i = 0, err; err = validator.errors[i]; i++) {
          if (err.path) {
            message += "Property '" + err.path + "': ";
          }
          message += err.message;
          message = message.substring(0, message.length - 1);
          message += ", ";
        }
        message = message.substring(0, message.length - 2);
        message += ".";

        throw new Error(message);
      } else if (!schemas[i].optional) {
        throw new Error("Parameter " + (i + 1) + " is required.");
      }
    }
  };

  // Callback handling.
  var requests = [];
  chromeHidden.handleResponse = function(requestId, name,
                                         success, response, error) {
    try {
      var request = requests[requestId];
      if (success) {
        delete chrome.extension.lastError;
      } else {
        if (!error) {
          error = "Unknown error.";
        }
        console.error("Error during " + name + ": " + error);
        chrome.extension.lastError = {
          "message": error
        };
      }

      if (request.customCallback) {
        request.customCallback(name, request, response);
      }

      if (request.callback) {
        // Callbacks currently only support one callback argument.
        var callbackArgs = response ? [chromeHidden.JSON.parse(response)] : [];

        // Validate callback in debug only -- and only when the
        // caller has provided a callback. Implementations of api
        // calls my not return data if they observe the caller
        // has not provided a callback.
        if (chromeHidden.validateCallbacks && !error) {
          try {
            if (!request.callbackSchema.parameters) {
              throw "No callback schemas defined";
            }

            if (request.callbackSchema.parameters.length > 1) {
              throw "Callbacks may only define one parameter";
            }

            chromeHidden.validate(callbackArgs,
                request.callbackSchema.parameters);
          } catch (exception) {
            return "Callback validation error during " + name + " -- " +
                   exception.stack;
          }
        }

        if (response) {
          request.callback(callbackArgs[0]);
        } else {
          request.callback();
        }
      }
    } finally {
      delete requests[requestId];
      delete chrome.extension.lastError;
    }

    return undefined;
  };

  function prepareRequest(args, argSchemas) {
    var request = {};
    var argCount = args.length;

    // Look for callback param.
    if (argSchemas.length > 0 &&
        args.length == argSchemas.length &&
        argSchemas[argSchemas.length - 1].type == "function") {
      request.callback = args[argSchemas.length - 1];
      request.callbackSchema = argSchemas[argSchemas.length - 1];
      --argCount;
    }

    request.args = [];
    for (var k = 0; k < argCount; k++) {
      request.args[k] = args[k];
    }

    return request;
  }

  // Send an API request and optionally register a callback.
  // |opt_args| is an object with optional parameters as follows:
  // - noStringify: true if we should not stringify the request arguments.
  // - customCallback: a callback that should be called instead of the standard
  //   callback.
  // - nativeFunction: the v8 native function to handle the request, or
  //   StartRequest if missing.
  // - forIOThread: true if this function should be handled on the browser IO
  //   thread.
  function sendRequest(functionName, args, argSchemas, opt_args) {
    if (!opt_args)
      opt_args = {};
    var request = prepareRequest(args, argSchemas);
    if (opt_args.customCallback) {
      request.customCallback = opt_args.customCallback;
    }
    // JSON.stringify doesn't support a root object which is undefined.
    if (request.args === undefined)
      request.args = null;

    var sargs = opt_args.noStringify ?
        request.args : chromeHidden.JSON.stringify(request.args);
    var nativeFunction = opt_args.nativeFunction || StartRequest;

    var requestId = GetNextRequestId();
    request.id = requestId;
    requests[requestId] = request;
    var hasCallback =
        (request.callback || opt_args.customCallback) ? true : false;
    return nativeFunction(functionName, sargs, requestId, hasCallback,
                          opt_args.forIOThread);
  }

  // --- Setup additional api's not currently handled in common/extensions/api

  // WebRequestEvent object. This is used for special webRequest events with
  // extra parameters. Each invocation of addListener creates a new named
  // sub-event. That sub-event is associated with the extra parameters in the
  // browser process, so that only it is dispatched when the main event occurs
  // matching the extra parameters.
  //
  // Example:
  //   chrome.webRequest.onBeforeRequest.addListener(
  //       callback, {urls: "http://*.google.com/*"});
  //   ^ callback will only be called for onBeforeRequests matching the filter.
  chrome.WebRequestEvent =
     function(eventName, opt_argSchemas, opt_extraArgSchemas) {
    if (typeof eventName != "string")
      throw new Error("chrome.WebRequestEvent requires an event name.");

    this.eventName_ = eventName;
    this.argSchemas_ = opt_argSchemas;
    this.extraArgSchemas_ = opt_extraArgSchemas;
    this.subEvents_ = [];
  };

  // Test if the given callback is registered for this event.
  chrome.WebRequestEvent.prototype.hasListener = function(cb) {
    return this.findListener_(cb) > -1;
  };

  // Test if any callbacks are registered fur thus event.
  chrome.WebRequestEvent.prototype.hasListeners = function(cb) {
    return this.subEvents_.length > 0;
  };

  // Registers a callback to be called when this event is dispatched. If
  // opt_filter is specified, then the callback is only called for events that
  // match the given filters. If opt_extraInfo is specified, the given optional
  // info is sent to the callback.
  chrome.WebRequestEvent.prototype.addListener =
      function(cb, opt_filter, opt_extraInfo) {
    var subEventName = GetUniqueSubEventName(this.eventName_);
    // Note: this could fail to validate, in which case we would not add the
    // subEvent listener.
    chromeHidden.validate(Array.prototype.slice.call(arguments, 1),
                          this.extraArgSchemas_);
    chrome.experimental.webRequest.addEventListener(
        cb, opt_filter, opt_extraInfo, this.eventName_, subEventName);

    var subEvent = new chrome.Event(subEventName, this.argSchemas_);
    var subEventCallback = cb;
    if (opt_extraInfo && opt_extraInfo.indexOf("blocking") >= 0) {
      var eventName = this.eventName_;
      subEventCallback = function() {
        var requestId = arguments[0].requestId;
        try {
          var result = cb.apply(null, arguments);
          chrome.experimental.webRequest.eventHandled(
              eventName, subEventName, requestId, result);
        } catch (e) {
          chrome.experimental.webRequest.eventHandled(
              eventName, subEventName, requestId);
          throw e;
        }
      };
    } else if (opt_extraInfo && opt_extraInfo.indexOf("asyncBlocking") >= 0) {
      var eventName = this.eventName_;
      subEventCallback = function() {
        var details = arguments[0];
        var requestId = details.requestId;
        var handledCallback = function(response) {
          chrome.experimental.webRequest.eventHandled(
              eventName, subEventName, requestId, response);
        };
        cb.apply(null, [details, handledCallback]);
      };
    }
    this.subEvents_.push(
        {subEvent: subEvent, callback: cb, subEventCallback: subEventCallback});
    subEvent.addListener(subEventCallback);
  };

  // Unregisters a callback.
  chrome.WebRequestEvent.prototype.removeListener = function(cb) {
    var idx = this.findListener_(cb);
    if (idx < 0) {
      return;
    }

    var e = this.subEvents_[idx];
    e.subEvent.removeListener(e.subEventCallback);
    if (e.subEvent.hasListeners()) {
      console.error(
          "Internal error: webRequest subEvent has orphaned listeners.");
    }
    this.subEvents_.splice(idx, 1);
  };

  chrome.WebRequestEvent.prototype.findListener_ = function(cb) {
    for (var i in this.subEvents_) {
      var e = this.subEvents_[i];
      if (e.callback === cb) {
        if (e.subEvent.findListener_(e.subEventCallback) > -1)
          return i;
        console.error("Internal error: webRequest subEvent has no callback.");
      }
    }

    return -1;
  };

  function CustomBindingsObject() {
  }
  CustomBindingsObject.prototype.setSchema = function(schema) {
    // The functions in the schema are in list form, so we move them into a
    // dictionary for easier access.
    var self = this;
    self.parameters = {};
    schema.functions.forEach(function(f) {
      self.parameters[f.name] = f.parameters;
    });
  };

  function extendSchema(schema) {
    var extendedSchema = schema.slice();
    extendedSchema.unshift({'type': 'string'});
    return extendedSchema;
  }

  var customBindings = {};

  function setupChromeSetting() {
    function ChromeSetting(prefKey, valueSchema) {
      this.get = function(details, callback) {
        var getSchema = this.parameters.get;
        chromeHidden.validate([details, callback], getSchema);
        return sendRequest('types.ChromeSetting.get',
                           [prefKey, details, callback],
                           extendSchema(getSchema));
      };
      this.set = function(details, callback) {
        var setSchema = this.parameters.set.slice();
        setSchema[0].properties.value = valueSchema;
        chromeHidden.validate([details, callback], setSchema);
        return sendRequest('types.ChromeSetting.set',
                           [prefKey, details, callback],
                           extendSchema(setSchema));
      };
      this.clear = function(details, callback) {
        var clearSchema = this.parameters.clear;
        chromeHidden.validate([details, callback], clearSchema);
        return sendRequest('types.ChromeSetting.clear',
                           [prefKey, details, callback],
                           extendSchema(clearSchema));
      };
      this.onChange = new chrome.Event('types.ChromeSetting.' + prefKey +
                                       '.onChange');
    };
    ChromeSetting.prototype = new CustomBindingsObject();
    customBindings['ChromeSetting'] = ChromeSetting;
  }

  function setupContentSetting() {
    function ContentSetting(contentType, settingSchema) {
      this.get = function(details, callback) {
        var getSchema = this.parameters.get;
        chromeHidden.validate([details, callback], getSchema);
        return sendRequest('contentSettings.get',
                           [contentType, details, callback],
                           extendSchema(getSchema));
      };
      this.set = function(details, callback) {
        var setSchema = this.parameters.set.slice();
        setSchema[0].properties.setting = settingSchema;
        chromeHidden.validate([details, callback], setSchema);
        return sendRequest('contentSettings.set',
                           [contentType, details, callback],
                           extendSchema(setSchema));
      };
      this.clear = function(details, callback) {
        var clearSchema = this.parameters.clear;
        chromeHidden.validate([details, callback], clearSchema);
        return sendRequest('contentSettings.clear',
                           [contentType, details, callback],
                           extendSchema(clearSchema));
      };
      this.getResourceIdentifiers = function(callback) {
        var schema = this.parameters.getResourceIdentifiers;
        chromeHidden.validate([callback], schema);
        return sendRequest(
            'contentSettings.getResourceIdentifiers',
            [contentType, callback],
            extendSchema(schema));
      };
    }
    ContentSetting.prototype = new CustomBindingsObject();
    customBindings['ContentSetting'] = ContentSetting;
  }

  function setupStorageNamespace() {
    function StorageNamespace(namespace, schema) {
      // Binds an API function for a namespace to its browser-side call, e.g.
      // experimental.storage.sync.get('foo') -> (binds to) ->
      // experimental.storage.get('sync', 'foo').
      //
      // TODO(kalman): Put as a method on CustomBindingsObject and re-use (or
      // even generate) for other APIs that need to do this.
      function bindApiFunction(functionName) {
        this[functionName] = function() {
          var schema = this.parameters[functionName];
          chromeHidden.validate(arguments, schema);
          return sendRequest(
              'experimental.storage.' + functionName,
              [namespace].concat(Array.prototype.slice.call(arguments)),
              extendSchema(schema));
        };
      }
      ['get', 'set', 'remove', 'clear'].forEach(bindApiFunction.bind(this));
    }
    StorageNamespace.prototype = new CustomBindingsObject();
    customBindings['StorageNamespace'] = StorageNamespace;
  }
  function setupInputEvents() {
    chrome.experimental.input.ime.onKeyEvent.dispatch =
        function(engineID, keyData) {
      var args = Array.prototype.slice.call(arguments);
      if (this.validate_) {
        var validationErrors = this.validate_(args);
        if (validationErrors) {
          chrome.experimental.input.ime.eventHandled(requestId, false);
          return validationErrors;
        }
      }
      if (this.listeners_.length > 1) {
        console.error("Too many listeners for 'onKeyEvent': " + e.stack);
        chrome.experimental.input.ime.eventHandled(requestId, false);
        return;
      }
      for (var i = 0; i < this.listeners_.length; i++) {
        try {
          var requestId = keyData.requestId;
          var result = this.listeners_[i].apply(null, args);
          chrome.experimental.input.ime.eventHandled(requestId, result);
        } catch (e) {
          console.error("Error in event handler for 'onKeyEvent': " + e.stack);
          chrome.experimental.input.ime.eventHandled(requestId, false);
        }
      }
    };
  }

  // Page action events send (pageActionId, {tabId, tabUrl}).
  function setupPageActionEvents(extensionId) {
    var pageActions = GetCurrentPageActions(extensionId);

    var oldStyleEventName = "pageActions";
    // TODO(EXTENSIONS_DEPRECATED): only one page action
    for (var i = 0; i < pageActions.length; ++i) {
      // Setup events for each extension_id/page_action_id string we find.
      chrome.pageActions[pageActions[i]] = new chrome.Event(oldStyleEventName);
    }
  }

  function setupHiddenContextMenuEvent(extensionId) {
    chromeHidden.contextMenus = {};
    chromeHidden.contextMenus.handlers = {};
    var eventName = "contextMenus";
    chromeHidden.contextMenus.event = new chrome.Event(eventName);
    chromeHidden.contextMenus.ensureListenerSetup = function() {
      if (chromeHidden.contextMenus.listening) {
        return;
      }
      chromeHidden.contextMenus.listening = true;
      chromeHidden.contextMenus.event.addListener(function() {
        // An extension context menu item has been clicked on - fire the onclick
        // if there is one.
        var id = arguments[0].menuItemId;
        var onclick = chromeHidden.contextMenus.handlers[id];
        if (onclick) {
          onclick.apply(null, arguments);
        }
      });
    };
  }

  // Remove invalid characters from |text| so that it is suitable to use
  // for |AutocompleteMatch::contents|.
  function sanitizeString(text) {
    // NOTE: This logic mirrors |AutocompleteMatch::SanitizeString()|.
    // 0x2028 = line separator; 0x2029 = paragraph separator.
    var kRemoveChars = /(\r|\n|\t|\u2028|\u2029)/gm;
    return text.trimLeft().replace(kRemoveChars, '');
  }

  // Parses the xml syntax supported by omnibox suggestion results. Returns an
  // object with two properties: 'description', which is just the text content,
  // and 'descriptionStyles', which is an array of style objects in a format
  // understood by the C++ backend.
  function parseOmniboxDescription(input) {
    var domParser = new DOMParser();

    // The XML parser requires a single top-level element, but we want to
    // support things like 'hello, <match>world</match>!'. So we wrap the
    // provided text in generated root level element.
    var root = domParser.parseFromString(
        '<fragment>' + input + '</fragment>', 'text/xml');

    // DOMParser has a terrible error reporting facility. Errors come out nested
    // inside the returned document.
    var error = root.querySelector('parsererror div');
    if (error) {
      throw new Error(error.textContent);
    }

    // Otherwise, it's valid, so build up the result.
    var result = {
      description: '',
      descriptionStyles: []
    };

    // Recursively walk the tree.
    (function(node) {
      for (var i = 0, child; child = node.childNodes[i]; i++) {
        // Append text nodes to our description.
        if (child.nodeType == Node.TEXT_NODE) {
          result.description += sanitizeString(child.nodeValue);
          continue;
        }

        // Process and descend into a subset of recognized tags.
        if (child.nodeType == Node.ELEMENT_NODE &&
            (child.nodeName == 'dim' || child.nodeName == 'match' ||
             child.nodeName == 'url')) {
          var style = {
            'type': child.nodeName,
            'offset': result.description.length
          };
          result.descriptionStyles.push(style);
          arguments.callee(child);
          style.length = result.description.length - style.offset;
          continue;
        }

        // Descend into all other nodes, even if they are unrecognized, for
        // forward compat.
        arguments.callee(child);
      }
    })(root);

    return result;
  }

  function setupOmniboxEvents() {
    chrome.omnibox.onInputChanged.dispatch =
        function(text, requestId) {
      var suggestCallback = function(suggestions) {
        chrome.omnibox.sendSuggestions(requestId, suggestions);
      };
      chrome.Event.prototype.dispatch.apply(this, [text, suggestCallback]);
    };
  }

  function setupTtsEvents() {
    chromeHidden.tts = {};
    chromeHidden.tts.handlers = {};
    chrome.ttsEngine.onSpeak.dispatch =
        function(text, options, requestId) {
          var sendTtsEvent = function(event) {
            chrome.ttsEngine.sendTtsEvent(requestId, event);
          };
          chrome.Event.prototype.dispatch.apply(
              this, [text, options, sendTtsEvent]);
        };
    try {
      chrome.tts.onEvent.addListener(
          function(event) {
            var eventHandler = chromeHidden.tts.handlers[event.srcId];
            if (eventHandler) {
              eventHandler({
                             type: event.type,
                             charIndex: event.charIndex,
                             errorMessage: event.errorMessage
                           });
              if (event.isFinalEvent) {
                delete chromeHidden.tts.handlers[event.srcId];
              }
            }
          });
      } catch (e) {
        // This extension doesn't have permission to access TTS, so we
        // can safely ignore this.
      }
  }

  // Get the platform from navigator.appVersion.
  function getPlatform() {
    var platforms = [
      [/CrOS Touch/, "chromeos touch"],
      [/CrOS/, "chromeos"],
      [/Linux/, "linux"],
      [/Mac/, "mac"],
      [/Win/, "win"],
    ];

    for (var i = 0; i < platforms.length; i++) {
      if (platforms[i][0].test(navigator.appVersion)) {
        return platforms[i][1];
      }
    }
    return "unknown";
  }

  chromeHidden.onLoad.addListener(function(extensionId, isExtensionProcess,
                                           isIncognitoProcess) {
    // Setup the ChromeSetting class so we can use it to construct
    // ChromeSetting objects from the API definition.
    setupChromeSetting();

    // Ditto ContentSetting.
    setupContentSetting();

    // Ditto StorageNamespace.
    setupStorageNamespace();

    // |apiFunctions| is a hash of name -> object that stores the
    // name & definition of the apiFunction. Custom handling of api functions
    // is implemented by adding a "handleRequest" function to the object.
    var apiFunctions = {};

    // Read api definitions and setup api functions in the chrome namespace.
    // TODO(rafaelw): Consider defining a json schema for an api definition
    //   and validating either here, in a unit_test or both.
    // TODO(rafaelw): Handle synchronous functions.
    // TODO(rafaelw): Consider providing some convenient override points
    //   for api functions that wish to insert themselves into the call.
    var apiDefinitions = GetExtensionAPIDefinition();
    var platform = getPlatform();

    apiDefinitions.forEach(function(apiDef) {
      // Check platform, if apiDef has platforms key.
      if (apiDef.platforms && apiDef.platforms.indexOf(platform) == -1) {
        return;
      }

      var module = chrome;
      var namespaces = apiDef.namespace.split('.');
      for (var index = 0, name; name = namespaces[index]; index++) {
        module[name] = module[name] || {};
        module = module[name];
      }

      // Add types to global validationTypes
      if (apiDef.types) {
        apiDef.types.forEach(function(t) {
          chromeHidden.validationTypes.push(t);
          if (t.type == 'object' && customBindings[t.id]) {
            customBindings[t.id].prototype.setSchema(t);
          }
        });
      }

      // Adds a getter that throws an access denied error to object |module|
      // for property |name| described by |schemaNode| if necessary.
      //
      // Returns true if the getter was necessary (access is disallowed), or
      // false otherwise.
      function addUnprivilegedAccessGetter(module, name, allowUnprivileged) {
        if (allowUnprivileged || isExtensionProcess)
          return false;

        module.__defineGetter__(name, function() {
          throw new Error(
              '"' + name + '" can only be used in extension processes. See ' +
              'the content scripts documentation for more details.');
        });
        return true;
      }

      // Setup Functions.
      if (apiDef.functions) {
        apiDef.functions.forEach(function(functionDef) {
          if (functionDef.name in module ||
              addUnprivilegedAccessGetter(module, functionDef.name,
                                          functionDef.unprivileged)) {
            return;
          }

          var apiFunction = {};
          apiFunction.definition = functionDef;
          apiFunction.name = apiDef.namespace + "." + functionDef.name;
          apiFunctions[apiFunction.name] = apiFunction;

          module[functionDef.name] = (function() {
            var args = arguments;
            if (this.updateArgumentsPreValidate)
              args = this.updateArgumentsPreValidate.apply(this, args);
            chromeHidden.validate(args, this.definition.parameters);
            if (this.updateArgumentsPostValidate)
              args = this.updateArgumentsPostValidate.apply(this, args);

            var retval;
            if (this.handleRequest) {
              retval = this.handleRequest.apply(this, args);
            } else {
              retval = sendRequest(this.name, args,
                                   this.definition.parameters,
                                   {customCallback: this.customCallback});
            }

            // Validate return value if defined - only in debug.
            if (chromeHidden.validateCallbacks &&
                chromeHidden.validate &&
                this.definition.returns) {
              chromeHidden.validate([retval], [this.definition.returns]);
            }
            return retval;
          }).bind(apiFunction);
        });
      }

      // Setup Events
      if (apiDef.events) {
        apiDef.events.forEach(function(eventDef) {
          if (eventDef.name in module ||
              addUnprivilegedAccessGetter(module, eventDef.name,
                                          eventDef.unprivileged)) {
            return;
          }

          var eventName = apiDef.namespace + "." + eventDef.name;
          if (apiDef.namespace == "experimental.webRequest") {
            module[eventDef.name] = new chrome.WebRequestEvent(eventName,
                eventDef.parameters, eventDef.extraParameters);
          } else {
            module[eventDef.name] = new chrome.Event(eventName,
                eventDef.parameters);
          }
        });
      }

      function addProperties(m, def) {
        // Parse any values defined for properties.
        if (def.properties) {
          forEach(def.properties, function(prop, property) {
            if (prop in m ||
                addUnprivilegedAccessGetter(m, prop, property.unprivileged)) {
              return;
            }

            var value = property.value;
            if (value) {
              if (property.type === 'integer') {
                value = parseInt(value);
              } else if (property.type === 'boolean') {
                value = value === "true";
              } else if (property["$ref"]) {
                var constructor = customBindings[property["$ref"]];
                var args = value;
                // For an object property, |value| is an array of constructor
                // arguments, but we want to pass the arguments directly
                // (i.e. not as an array), so we have to fake calling |new| on
                // the constructor.
                value = { __proto__: constructor.prototype };
                constructor.apply(value, args);
              } else if (property.type === 'object') {
                // Recursively add properties.
                addProperties(value, property);
              } else if (property.type !== 'string') {
                throw "NOT IMPLEMENTED (extension_api.json error): Cannot " +
                    "parse values for type \"" + property.type + "\"";
              }
            }
            if (value) {
              m[prop] = value;
            }
          });
        }
      }

      addProperties(module, apiDef);
    });

    // TODO(aa): The rest of the crap below this really needs to be factored out
    // with a clean API boundary. Right now it is too soupy for me to feel
    // comfortable running in content scripts. What if people are just
    // overwriting random APIs? That would bypass our content script access
    // checks.
    if (!isExtensionProcess)
      return;

    // getTabContentses is retained for backwards compatibility
    // See http://crbug.com/21433
    chrome.extension.getTabContentses = chrome.extension.getExtensionTabs;
    // TOOD(mihaip): remove this alias once the webstore stops calling
    // beginInstallWithManifest2.
    // See http://crbug.com/100242
    chrome.webstorePrivate.beginInstallWithManifest2 =
        chrome.webstorePrivate.beginInstallWithManifest3;

    apiFunctions["tabs.connect"].handleRequest = function(tabId, connectInfo) {
      var name = "";
      if (connectInfo) {
        name = connectInfo.name || name;
      }
      var portId = OpenChannelToTab(tabId, chromeHidden.extensionId, name);
      return chromeHidden.Port.createPort(portId, name);
    };

    apiFunctions["tabs.sendRequest"].handleRequest =
        function(tabId, request, responseCallback) {
      var port = chrome.tabs.connect(tabId,
                                     {name: chromeHidden.kRequestChannel});
      port.postMessage(request);
      port.onDisconnect.addListener(function() {
        // For onDisconnects, we only notify the callback if there was an error.
        if (chrome.extension.lastError && responseCallback)
          responseCallback();
      });
      port.onMessage.addListener(function(response) {
        try {
          if (responseCallback)
            responseCallback(response);
        } finally {
          port.disconnect();
          port = null;
        }
      });
    };

    apiFunctions["pageCapture.saveAsMHTML"].customCallback =
      function(name, request, response) {
        var params = chromeHidden.JSON.parse(response);
        var path = params.mhtmlFilePath;
        var size = params.mhtmlFileLength;

        if (request.callback)
          request.callback(CreateBlob(path, size));
        request.callback = null;

        // Notify the browser. Now that the blob is referenced from JavaScript,
        // the browser can drop its reference to it.
        SendResponseAck(request.id);
      };

    apiFunctions["fileBrowserPrivate.requestLocalFileSystem"].customCallback =
      function(name, request, response) {
        var resp = response ? [chromeHidden.JSON.parse(response)] : [];
        var fs = null;
        if (!resp[0].error)
          fs = GetLocalFileSystem(resp[0].name, resp[0].path);
        if (request.callback)
          request.callback(fs);
        request.callback = null;
      };

    apiFunctions["chromePrivate.decodeJPEG"].handleRequest =
      function(jpeg_image) {
        return DecodeJPEG(jpeg_image);
      };

    apiFunctions["extension.getViews"].handleRequest = function(properties) {
      var windowId = -1;
      var type = "ALL";
      if (typeof(properties) != "undefined") {
        if (typeof(properties.type) != "undefined") {
          type = properties.type;
        }
        if (typeof(properties.windowId) != "undefined") {
          windowId = properties.windowId;
        }
      }
      return GetExtensionViews(windowId, type) || null;
    };

    apiFunctions["extension.getBackgroundPage"].handleRequest = function() {
      return GetExtensionViews(-1, "BACKGROUND")[0] || null;
    };

    apiFunctions["extension.getExtensionTabs"].handleRequest =
        function(windowId) {
      if (typeof(windowId) == "undefined")
        windowId = -1;
      return GetExtensionViews(windowId, "TAB");
    };

    apiFunctions["devtools.getTabEvents"].handleRequest = function(tabId) {
      var tabIdProxy = {};
      var functions = ["onPageEvent", "onTabClose"];
      functions.forEach(function(name) {
        // Event disambiguation is handled by name munging.  See
        // chrome/browser/extensions/extension_devtools_events.h for the C++
        // equivalent of this logic.
        tabIdProxy[name] = new chrome.Event("devtools." + tabId + "." + name);
      });
      return tabIdProxy;
    };

    var canvas;
    function setIconCommon(details, name, parameters, actionType, iconSize,
                           nativeFunction) {
      if ("iconIndex" in details) {
        sendRequest(name, [details], parameters);
      } else if ("imageData" in details) {
        // Verify that this at least looks like an ImageData element.
        // Unfortunately, we cannot use instanceof because the ImageData
        // constructor is not public.
        //
        // We do this manually instead of using JSONSchema to avoid having these
        // properties show up in the doc.
        if (!("width" in details.imageData) ||
            !("height" in details.imageData) ||
            !("data" in details.imageData)) {
          throw new Error(
              "The imageData property must contain an ImageData object.");
        }

        if (details.imageData.width > iconSize ||
            details.imageData.height > iconSize) {
          throw new Error(
              "The imageData property must contain an ImageData object that " +
              "is no larger than " + iconSize + " pixels square.");
        }

        sendRequest(name, [details], parameters,
                    {noStringify: true, nativeFunction: nativeFunction});
      } else if ("path" in details) {
        var img = new Image();
        img.onerror = function() {
          console.error("Could not load " + actionType + " icon '" +
                        details.path + "'.");
        };
        img.onload = function() {
          var canvas = document.createElement("canvas");
          canvas.width = img.width > iconSize ? iconSize : img.width;
          canvas.height = img.height > iconSize ? iconSize : img.height;

          var canvas_context = canvas.getContext('2d');
          canvas_context.clearRect(0, 0, canvas.width, canvas.height);
          canvas_context.drawImage(img, 0, 0, canvas.width, canvas.height);
          delete details.path;
          details.imageData = canvas_context.getImageData(0, 0, canvas.width,
                                                          canvas.height);
          sendRequest(name, [details], parameters,
                      {noStringify: true, nativeFunction: nativeFunction});
        };
        img.src = details.path;
      } else {
        throw new Error(
            "Either the path or imageData property must be specified.");
      }
    }

    function setExtensionActionIconCommon(details, name, parameters,
                                          actionType) {
      var EXTENSION_ACTION_ICON_SIZE = 19;
      setIconCommon(details, name, parameters, actionType,
                    EXTENSION_ACTION_ICON_SIZE, SetIconCommon);
    }

    apiFunctions["browserAction.setIcon"].handleRequest = function(details) {
      setExtensionActionIconCommon(
          details, this.name, this.definition.parameters, "browser action");
    };

    apiFunctions["pageAction.setIcon"].handleRequest = function(details) {
      setExtensionActionIconCommon(
          details, this.name, this.definition.parameters, "page action");
    };

    apiFunctions["experimental.sidebar.setIcon"].handleRequest =
        function(details) {
      var SIDEBAR_ICON_SIZE = 16;
      setIconCommon(
          details, this.name, this.definition.parameters, "sidebar",
          SIDEBAR_ICON_SIZE, SetIconCommon);
    };

    apiFunctions["contextMenus.create"].handleRequest =
        function() {
      var args = arguments;
      var id = GetNextContextMenuId();
      args[0].generatedId = id;
      sendRequest(this.name, args, this.definition.parameters,
                  {customCallback: this.customCallback});
      return id;
    };

    apiFunctions["omnibox.setDefaultSuggestion"].handleRequest =
        function(details) {
      var parseResult = parseOmniboxDescription(details.description);
      sendRequest(this.name, [parseResult], this.definition.parameters);
    };

    apiFunctions["experimental.webRequest.addEventListener"].handleRequest =
        function() {
      var args = Array.prototype.slice.call(arguments);
      sendRequest(this.name, args, this.definition.parameters,
                  {forIOThread: true});
    };

    apiFunctions["experimental.webRequest.eventHandled"].handleRequest =
        function() {
      var args = Array.prototype.slice.call(arguments);
      sendRequest(this.name, args, this.definition.parameters,
                  {forIOThread: true});
    };

    apiFunctions["experimental.webRequest.handlerBehaviorChanged"].
        handleRequest = function() {
      var args = Array.prototype.slice.call(arguments);
      sendRequest(this.name, args, this.definition.parameters,
                  {forIOThread: true});
    };

    apiFunctions["contextMenus.create"].customCallback =
        function(name, request, response) {
      if (chrome.extension.lastError) {
        return;
      }

      var id = request.args[0].generatedId;

      // Set up the onclick handler if we were passed one in the request.
      var onclick = request.args.length ? request.args[0].onclick : null;
      if (onclick) {
        chromeHidden.contextMenus.ensureListenerSetup();
        chromeHidden.contextMenus.handlers[id] = onclick;
      }
    };

    apiFunctions["contextMenus.remove"].customCallback =
        function(name, request, response) {
      if (chrome.extension.lastError) {
        return;
      }
      var id = request.args[0];
      delete chromeHidden.contextMenus.handlers[id];
    };

    apiFunctions["contextMenus.update"].customCallback =
        function(name, request, response) {
      if (chrome.extension.lastError) {
        return;
      }
      var id = request.args[0];
      if (request.args[1].onclick) {
        chromeHidden.contextMenus.handlers[id] = request.args[1].onclick;
      }
    };

    apiFunctions["contextMenus.removeAll"].customCallback =
        function(name, request, response) {
      if (chrome.extension.lastError) {
        return;
      }
      chromeHidden.contextMenus.handlers = {};
    };

    apiFunctions["tabs.captureVisibleTab"].updateArgumentsPreValidate =
        function() {
      // Old signature:
      //    captureVisibleTab(int windowId, function callback);
      // New signature:
      //    captureVisibleTab(int windowId, object details, function callback);
      //
      // TODO(skerner): The next step to omitting optional arguments is the
      // replacement of this code with code that matches arguments by type.
      // Once this is working for captureVisibleTab() it can be enabled for
      // the rest of the API. See crbug/29215 .
      if (arguments.length == 2 && typeof(arguments[1]) == "function") {
        // If the old signature is used, add a null details object.
        newArgs = [arguments[0], null, arguments[1]];
      } else {
        newArgs = arguments;
      }
      return newArgs;
    };

    apiFunctions["omnibox.sendSuggestions"].updateArgumentsPostValidate =
        function(requestId, userSuggestions) {
      var suggestions = [];
      for (var i = 0; i < userSuggestions.length; i++) {
        var parseResult = parseOmniboxDescription(
            userSuggestions[i].description);
        parseResult.content = userSuggestions[i].content;
        suggestions.push(parseResult);
      }
      return [requestId, suggestions];
    };

    apiFunctions["tts.speak"].handleRequest = function() {
      var args = arguments;
      if (args.length > 1 && args[1] && args[1].onEvent) {
        var id = GetNextTtsEventId();
        args[1].srcId = id;
        chromeHidden.tts.handlers[id] = args[1].onEvent;
      }
      sendRequest(this.name, args, this.definition.parameters);
      return id;
    };

    if (chrome.test) {
      chrome.test.getApiDefinitions = GetExtensionAPIDefinition;
    }

    setupPageActionEvents(extensionId);
    setupHiddenContextMenuEvent(extensionId);
    setupInputEvents();
    setupOmniboxEvents();
    setupTtsEvents();
  });

  if (!chrome.experimental)
    chrome.experimental = {};

  if (!chrome.experimental.accessibility)
    chrome.experimental.accessibility = {};

  if (!chrome.experimental.speechInput)
    chrome.experimental.speechInput = {};

  if (!chrome.tts)
    chrome.tts = {};

  if (!chrome.ttsEngine)
    chrome.ttsEngine = {};

  if (!chrome.experimental.downloads)
    chrome.experimental.downloads = {};
})();
