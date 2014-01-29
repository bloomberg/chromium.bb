// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var schemaRegistry = requireNative('schema_registry');
var CHECK = requireNative('logging').CHECK;
var WARNING = requireNative('logging').WARNING;

// An object forEach. Calls |f| with each (key, value) pair of |obj|, using
// |self| as the target.
function forEach(obj, f, self) {
  for (var key in obj) {
    if ($Object.hasOwnProperty(obj, key))
      $Function.call(f, self, key, obj[key]);
  }
}

// Assuming |array_of_dictionaries| is structured like this:
// [{id: 1, ... }, {id: 2, ...}, ...], you can use
// lookup(array_of_dictionaries, 'id', 2) to get the dictionary with id == 2.
function lookup(array_of_dictionaries, field, value) {
  var filter = function (dict) {return dict[field] == value;};
  var matches = array_of_dictionaries.filter(filter);
  if (matches.length == 0) {
    return undefined;
  } else if (matches.length == 1) {
    return matches[0]
  } else {
    throw new Error("Failed lookup of field '" + field + "' with value '" +
                    value + "'");
  }
}

function loadTypeSchema(typeName, defaultSchema) {
  var parts = $String.split(typeName, '.');
  if (parts.length == 1) {
    if (defaultSchema == null) {
      WARNING('Trying to reference "' + typeName + '" ' +
              'with neither namespace nor default schema.');
      return null;
    }
    var types = defaultSchema.types;
  } else {
    var schemaName = $Array.join($Array.slice(parts, 0, parts.length - 1), '.');
    var types = schemaRegistry.GetSchema(schemaName).types;
  }
  for (var i = 0; i < types.length; ++i) {
    if (types[i].id == typeName)
      return types[i];
  }
  return null;
}

// expose takes a private class implementation |cls| and exposes a subset of its
// methods |funcs| and properties |props| in a public wrapper class that it
// returns.
function expose(cls, funcs, props) {
  function publicClass() {
    var privateObj = $Object.create(cls.prototype);
    $Function.apply(cls, privateObj, arguments);
    privateObj.wrapper = this;
    privates(this).impl = privateObj;
  }

  if (funcs) {
    $Array.forEach(funcs, function(func) {
      publicClass.prototype[func] = function() {
        var impl = privates(this).impl;
        return $Function.apply(impl[func], impl, arguments);
      };
    });
  }

  if (props) {
    $Array.forEach(props, function(prop) {
      $Object.defineProperty(publicClass.prototype, prop, {
        enumerable: true,
        get: function() {
          return privates(this).impl[prop];
        },
        set: function(value) {
          var impl = privates(this).impl;
          delete impl[prop];
          impl[prop] = value;
        }
      });
    });
  }

  return publicClass;
}

exports.forEach = forEach;
exports.loadTypeSchema = loadTypeSchema;
exports.lookup = lookup;
exports.expose = expose;
