// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This script contains privileged chrome extension related javascript APIs.
// It is loaded by pages whose URL has the chrome-extension protocol.

var chrome = chrome || {};
(function() {
  native function GetExtensionAPIDefinition();
  native function StartRequest();
  native function GetCurrentPageActions(extensionId);
  native function GetExtensionViews();
  native function GetChromeHidden();
  native function GetNextRequestId();
  native function OpenChannelToTab();
  native function GetRenderViewId();
  native function SetIconCommon();
  native function IsExtensionProcess();
  native function IsIncognitoProcess();
  native function GetUniqueSubEventName(eventName);

  var chromeHidden = GetChromeHidden();

  // These bindings are for the extension process only. Since a chrome-extension
  // URL can be loaded in an iframe of a regular renderer, we check here to
  // ensure we don't expose the APIs in that case.
  if (!IsExtensionProcess()) {
    chromeHidden.onLoad.addListener(function (extensionId) {
      if (!extensionId) {
        return;
      }
      chrome.initExtension(extensionId, false);
    });
    return;
  }

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
                   exception;
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

  chromeHidden.setViewType = function(type) {
    var modeClass = "chrome-" + type;
    var className = document.documentElement.className;
    if (className && className.length) {
      var classes = className.split(" ");
      var new_classes = [];
      classes.forEach(function(cls) {
        if (cls.indexOf("chrome-") != 0) {
          new_classes.push(cls);
        }
      });
      new_classes.push(modeClass);
      document.documentElement.className = new_classes.join(" ");
    } else {
      document.documentElement.className = modeClass;
    }
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
  function sendRequest(functionName, args, argSchemas, customCallback) {
    var request = prepareRequest(args, argSchemas);
    if (customCallback) {
      request.customCallback = customCallback;
    }
    // JSON.stringify doesn't support a root object which is undefined.
    if (request.args === undefined)
      request.args = null;

    var sargs = chromeHidden.JSON.stringify(request.args);

    var requestId = GetNextRequestId();
    requests[requestId] = request;
    var hasCallback = (request.callback || customCallback) ? true : false;
    return StartRequest(functionName, sargs, requestId, hasCallback);
  }

  // Send a special API request that is not JSON stringifiable, and optionally
  // register a callback.
  function sendCustomRequest(nativeFunction, functionName, args, argSchemas) {
    var request = prepareRequest(args, argSchemas);
    var requestId = GetNextRequestId();
    requests[requestId] = request;
    return nativeFunction(functionName, request.args, requestId,
                          request.callback ? true : false);
  }

  // Helper function for positioning pop-up windows relative to DOM objects.
  // Returns the absolute position of the given element relative to the hosting
  // browser frame.
  function findAbsolutePosition(domElement) {
    var left = domElement.offsetLeft;
    var top = domElement.offsetTop;

    // Ascend through the parent hierarchy, taking into account object nesting
    // and scoll positions.
    for (var parentElement = domElement.offsetParent; parentElement;
         parentElement = parentElement.offsetParent) {
      left += parentElement.offsetLeft;
      top += parentElement.offsetTop;

      left -= parentElement.scrollLeft;
      top -= parentElement.scrollTop;
    }

    return {
      top: top,
      left: left
    };
  }

  // Returns the coordiates of the rectangle encompassing the domElement,
  // in browser coordinates relative to the frame hosting the element.
  function getAbsoluteRect(domElement) {
    var rect = findAbsolutePosition(domElement);
    rect.width = domElement.offsetWidth || 0;
    rect.height = domElement.offsetHeight || 0;
    return rect;
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
  chrome.WebRequestEvent = function(eventName, opt_argSchemas) {
    if (typeof eventName != "string")
      throw new Error("chrome.WebRequestEvent requires an event name.");

    this.eventName_ = eventName;
    this.argSchemas_ = opt_argSchemas;
    this.subEvents_ = [];
    this.callbackMap_ = {};
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
    chrome.experimental.webRequest.addEventListener(
        cb, opt_filter, opt_extraInfo, this.eventName_, subEventName);

    var subEvent = new chrome.Event(subEventName, this.argSchemas_);
    this.subEvents_.push(subEvent);
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
    }
    this.callbackMap_[cb] = subEventCallback;
    subEvent.addListener(subEventCallback);
  };

  // Unregisters a callback.
  chrome.WebRequestEvent.prototype.removeListener = function(cb) {
    var idx = this.findListener_(cb);
    if (idx < 0) {
      return;
    }

    var subEventCallback = this.callbackMap_[cb];
    this.subEvents_[idx].removeListener(subEventCallback);
    if (!this.subEvents_[idx].hasListeners())
      this.subEvents_.splice(idx, 1);
  };

  chrome.WebRequestEvent.prototype.findListener_ = function(cb) {
    var subEventCallback = this.callbackMap_[cb];
    for (var i = 0; i < this.subEvents_.length; i++) {
      if (this.subEvents_[i].findListener_(subEventCallback) > -1)
        return i;
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

  function setupPreferences() {
    customBindings['Preference'] =
        function(prefKey, valueSchema, customHandlers) {
      if (customHandlers === undefined)
        customHandlers = {};
      this.get = function(details, callback) {
        var getSchema = this.parameters.get;
        chromeHidden.validate([details, callback], getSchema);
        return sendRequest(customHandlers.get || 'experimental.preferences.get',
                           [prefKey, details, callback],
                           extendSchema(getSchema));
      };
      this.set = function(details, callback) {
        var setSchema = this.parameters.set.slice();
        setSchema[0].properties.value = valueSchema;
        chromeHidden.validate([details, callback], setSchema);
        return sendRequest(customHandlers.set || 'experimental.preferences.set',
                           [prefKey, details, callback],
                           extendSchema(setSchema));
      };
      this.clear = function(details, callback) {
        var clearSchema = this.parameters.clear;
        chromeHidden.validate([details, callback], clearSchema);
        return sendRequest(customHandlers.clear ||
                               'experimental.preferences.clear',
                           [prefKey, details, callback],
                           extendSchema(clearSchema));
      };
      this.onChange = new chrome.Event('experimental.preferences.' + prefKey
                                           + '.onChange');
    };
    customBindings['Preference'].prototype = new CustomBindingsObject();
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

  function setupToolstripEvents(renderViewId) {
    chrome.toolstrip = chrome.toolstrip || {};
    chrome.toolstrip.onExpanded =
        new chrome.Event("toolstrip.onExpanded." + renderViewId);
    chrome.toolstrip.onCollapsed =
        new chrome.Event("toolstrip.onCollapsed." + renderViewId);
  }

  function setupHiddenContextMenuEvent(extensionId) {
    chromeHidden.contextMenus = {};
    chromeHidden.contextMenus.nextId = 1;
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
          result.description += child.nodeValue;
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
    chrome.experimental.tts.onSpeak.dispatch =
        function(text, options, requestId) {
      var callback = function(errorMessage) {
        if (errorMessage)
          chrome.experimental.tts.speakCompleted(requestId, errorMessage);
        else
          chrome.experimental.tts.speakCompleted(requestId);
      };
      chrome.Event.prototype.dispatch.apply(this, [text, options, callback]);
    };
  }

  chromeHidden.onLoad.addListener(function (extensionId) {
    if (!extensionId) {
      return;
    }
    chrome.initExtension(extensionId, false, IsIncognitoProcess());

    // Setup the Preference class so we can use it to construct Preference
    // objects from the API definition.
    setupPreferences();

    // |apiFunctions| is a hash of name -> object that stores the
    // name & definition of the apiFunction. Custom handling of api functions
    // is implemented by adding a "handleRequest" function to the object.
    var apiFunctions = {};

    // Read api definitions and setup api functions in the chrome namespace.
    // TODO(rafaelw): Consider defining a json schema for an api definition
    //   and validating either here, in a unit_test or both.
    // TODO(rafaelw): Handle synchronous functions.
    // TOOD(rafaelw): Consider providing some convenient override points
    //   for api functions that wish to insert themselves into the call.
    var apiDefinitions = chromeHidden.JSON.parse(GetExtensionAPIDefinition());

    apiDefinitions.forEach(function(apiDef) {
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

      // Setup Functions.
      if (apiDef.functions) {
        apiDef.functions.forEach(function(functionDef) {
          // Module functions may have been defined earlier by hand. Don't
          // clobber them.
          if (module[functionDef.name])
            return;

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
                                   this.customCallback);
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
          // Module events may have been defined earlier by hand. Don't clobber
          // them.
          if (module[eventDef.name])
            return;

          var eventName = apiDef.namespace + "." + eventDef.name;
          if (apiDef.namespace == "experimental.webRequest") {
            // WebRequest events have a special structure.
            module[eventDef.name] = new chrome.WebRequestEvent(eventName,
                eventDef.parameters);
          } else {
            module[eventDef.name] = new chrome.Event(eventName,
                eventDef.parameters);
          }
        });
      }


      // Parse any values defined for properties.
      if (apiDef.properties) {
        forEach(apiDef.properties, function(prop, property) {
          if (property.value) {
            var value = property.value;
            if (property.type === 'integer') {
              value = parseInt(value);
            } else if (property.type === 'boolean') {
              value = value === "true";
            } else if (property["$ref"]) {
              var constructor = customBindings[property["$ref"]];
              var args = value;
              // For an object property, |value| is an array of constructor
              // arguments, but we want to pass the arguments directly
              // (i.e. not as an array), so we have to fake calling |new| on the
              // constructor.
              value = { __proto__: constructor.prototype };
              constructor.apply(value, args);
            } else if (property.type !== 'string') {
              throw "NOT IMPLEMENTED (extension_api.json error): Cannot " +
                  "parse values for type \"" + property.type + "\"";
            }
            module[prop] = value;
          }
        });
      }

      // getTabContentses is retained for backwards compatibility
      // See http://crbug.com/21433
      chrome.extension.getTabContentses = chrome.extension.getExtensionTabs;
    });

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
        if (responseCallback)
          responseCallback(response);
        port.disconnect();
      });
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

    apiFunctions["extension.getToolstrips"].handleRequest =
        function(windowId) {
      if (typeof(windowId) == "undefined")
        windowId = -1;
      return GetExtensionViews(windowId, "TOOLSTRIP");
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

        sendCustomRequest(nativeFunction, name, [details], parameters);
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
          sendCustomRequest(nativeFunction, name, [details], parameters);
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
      var id = chromeHidden.contextMenus.nextId++;
      args[0].generatedId = id;
      sendRequest(this.name, args, this.definition.parameters,
                  this.customCallback);
      return id;
    };

    apiFunctions["omnibox.setDefaultSuggestion"].handleRequest =
        function(details) {
      var parseResult = parseOmniboxDescription(details.description);
      sendRequest(this.name, [parseResult], this.definition.parameters);
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

    if (chrome.test) {
      chrome.test.getApiDefinitions = GetExtensionAPIDefinition;
    }

    setupPageActionEvents(extensionId);
    setupToolstripEvents(GetRenderViewId());
    setupHiddenContextMenuEvent(extensionId);
    setupOmniboxEvents();
    setupTtsEvents();
  });

  if (!chrome.experimental)
    chrome.experimental = {};

  if (!chrome.experimental.accessibility)
    chrome.experimental.accessibility = {};

  if (!chrome.experimental.tts)
    chrome.experimental.tts = {};
})();
