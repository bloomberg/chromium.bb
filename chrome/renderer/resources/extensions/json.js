// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Local implementation of JSON.parse & JSON.stringify that protect us
// from being clobbered by an extension.
//
// Make sure this is run before any extension code runs!
//
// TODO(aa): This makes me so sad. We shouldn't need it, as we can just pass
// Values directly over IPC without serializing to strings and use
// JSONValueConverter.
var classes = [Object, Array, Date, String, Number, Boolean];
var realToJSON = classes.map(function(cls) { return cls.prototype.toJSON; });
var realStringify = JSON.stringify;
var realParse = JSON.parse;

exports.stringify = function stringify(thing) {
  // I guess if we're being this paranoid we shouldn't use any of the methods
  // on Object/Array/etc either (forEach, push, etc).
  var saved = [];
  for (var i = 0; i < classes.length; i++) {
    var prototype = classes[i].prototype;
    if (prototype.toJSON !== realToJSON[i]) {
      saved[i] = prototype.toJSON;
      prototype.toJSON = realToJSON[i];
    }
  }
  try {
    return realStringify(thing);
  } finally {
    for (var i = 0; i < classes.length; i++) {
      if (saved.hasOwnProperty(i))
        classes[i].prototype.toJSON = saved[i];
    }
  }
};

exports.parse = function parse(thing) {
  return realParse(thing);
};
