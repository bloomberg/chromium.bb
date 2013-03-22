// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var chrome = requireNative('chrome').GetChrome();
var schemaRegistry = requireNative('schema_registry');
var CHECK = requireNative('logging').CHECK;

function forEach(obj, f, self) {
  // For arrays, make sure the indices are numbers not strings - and in that
  // case this method can be more efficient by assuming the array isn't sparse.
  //
  // Note: not (instanceof Array) because the array might come from a different
  // context than our own.
  if (obj.constructor && obj.constructor.name == 'Array') {
    for (var i = 0; i < obj.length; i++)
      f.call(self, i, obj[i]);
  } else {
    for (var key in obj) {
      if (obj.hasOwnProperty(key))
        f.call(self, key, obj[key]);
    }
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
  var parts = typeName.split('.');
  if (parts.length == 1) {
    CHECK(defaultSchema, 'Trying to reference "' + typeName +
        '" with neither namespace nor default schema.');
    var types = defaultSchema.types;
  } else {
    var schemaName = parts.slice(0, parts.length - 1).join('.')
    var types = schemaRegistry.GetSchema(schemaName).types;
  }
  for (var i = 0; i < types.length; ++i) {
    if (types[i].id == typeName)
      return types[i];
  }
  return null;
}

exports.forEach = forEach;
exports.loadTypeSchema = loadTypeSchema;
exports.lookup = lookup;
