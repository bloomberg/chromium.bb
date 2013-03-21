// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom bindings for the downloads API.

var binding = require('binding').Binding.create('downloads');
var chromeHidden = requireNative('chrome_hidden').GetChromeHidden();

chromeHidden.Event.registerArgumentMassager(
    'downloads.onDeterminingFilename',
    function massage_determining_filename(args, dispatch) {
  var downloadItem = args[0];
  // Copy the id so that extensions can't change it.
  var downloadId = downloadItem.id;
  var suggestable = true;
  function suggestCallback(result) {
    if (!suggestable) {
      console.error('suggestCallback may not be called more than once.');
      return;
    }
    suggestable = false;
    if ((typeof(result) == 'object') &&
        result.filename &&
        (typeof(result.filename) == 'string') &&
        ((result.overwrite === undefined) ||
          (typeof(result.overwrite) == 'boolean'))) {
      chromeHidden.internalAPIs.downloadsInternal.determineFilename(
          downloadId,
          result.filename,
          result.overwrite || false);
    } else {
      chromeHidden.internalAPIs.downloadsInternal.determineFilename(
          downloadId);
    }
  }
  try {
    var results = dispatch([downloadItem, suggestCallback]);
    var async = (results &&
                 results.results &&
                 (results.results.length != 0) &&
                 (results.results[0] === true));
    if (suggestable && !async)
      suggestCallback();
  } catch (e) {
    suggestCallback();
    throw e;
  }
});
exports.binding = binding.generate();
