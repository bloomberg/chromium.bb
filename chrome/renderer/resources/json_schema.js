// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// -----------------------------------------------------------------------------
// NOTE: If you change this file you need to touch renderer_resources.grd to
// have your change take effect.
// -----------------------------------------------------------------------------

//==============================================================================
// This file contains a class that implements a subset of JSON Schema.
// See: http://www.json.com/json-schema-proposal/ for more details.
//
// The following features of JSON Schema are not implemented:
// - requires
// - unique
// - disallow
// - union types (but replaced with 'choices')
//
// The following properties are not applicable to the interface exposed by
// this class:
// - options
// - readonly
// - title
// - description
// - format
// - default
// - transient
// - hidden
//
// There are also these departures from the JSON Schema proposal:
// - function and undefined types are supported
// - null counts as 'unspecified' for optional values
// - added the 'choices' property, to allow specifying a list of possible types
//   for a value
// - by default an "object" typed schema does not allow additional properties.
//   if present, "additionalProperties" is to be a schema against which all
//   additional properties will be validated.
//==============================================================================

(function() {
native function GetChromeHidden();
var chromeHidden = GetChromeHidden();

/**
 * Validates an instance against a schema and accumulates errors. Usage:
 *
 * var validator = new chromeHidden.JSONSchemaValidator();
 * validator.validate(inst, schema);
 * if (validator.errors.length == 0)
 *   console.log("Valid!");
 * else
 *   console.log(validator.errors);
 *
 * The errors property contains a list of objects. Each object has two
 * properties: "path" and "message". The "path" property contains the path to
 * the key that had the problem, and the "message" property contains a sentence
 * describing the error.
 */
chromeHidden.JSONSchemaValidator = function() {
  this.errors = [];
  this.types = [];
};

chromeHidden.JSONSchemaValidator.messages = {
  invalidEnum: "Value must be one of: [*].",
  propertyRequired: "Property is required.",
  unexpectedProperty: "Unexpected property.",
  arrayMinItems: "Array must have at least * items.",
  arrayMaxItems: "Array must not have more than * items.",
  itemRequired: "Item is required.",
  stringMinLength: "String must be at least * characters long.",
  stringMaxLength: "String must not be more than * characters long.",
  stringPattern: "String must match the pattern: *.",
  numberFiniteNotNan: "Value must not be *.",
  numberMinValue: "Value must not be less than *.",
  numberMaxValue: "Value must not be greater than *.",
  numberMaxDecimal: "Value must not have more than * decimal places.",
  invalidType: "Expected '*' but got '*'.",
  invalidChoice: "Value does not match any valid type choices.",
  invalidPropertyType: "Missing property type.",
  schemaRequired: "Schema value required.",
  unknownSchemaReference: "Unknown schema reference: *.",
  notInstance: "Object must be an instance of *."
};

/**
 * Builds an error message. Key is the property in the |errors| object, and
 * |opt_replacements| is an array of values to replace "*" characters with.
 */
chromeHidden.JSONSchemaValidator.formatError = function(key, opt_replacements) {
  var message = this.messages[key];
  if (opt_replacements) {
    for (var i = 0; i < opt_replacements.length; i++) {
      message = message.replace("*", opt_replacements[i]);
    }
  }
  return message;
};

/**
 * Classifies a value as one of the JSON schema primitive types. Note that we
 * don't explicitly disallow 'function', because we want to allow functions in
 * the input values.
 */
chromeHidden.JSONSchemaValidator.getType = function(value) {
  var s = typeof value;

  if (s == "object") {
    if (value === null) {
      return "null";
    } else if (Object.prototype.toString.call(value) == "[object Array]") {
      return "array";
    }
  } else if (s == "number") {
    if (value % 1 == 0) {
      return "integer";
    }
  }

  return s;
};

/**
 * Add types that may be referenced by validated schemas that reference them
 * with "$ref": <typeId>. Each type must be a valid schema and define an
 * "id" property.
 */
chromeHidden.JSONSchemaValidator.prototype.addTypes = function(typeOrTypeList) {
  function addType(validator, type) {
    if(!type.id)
      throw "Attempt to addType with missing 'id' property";
    validator.types[type.id] = type;
  }

  if (typeOrTypeList instanceof Array) {
    for (var i = 0; i < typeOrTypeList.length; i++) {
      addType(this, typeOrTypeList[i]);
    }
  } else {
    addType(this, typeOrTypeList);
  }
}

/**
 * Validates an instance against a schema. The instance can be any JavaScript
 * value and will be validated recursively. When this method returns, the
 * |errors| property will contain a list of errors, if any.
 */
chromeHidden.JSONSchemaValidator.prototype.validate = function(
    instance, schema, opt_path) {
  var path = opt_path || "";

  if (!schema) {
    this.addError(path, "schemaRequired");
    return;
  }

  // If this schema defines itself as reference type, save it in this.types.
  if (schema.id)
    this.types[schema.id] = schema;

  // If the schema has an extends property, the instance must validate against
  // that schema too.
  if (schema.extends)
    this.validate(instance, schema.extends, path);

  // If the schema has a $ref property, the instance must validate against
  // that schema too. It must be present in this.types to be referenced.
  if (schema["$ref"]) {
    if (!this.types[schema["$ref"]])
      this.addError(path, "unknownSchemaReference", [ schema["$ref"] ]);
    else
      this.validate(instance, this.types[schema["$ref"]], path)
  }

  // If the schema has a choices property, the instance must validate against at
  // least one of the items in that array.
  if (schema.choices) {
    this.validateChoices(instance, schema, path);
    return;
  }

  // If the schema has an enum property, the instance must be one of those
  // values.
  if (schema.enum) {
    if (!this.validateEnum(instance, schema, path))
      return;
  }

  if (schema.type && schema.type != "any") {
    if (!this.validateType(instance, schema, path))
      return;

    // Type-specific validation.
    switch (schema.type) {
      case "object":
        this.validateObject(instance, schema, path);
        break;
      case "array":
        this.validateArray(instance, schema, path);
        break;
      case "string":
        this.validateString(instance, schema, path);
        break;
      case "number":
      case "integer":
        this.validateNumber(instance, schema, path);
        break;
    }
  }
};

/**
 * Validates an instance against a choices schema. The instance must match at
 * least one of the provided choices.
 */
chromeHidden.JSONSchemaValidator.prototype.validateChoices = function(
    instance, schema, path) {
  var originalErrors = this.errors;

  for (var i = 0; i < schema.choices.length; i++) {
    this.errors = [];
    this.validate(instance, schema.choices[i], path);
    if (this.errors.length == 0) {
      this.errors = originalErrors;
      return;
    }
  }

  this.errors = originalErrors;
  this.addError(path, "invalidChoice");
};

/**
 * Validates an instance against a schema with an enum type. Populates the
 * |errors| property, and returns a boolean indicating whether the instance
 * validates.
 */
chromeHidden.JSONSchemaValidator.prototype.validateEnum = function(
    instance, schema, path) {
  for (var i = 0; i < schema.enum.length; i++) {
    if (instance === schema.enum[i])
      return true;
  }

  this.addError(path, "invalidEnum", [schema.enum.join(", ")]);
  return false;
};

/**
 * Validates an instance against an object schema and populates the errors
 * property.
 */
chromeHidden.JSONSchemaValidator.prototype.validateObject = function(
    instance, schema, path) {
  for (var prop in schema.properties) {
    // It is common in JavaScript to add properties to Object.prototype. This
    // check prevents such additions from being interpreted as required schema
    // properties.
    // TODO(aa): If it ever turns out that we actually want this to work, there
    // are other checks we could put here, like requiring that schema properties
    // be objects that have a 'type' property.
    if (!schema.properties.hasOwnProperty(prop))
      continue;

    var propPath = path ? path + "." + prop : prop;
    if (schema.properties[prop] == undefined) {
      this.addError(propPath, "invalidPropertyType");
    } else if (prop in instance && instance[prop] !== undefined) {
      this.validate(instance[prop], schema.properties[prop], propPath);
    } else if (!schema.properties[prop].optional) {
      this.addError(propPath, "propertyRequired");
    }
  }

  // If "instanceof" property is set, check that this object inherits from
  // the specified constructor (function).
  if (schema.isInstanceOf) {
    var isInstance = function() {
      var constructor = this[schema.isInstanceOf];
      if (constructor) {
        return (instance instanceof constructor);
      }

      // Special-case constructors that can not always be found on the global
      // object, but for which we to allow validation.
      var allowedNamedConstructors = {
        "DOMWindow": true,
        "ImageData": true
      }
      if (!allowedNamedConstructors[schema.isInstanceOf]) {
        throw "Attempt to validate against an instance ctor that could not be" +
              "found: " + schema.isInstanceOf;
      }
      return (schema.isInstanceOf == instance.constructor.name)
    }();

    if (!isInstance)
      this.addError(propPath, "notInstance", [schema.isInstanceOf]);
  }

  // Exit early from additional property check if "type":"any" is defined.
  if (schema.additionalProperties &&
      schema.additionalProperties.type &&
      schema.additionalProperties.type == "any") {
    return;
  }

  // By default, additional properties are not allowed on instance objects. This
  // can be overridden by setting the additionalProperties property to a schema
  // which any additional properties must validate against.
  for (var prop in instance) {
    if (prop in schema.properties)
      continue;

    // Any properties inherited through the prototype are ignored.
    if (!instance.hasOwnProperty(prop))
      continue;

    var propPath = path ? path + "." + prop : prop;
    if (schema.additionalProperties)
      this.validate(instance[prop], schema.additionalProperties, propPath);
    else
      this.addError(propPath, "unexpectedProperty");
  }
};

/**
 * Validates an instance against an array schema and populates the errors
 * property.
 */
chromeHidden.JSONSchemaValidator.prototype.validateArray = function(
    instance, schema, path) {
  var typeOfItems = chromeHidden.JSONSchemaValidator.getType(schema.items);

  if (typeOfItems == 'object') {
    if (schema.minItems && instance.length < schema.minItems) {
      this.addError(path, "arrayMinItems", [schema.minItems]);
    }

    if (typeof schema.maxItems != "undefined" &&
        instance.length > schema.maxItems) {
      this.addError(path, "arrayMaxItems", [schema.maxItems]);
    }

    // If the items property is a single schema, each item in the array must
    // have that schema.
    for (var i = 0; i < instance.length; i++) {
      this.validate(instance[i], schema.items, path + "." + i);
    }
  } else if (typeOfItems == 'array') {
    // If the items property is an array of schemas, each item in the array must
    // validate against the corresponding schema.
    for (var i = 0; i < schema.items.length; i++) {
      var itemPath = path ? path + "." + i : String(i);
      if (i in instance && instance[i] !== null && instance[i] !== undefined) {
        this.validate(instance[i], schema.items[i], itemPath);
      } else if (!schema.items[i].optional) {
        this.addError(itemPath, "itemRequired");
      }
    }

    if (schema.additionalProperties) {
      for (var i = schema.items.length; i < instance.length; i++) {
        var itemPath = path ? path + "." + i : String(i);
        this.validate(instance[i], schema.additionalProperties, itemPath);
      }
    } else {
      if (instance.length > schema.items.length) {
        this.addError(path, "arrayMaxItems", [schema.items.length]);
      }
    }
  }
};

/**
 * Validates a string and populates the errors property.
 */
chromeHidden.JSONSchemaValidator.prototype.validateString = function(
    instance, schema, path) {
  if (schema.minLength && instance.length < schema.minLength)
    this.addError(path, "stringMinLength", [schema.minLength]);

  if (schema.maxLength && instance.length > schema.maxLength)
    this.addError(path, "stringMaxLength", [schema.maxLength]);

  if (schema.pattern && !schema.pattern.test(instance))
    this.addError(path, "stringPattern", [schema.pattern]);
};

/**
 * Validates a number and populates the errors property. The instance is
 * assumed to be a number.
 */
chromeHidden.JSONSchemaValidator.prototype.validateNumber = function(
    instance, schema, path) {

  // Forbid NaN, +Infinity, and -Infinity.  Our APIs don't use them, and
  // JSON serialization encodes them as 'null'.  Re-evaluate supporting
  // them if we add an API that could reasonably take them as a parameter.
  if (isNaN(instance) ||
      instance == Number.POSITIVE_INFINITY ||
      instance == Number.NEGATIVE_INFINITY )
    this.addError(path, "numberFiniteNotNan", [instance]);

  if (schema.minimum && instance < schema.minimum)
    this.addError(path, "numberMinValue", [schema.minimum]);

  if (schema.maximum && instance > schema.maximum)
    this.addError(path, "numberMaxValue", [schema.maximum]);

  if (schema.maxDecimal && instance * Math.pow(10, schema.maxDecimal) % 1)
    this.addError(path, "numberMaxDecimal", [schema.maxDecimal]);
};

/**
 * Validates the primitive type of an instance and populates the errors
 * property. Returns true if the instance validates, false otherwise.
 */
chromeHidden.JSONSchemaValidator.prototype.validateType = function(
    instance, schema, path) {
  var actualType = chromeHidden.JSONSchemaValidator.getType(instance);
  if (schema.type != actualType && !(schema.type == "number" &&
      actualType == "integer")) {
    this.addError(path, "invalidType", [schema.type, actualType]);
    return false;
  }

  return true;
};

/**
 * Adds an error message. |key| is an index into the |messages| object.
 * |replacements| is an array of values to replace '*' characters in the
 * message.
 */
chromeHidden.JSONSchemaValidator.prototype.addError = function(
    path, key, replacements) {
  this.errors.push({
    path: path,
    message: chromeHidden.JSONSchemaValidator.formatError(key, replacements)
  });
};

})();
