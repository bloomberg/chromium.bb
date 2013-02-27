// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var chrome = requireNative('chrome').GetChrome();

function forEach(dict, f) {
  for (var key in dict) {
    if (dict.hasOwnProperty(key))
      f(key, dict[key]);
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

// Specify |currentApi| if this should return an API for $refs in the current
// namespace.
function loadRefDependency(ref, currentApi) {
  var parts = ref.split(".");
  if (parts.length > 1)
    return chrome[parts.slice(0, parts.length - 1).join(".")];
  else
    return currentApi;
}

exports.forEach = forEach;
exports.loadRefDependency = loadRefDependency;
exports.lookup = lookup;
