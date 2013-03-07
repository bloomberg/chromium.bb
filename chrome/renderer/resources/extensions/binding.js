// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

require('json_schema');
require('event_bindings');
var schemaRegistry = requireNative('schema_registry');
var sendRequest = require('sendRequest').sendRequest;
var utils = require('utils');
var chromeHidden = requireNative('chrome_hidden').GetChromeHidden();
var chrome = requireNative('chrome').GetChrome();
var schemaUtils = require('schemaUtils');
var process = requireNative('process');
var manifestVersion = process.GetManifestVersion();
var extensionId = process.GetExtensionId();
var contextType = process.GetContextType();
var GetAvailability = requireNative('v8_context').GetAvailability;
var logging = requireNative('logging');

// Stores the name and definition of each API function, with methods to
// modify their behaviour (such as a custom way to handle requests to the
// API, a custom callback, etc).
function APIFunctions() {
  this.apiFunctions_ = {};
  this.unavailableApiFunctions_ = {};
}

APIFunctions.prototype.register = function(apiName, apiFunction) {
  this.apiFunctions_[apiName] = apiFunction;
};

// Registers a function as existing but not available, meaning that calls to
// the set* methods that reference this function should be ignored rather
// than throwing Errors.
APIFunctions.prototype.registerUnavailable = function(apiName) {
  this.unavailableApiFunctions_[apiName] = apiName;
};

APIFunctions.prototype.setHook_ =
    function(apiName, propertyName, customizedFunction) {
  if (this.unavailableApiFunctions_.hasOwnProperty(apiName))
    return;
  if (!this.apiFunctions_.hasOwnProperty(apiName))
    throw new Error('Tried to set hook for unknown API "' + apiName + '"');
  this.apiFunctions_[apiName][propertyName] = customizedFunction;
};

APIFunctions.prototype.setHandleRequest =
    function(apiName, customizedFunction) {
  return this.setHook_(apiName, 'handleRequest', customizedFunction);
};

APIFunctions.prototype.setUpdateArgumentsPostValidate =
    function(apiName, customizedFunction) {
  return this.setHook_(
    apiName, 'updateArgumentsPostValidate', customizedFunction);
};

APIFunctions.prototype.setUpdateArgumentsPreValidate =
    function(apiName, customizedFunction) {
  return this.setHook_(
    apiName, 'updateArgumentsPreValidate', customizedFunction);
};

APIFunctions.prototype.setCustomCallback =
    function(apiName, customizedFunction) {
  return this.setHook_(apiName, 'customCallback', customizedFunction);
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

var platform = getPlatform();

function Binding(schema) {
  this.schema_ = schema;
  this.apiFunctions_ = new APIFunctions();
  this.customEvent_ = null;
  this.customTypes_ = {};
  this.customHooks_ = [];
};

Binding.create = function(apiName) {
  return new Binding(schemaRegistry.GetSchema(apiName));
};

Binding.prototype = {
  // The API through which the ${api_name}_custom_bindings.js files customize
  // their API bindings beyond what can be generated.
  //
  // There are 2 types of customizations available: those which are required in
  // order to do the schema generation (registerCustomEvent and
  // registerCustomType), and those which can only run after the bindings have
  // been generated (registerCustomHook).
  //

  // Registers a custom type referenced via "$ref" fields in the API schema
  // JSON.
  registerCustomType: function(typeName, customTypeFactory) {
    var customType = customTypeFactory();
    customType.prototype = new CustomBindingsObject();
    this.customTypes_[typeName] = customType;
  },

  // Registers a custom event type for the API identified by |namespace|.
  // |event| is the event's constructor.
  registerCustomEvent: function(event) {
    this.customEvent_ = event;
  },

  // Registers a function |hook| to run after the schema for all APIs has been
  // generated.  The hook is passed as its first argument an "API" object to
  // interact with, and second the current extension ID. See where
  // |customHooks| is used.
  registerCustomHook: function(fn) {
    this.customHooks_.push(fn);
  },

  // TODO(kalman/cduvall): Refactor this so |runHooks_| is not needed.
  runHooks_: function(api) {
    this.customHooks_.forEach(function(hook) {
      if (!isSchemaNodeSupported(this.schema_, platform, manifestVersion))
        return;

      if (!hook)
        return;

      hook({
        apiFunctions: this.apiFunctions_,
        schema: this.schema_,
        compiledApi: api
      }, extensionId, contextType);
    }, this);
  },

  // Generates the bindings from |this.schema_| and integrates any custom
  // bindings that might be present.
  generate: function() {
    var schema = this.schema_;
    var customTypes = this.customTypes_;

    // TODO(kalman/cduvall): Make GetAvailability handle this, then delete the
    // supporting code.
    if (!isSchemaNodeSupported(schema, platform, manifestVersion))
      return;

    var availability = GetAvailability(schema.namespace);
    if (!availability.is_available) {
      console.error('chrome.' + schema.namespace + ' is not available: ' +
                        availability.message);
      return;
    }

    // See comment on internalAPIs at the top.
    var mod = {};

    var namespaces = schema.namespace.split('.');
    for (var index = 0, name; name = namespaces[index]; index++) {
      mod[name] = mod[name] || {};
      mod = mod[name];
    }

    // Add types to global schemaValidator
    if (schema.types) {
      schema.types.forEach(function(t) {
        if (!isSchemaNodeSupported(t, platform, manifestVersion))
          return;

        schemaUtils.schemaValidator.addTypes(t);
        if (t.type == 'object' && this.customTypes_[t.id]) {
          var parts = t.id.split(".");
          this.customTypes_[t.id].prototype.setSchema(t);
          mod[parts[parts.length - 1]] = this.customTypes_[t.id];
        }
      }, this);
    }

    // Returns whether access to the content of a schema should be denied,
    // based on the presence of "unprivileged" and whether this is an
    // extension process (versus e.g. a content script).
    function isSchemaAccessAllowed(itemSchema) {
      return (contextType == 'BLESSED_EXTENSION') ||
             schema.unprivileged ||
             itemSchema.unprivileged;
    };

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
    if (schema.functions) {
      schema.functions.forEach(function(functionDef) {
        if (functionDef.name in mod) {
          throw new Error('Function ' + functionDef.name +
                          ' already defined in ' + schema.namespace);
        }

        if (!isSchemaNodeSupported(functionDef, platform, manifestVersion)) {
          this.apiFunctions_.registerUnavailable(functionDef.name);
          return;
        }
        if (!isSchemaAccessAllowed(functionDef)) {
          this.apiFunctions_.registerUnavailable(functionDef.name);
          addUnprivilegedAccessGetter(mod, functionDef.name);
          return;
        }

        var apiFunction = {};
        apiFunction.definition = functionDef;
        apiFunction.name = schema.namespace + '.' + functionDef.name;

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

        this.apiFunctions_.register(functionDef.name, apiFunction);

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
      }, this);
    }

    // Setup Events
    if (schema.events) {
      schema.events.forEach(function(eventDef) {
        if (eventDef.name in mod) {
          throw new Error('Event ' + eventDef.name +
                          ' already defined in ' + schema.namespace);
        }
        if (!isSchemaNodeSupported(eventDef, platform, manifestVersion))
          return;
        if (!isSchemaAccessAllowed(eventDef)) {
          addUnprivilegedAccessGetter(mod, eventDef.name);
          return;
        }

        var eventName = schema.namespace + "." + eventDef.name;
        var options = eventDef.options || {};

        if (eventDef.filters && eventDef.filters.length > 0)
          options.supportsFilters = true;

        if (this.customEvent_) {
          mod[eventDef.name] = new this.customEvent_(
              eventName, eventDef.parameters, eventDef.extraParameters,
              options);
        } else if (eventDef.anonymous) {
          mod[eventDef.name] = new chrome.Event();
        } else {
          mod[eventDef.name] = new chrome.Event(
              eventName, eventDef.parameters, options);
        }
      }, this);
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
            value = value === 'true';
          } else if (propertyDef['$ref']) {
            if (propertyDef['$ref'] in customTypes) {
              var constructor = customTypes[propertyDef['$ref']];
            } else {
              var refParts = propertyDef['$ref'].split('.');
              // This should never try to load a $ref in the current namespace.
              var constructor = utils.loadRefDependency(
                  propertyDef['$ref'])[refParts[refParts.length - 1]];
            }
            if (!constructor)
              throw new Error('No custom binding for ' + propertyDef['$ref']);
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
            throw new Error('NOT IMPLEMENTED (extension_api.json error): ' +
                'Cannot parse values for type "' + type + '"');
          }
          m[propertyName] = value;
        }
      });
    };

    addProperties(mod, schema);
    this.runHooks_(mod);
    return mod;
  }
};

exports.Binding = Binding;
