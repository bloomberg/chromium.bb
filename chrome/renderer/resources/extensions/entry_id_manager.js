// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var nameToIds = {};
var idsToEntries = {};

function computeName(entry) {
  return entry.filesystem.name + ':' + entry.fullPath;
}

function registerEntry(id, entry) {
  var name = computeName(entry);
  nameToIds[name] = id;
  idsToEntries[id] = entry;
};

function getEntryId(entry) {
  var name = null;
  try {
    name = computeName(entry);
  } catch(e) {
    return null;
  }
  return nameToIds[name];
}

function getEntryById(id) {
  return idsToEntries[id];
}

exports.registerEntry = registerEntry;
exports.getEntryId = getEntryId;
exports.getEntryById = getEntryById;
