// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This script contains privileged chrome extension related javascript APIs.
// It is loaded by pages whose URL has the chrome-extension protocol.

var chrome = chrome || {};
(function() {
  native function GetChromeHidden();
  native function GetExtensionAPIDefinition();
  native function GetNextRequestId();
  native function StartRequest();
  native function SetIconCommon();

  var chromeHidden = GetChromeHidden();

  if (!chrome)
    chrome = {};

  function apiExists(path) {
    var resolved = chrome;
    path.split(".").forEach(function(next) {
      if (resolved)
        resolved = resolved[next];
    });
    return !!resolved;
  }

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

  // TODO(kalman): It's a shame to need to define this function here, since it's
  // only used in 2 APIs (browserAction and pageAction). It would be nice to
  // only load this if one of those APIs has been loaded.
  // That said, both of those APIs are always injected into pages anyway (see
  // chrome/common/extensions/extension_permission_set.cc).
  function setIcon(details, name, parameters, actionType) {
    var iconSize = 19;
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
                  {noStringify: true, nativeFunction: SetIconCommon});
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
                    {noStringify: true, nativeFunction: SetIconCommon});
      };
      img.src = details.path;
    } else {
      throw new Error(
          "Either the path or imageData property must be specified.");
    }
  }

  // Stores the name and definition of each API function, with methods to
  // modify their behaviour (such as a custom way to handle requests to the
  // API, a custom callback, etc).
  function APIFunctions() {
    this._apiFunctions = {};
  }
  APIFunctions.prototype.register = function(apiName, apiFunction) {
    this._apiFunctions[apiName] = apiFunction;
  };
  APIFunctions.prototype._setHook =
      function(apiName, propertyName, customizedFunction) {
    if (this._apiFunctions.hasOwnProperty(apiName))
      this._apiFunctions[apiName][propertyName] = customizedFunction;
  };
  APIFunctions.prototype.setHandleRequest =
      function(apiName, customizedFunction) {
    return this._setHook(apiName, 'handleRequest', customizedFunction);
  };
  APIFunctions.prototype.setUpdateArgumentsPostValidate =
      function(apiName, customizedFunction) {
    return this._setHook(
      apiName, 'updateArgumentsPostValidate', customizedFunction);
  };
  APIFunctions.prototype.setUpdateArgumentsPreValidate =
      function(apiName, customizedFunction) {
    return this._setHook(
      apiName, 'updateArgumentsPreValidate', customizedFunction);
  };
  APIFunctions.prototype.setCustomCallback =
      function(apiName, customizedFunction) {
    return this._setHook(apiName, 'customCallback', customizedFunction);
  };

  var apiFunctions = new APIFunctions();

  //
  // The API through which the ${api_name}_custom_bindings.js files customize
  // their API bindings beyond what can be generated.
  //
  // There are 2 types of customizations available: those which are required in
  // order to do the schema generation (registerCustomEvent and
  // registerCustomType), and those which can only run after the bindings have
  // been generated (registerCustomHook).
  //

  // Registers a custom event type for the API identified by |namespace|.
  // |event| is the event's constructor.
  var customEvents = {};
  chromeHidden.registerCustomEvent = function(namespace, event) {
    if (typeof(namespace) !== 'string') {
      throw new Error("registerCustomEvent requires the namespace of the " +
                      "API as its first argument");
    }
    customEvents[namespace] = event;
  };

  // Registers a function |hook| to run after the schema for all APIs has been
  // generated.  The hook is passed as its first argument an "API" object to
  // interact with, and second the current extension ID. See where
  // |customHooks| is used.
  var customHooks = {};
  chromeHidden.registerCustomHook = function(namespace, fn) {
    if (typeof(namespace) !== 'string') {
      throw new Error("registerCustomHook requires the namespace of the " +
                      "API as its first argument");
    }
    customHooks[namespace] = fn;
  };

  // --- Setup additional api's not currently handled in common/extensions/api

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

  var customTypes = {};

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
    customTypes['ChromeSetting'] = ChromeSetting;
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
    customTypes['ContentSetting'] = ContentSetting;
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
    customTypes['StorageNamespace'] = StorageNamespace;
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
    var apiDefinitions = GetExtensionAPIDefinition();

    // Setup custom classes so we can use them to construct $ref'd objects from
    // the API definition.
    apiDefinitions.forEach(function(apiDef) {
      switch (apiDef.namespace) {
        case "types":
          setupChromeSetting();
          break;

        case "contentSettings":
          setupContentSetting();
          break;

        case "experimental.storage":
          setupStorageNamespace();
          break;
      }
    });

    // Read api definitions and setup api functions in the chrome namespace.
    // TODO(rafaelw): Consider defining a json schema for an api definition
    //   and validating either here, in a unit_test or both.
    // TODO(rafaelw): Handle synchronous functions.
    // TODO(rafaelw): Consider providing some convenient override points
    //   for api functions that wish to insert themselves into the call.
    var platform = getPlatform();

    apiDefinitions.forEach(function(apiDef) {
      // Only generate bindings if supported by this platform.
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
          if (t.type == 'object' && customTypes[t.id]) {
            customTypes[t.id].prototype.setSchema(t);
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
          apiFunctions.register(apiFunction.name, apiFunction);

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
          var customEvent = customEvents[apiDef.namespace];
          if (customEvent) {
            module[eventDef.name] = new customEvent(
                eventName, eventDef.parameters, eventDef.extraParameters);
          } else {
            module[eventDef.name] = new chrome.Event(
                eventName, eventDef.parameters);
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
                var constructor = customTypes[property["$ref"]];
                if (!constructor)
                  throw new Error("No custom binding for " + property["$ref"]);
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

    // TODO(kalman/aa): "The rest of this crap..." comment above. Only run the
    // custom hooks in extension processes, to maintain current behaviour. We
    // should fix this this with a smaller hammer.
    apiDefinitions.forEach(function(apiDef) {
      // Only generate bindings if supported by this platform.
      if (apiDef.platforms && apiDef.platforms.indexOf(platform) == -1)
        return;

      var hook = customHooks[apiDef.namespace];
      if (!hook)
        return;

      // Pass through the public API of schema_generated_bindings, to be used
      // by custom bindings JS files. Create a new one so that bindings can't
      // interfere with each other.
      hook({
        apiFunctions: apiFunctions,
        sendRequest: sendRequest,
        setIcon: setIcon,
      }, extensionId);
    });

    // TOOD(mihaip): remove this alias once the webstore stops calling
    // beginInstallWithManifest2.
    // See http://crbug.com/100242
    if (apiExists("webstorePrivate")) {
      chrome.webstorePrivate.beginInstallWithManifest2 =
          chrome.webstorePrivate.beginInstallWithManifest3;
    }

    if (apiExists("test"))
      chrome.test.getApiDefinitions = GetExtensionAPIDefinition;
  });
})();
