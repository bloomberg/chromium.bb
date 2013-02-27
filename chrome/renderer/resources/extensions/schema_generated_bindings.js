// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Generates the chrome.* API bindings from a list of schemas.

  // TODO(battre): cleanup the usage of packages everywhere, as described here
  // http://codereview.chromium.org/10392008/diff/38/chrome/renderer/resources/extensions/schema_generated_bindings.js

  require('json_schema');
  require('event_bindings');
  var GetExtensionAPIDefinition =
      requireNative('apiDefinitions').GetExtensionAPIDefinition;
  var sendRequest = require('sendRequest').sendRequest;
  var utils = require('utils');
  var chromeHidden = requireNative('chrome_hidden').GetChromeHidden();
  var schemaUtils = require('schemaUtils');

  // The object to generate the bindings for "internal" APIs in, so that
  // extensions can't directly call them (without access to chromeHidden),
  // but are still needed for internal mechanisms of extensions (e.g. events).
  //
  // This is distinct to the "*Private" APIs which are controlled via
  // having strict permissions and aren't generated *anywhere* unless needed.
  var internalAPIs = {};
  chromeHidden.internalAPIs = internalAPIs;

  // Stores the name and definition of each API function, with methods to
  // modify their behaviour (such as a custom way to handle requests to the
  // API, a custom callback, etc).
  function APIFunctions() {
    this._apiFunctions = {};
    this._unavailableApiFunctions = {};
  }
  APIFunctions.prototype.register = function(apiName, apiFunction) {
    this._apiFunctions[apiName] = apiFunction;
  };
  // Registers a function as existing but not available, meaning that calls to
  // the set* methods that reference this function should be ignored rather
  // than throwing Errors.
  APIFunctions.prototype.registerUnavailable = function(apiName) {
    this._unavailableApiFunctions[apiName] = apiName;
  };
  APIFunctions.prototype._setHook =
      function(apiName, propertyName, customizedFunction) {
    if (this._unavailableApiFunctions.hasOwnProperty(apiName))
      return;
    if (!this._apiFunctions.hasOwnProperty(apiName))
      throw new Error('Tried to set hook for unknown API "' + apiName + '"');
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

  // Wraps the calls to the set* methods of APIFunctions with the namespace of
  // an API, and validates that all calls to set* methods aren't prefixed with
  // a namespace.
  //
  // For example, if constructed with 'browserAction', a call to
  // handleRequest('foo') will be transformed into
  // handleRequest('browserAction.foo').
  //
  // Likewise, if a call to handleRequest is called with 'browserAction.foo',
  // it will throw an error.
  //
  // These help with isolating custom bindings from each other.
  function NamespacedAPIFunctions(namespace, delegate) {
    var self = this;
    function wrap(methodName) {
      self[methodName] = function(apiName, customizedFunction) {
        var prefix = namespace + '.';
        if (apiName.indexOf(prefix) === 0) {
          throw new Error(methodName + ' called with "' + apiName +
                          '" which has a "' + prefix + '" prefix. ' +
                          'This is unnecessary and must be left out.');
        }
        return delegate[methodName].call(delegate,
                                         prefix + apiName, customizedFunction);
      };
    }

    wrap('contains');
    wrap('setHandleRequest');
    wrap('setUpdateArgumentsPostValidate');
    wrap('setUpdateArgumentsPreValidate');
    wrap('setCustomCallback');
  }

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

  function CustomBindingsObject() {
  }
  CustomBindingsObject.prototype.setSchema = function(schema) {
    // The functions in the schema are in list form, so we move them into a
    // dictionary for easier access.
    var self = this;
    self.functionSchemas = {};
    schema.functions.forEach(function(f) {
      self.functionSchemas[f.name] = {
        name: f.name,
        definition: f
      }
    });
  };

  // Registers a custom type referenced via "$ref" fields in the API schema
  // JSON.
  var customTypes = {};
  chromeHidden.registerCustomType = function(typeName, customTypeFactory) {
    var customType = customTypeFactory();
    customType.prototype = new CustomBindingsObject();
    customTypes[typeName] = customType;
  };

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

  function isPlatformSupported(schemaNode, platform) {
    return !schemaNode.platforms ||
        schemaNode.platforms.indexOf(platform) > -1;
  }

  function isManifestVersionSupported(schemaNode, manifestVersion) {
    return !schemaNode.maximumManifestVersion ||
        manifestVersion <= schemaNode.maximumManifestVersion;
  }

  function isSchemaNodeSupported(schemaNode, platform, manifestVersion) {
    return isPlatformSupported(schemaNode, platform) &&
        isManifestVersionSupported(schemaNode, manifestVersion);
  }

  chromeHidden.onLoad.addListener(function(extensionId,
                                           contextType,
                                           isIncognitoProcess,
                                           manifestVersion) {
    var apiDefinitions = GetExtensionAPIDefinition();

    // Read api definitions and setup api functions in the chrome namespace.
    var platform = getPlatform();

    apiDefinitions.forEach(function(apiDef) {
      // TODO(kalman): Remove this, or refactor schema_generated_bindings.js so
      // that it isn't necessary. For now, chrome.app and chrome.webstore are
      // entirely handwritten.
      if (['app', 'webstore'].indexOf(apiDef.namespace) >= 0)
        return;

      if (!isSchemaNodeSupported(apiDef, platform, manifestVersion))
        return;

      // See comment on internalAPIs at the top.
      var mod = apiDef.internal ? internalAPIs : chrome;

      var namespaces = apiDef.namespace.split('.');
      for (var index = 0, name; name = namespaces[index]; index++) {
        mod[name] = mod[name] || {};
        mod = mod[name];
      }

      // Add types to global schemaValidator
      if (apiDef.types) {
        apiDef.types.forEach(function(t) {
          if (!isSchemaNodeSupported(t, platform, manifestVersion))
            return;

          schemaUtils.schemaValidator.addTypes(t);
          if (t.type == 'object' && customTypes[t.id]) {
            customTypes[t.id].prototype.setSchema(t);
          }
        });
      }

      // Returns whether access to the content of a schema should be denied,
      // based on the presence of "unprivileged" and whether this is an
      // extension process (versus e.g. a content script).
      function isSchemaAccessAllowed(itemSchema) {
        return (contextType == 'BLESSED_EXTENSION') ||
               apiDef.unprivileged ||
               itemSchema.unprivileged;
      }

      // Adds a getter that throws an access denied error to object |mod|
      // for property |name|.
      function addUnprivilegedAccessGetter(mod, name) {
        mod.__defineGetter__(name, function() {
          throw new Error(
              '"' + name + '" can only be used in extension processes. See ' +
              'the content scripts documentation for more details.');
        });
      }

      // Setup Functions.
      if (apiDef.functions) {
        apiDef.functions.forEach(function(functionDef) {
          if (functionDef.name in mod) {
            throw new Error('Function ' + functionDef.name +
                            ' already defined in ' + apiDef.namespace);
          }

          var apiFunctionName = apiDef.namespace + "." + functionDef.name;

          if (!isSchemaNodeSupported(functionDef, platform, manifestVersion)) {
            apiFunctions.registerUnavailable(apiFunctionName);
            return;
          }
          if (!isSchemaAccessAllowed(functionDef)) {
            apiFunctions.registerUnavailable(apiFunctionName);
            addUnprivilegedAccessGetter(mod, functionDef.name);
            return;
          }

          var apiFunction = {};
          apiFunction.definition = functionDef;
          apiFunction.name = apiFunctionName;

          // TODO(aa): It would be best to run this in a unit test, but in order
          // to do that we would need to better factor this code so that it
          // doesn't depend on so much v8::Extension machinery.
          if (chromeHidden.validateAPI &&
              schemaUtils.isFunctionSignatureAmbiguous(
                  apiFunction.definition)) {
            throw new Error(
                apiFunction.name + ' has ambiguous optional arguments. ' +
                'To implement custom disambiguation logic, add ' +
                '"allowAmbiguousOptionalArguments" to the function\'s schema.');
          }

          apiFunctions.register(apiFunction.name, apiFunction);

          mod[functionDef.name] = (function() {
            var args = Array.prototype.slice.call(arguments);
            if (this.updateArgumentsPreValidate)
              args = this.updateArgumentsPreValidate.apply(this, args);

            args = schemaUtils.normalizeArgumentsAndValidate(args, this);
            if (this.updateArgumentsPostValidate)
              args = this.updateArgumentsPostValidate.apply(this, args);

            var retval;
            if (this.handleRequest) {
              retval = this.handleRequest.apply(this, args);
            } else {
              var optArgs = {
                customCallback: this.customCallback
              };
              retval = sendRequest(this.name, args,
                                   this.definition.parameters,
                                   optArgs);
            }

            // Validate return value if defined - only in debug.
            if (chromeHidden.validateCallbacks &&
                this.definition.returns) {
              schemaUtils.validate([retval], [this.definition.returns]);
            }
            return retval;
          }).bind(apiFunction);
        });
      }

      // Setup Events
      if (apiDef.events) {
        apiDef.events.forEach(function(eventDef) {
          if (eventDef.name in mod) {
            throw new Error('Event ' + eventDef.name +
                            ' already defined in ' + apiDef.namespace);
          }
          if (!isSchemaNodeSupported(eventDef, platform, manifestVersion))
            return;
          if (!isSchemaAccessAllowed(eventDef)) {
            addUnprivilegedAccessGetter(mod, eventDef.name);
            return;
          }

          var eventName = apiDef.namespace + "." + eventDef.name;
          var customEvent = customEvents[apiDef.namespace];
          var options = eventDef.options || {};

          if (eventDef.filters && eventDef.filters.length > 0)
            options.supportsFilters = true;

          if (customEvent) {
            mod[eventDef.name] = new customEvent(
                eventName, eventDef.parameters, eventDef.extraParameters,
                options);
          } else if (eventDef.anonymous) {
            mod[eventDef.name] = new chrome.Event();
          } else {
            mod[eventDef.name] = new chrome.Event(
                eventName, eventDef.parameters, options);
          }
        });
      }

      function addProperties(m, parentDef) {
        var properties = parentDef.properties;
        if (!properties)
          return;

        utils.forEach(properties, function(propertyName, propertyDef) {
          if (propertyName in m)
            return;  // TODO(kalman): be strict like functions/events somehow.
          if (!isSchemaNodeSupported(propertyDef, platform, manifestVersion))
            return;
          if (!isSchemaAccessAllowed(propertyDef)) {
            addUnprivilegedAccessGetter(m, propertyName);
            return;
          }

          var value = propertyDef.value;
          if (value) {
            // Values may just have raw types as defined in the JSON, such
            // as "WINDOW_ID_NONE": { "value": -1 }. We handle this here.
            // TODO(kalman): enforce that things with a "value" property can't
            // define their own types.
            var type = propertyDef.type || typeof(value);
            if (type === 'integer' || type === 'number') {
              value = parseInt(value);
            } else if (type === 'boolean') {
              value = value === "true";
            } else if (propertyDef["$ref"]) {
              var constructor = customTypes[propertyDef["$ref"]];
              if (!constructor)
                throw new Error("No custom binding for " + propertyDef["$ref"]);
              var args = value;
              // For an object propertyDef, |value| is an array of constructor
              // arguments, but we want to pass the arguments directly (i.e.
              // not as an array), so we have to fake calling |new| on the
              // constructor.
              value = { __proto__: constructor.prototype };
              constructor.apply(value, args);
              // Recursively add properties.
              addProperties(value, propertyDef);
            } else if (type === 'object') {
              // Recursively add properties.
              addProperties(value, propertyDef);
            } else if (type !== 'string') {
              throw new Error("NOT IMPLEMENTED (extension_api.json error): " +
                  "Cannot parse values for type \"" + type + "\"");
            }
            m[propertyName] = value;
          }
        });
      }

      addProperties(mod, apiDef);
    });

    // Run the non-declarative custom hooks after all the schemas have been
    // generated, in case hooks depend on other APIs being available.
    apiDefinitions.forEach(function(apiDef) {
      if (!isSchemaNodeSupported(apiDef, platform, manifestVersion))
        return;

      var hook = customHooks[apiDef.namespace];
      if (!hook)
        return;

      // Pass through the public API of schema_generated_bindings, to be used
      // by custom bindings JS files. Create a new one so that bindings can't
      // interfere with each other.
      hook({
        apiFunctions: new NamespacedAPIFunctions(apiDef.namespace,
                                                 apiFunctions),
        apiDefinitions: apiDefinitions,
      }, extensionId, contextType);
    });

    if (chrome.test)
      chrome.test.getApiDefinitions = GetExtensionAPIDefinition;
  });
