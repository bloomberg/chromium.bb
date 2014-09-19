// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
  'use strict';

  /**
   * A map of view ID (which identifies a particular PDF viewer instance) to
   * stream object.
   * @type {Object.<string, Object>}
   */
  var streams = {};

  /**
   * A map of view ID (which identifies a particular PDF viewer instance) to
   * initialization function for that view.
   * @type {Object.<string, Function>}
   */
  var pluginInitFunctions = {};

  /**
   * If we have received a stream object and an initialization function for a
   * particular PDF viewer instance we know that the extension has loaded in
   * and we can pass it the stream. We can then delete the corresponding map
   * entries.
   * @param {string} viewId The ID of the view to initialize with a stream.
   */
  function flush(viewId) {
    if (viewId in streams && viewId in pluginInitFunctions) {
      pluginInitFunctions[viewId](streams[viewId]);
      delete streams[viewId];
      delete pluginInitFunctions[viewId];
    }
  }

  /**
   * This is called when loading a document with the PDF mime type and passes a
   * stream that points to the PDF file. This may be run before or after we
   * receive a message from the PDF viewer with its initialization function.
   */
  chrome.streamsPrivate.onExecuteMimeTypeHandler.addListener(
    function(streamDetails) {
      // Store the stream until we are contacted by the PDF viewer that owns the
      // stream.
      streams[streamDetails.viewId] = streamDetails;
      flush(streamDetails.viewId);
    }
  );

  /**
   * This is called when we receive a message from the PDF viewer indicating
   * it has loaded and is ready to receive a stream of the data.
   */
  chrome.runtime.onMessage.addListener(
    function(request, sender, responseFunction) {
      // Store the initialization function until we receive the stream which
      // corresponds to the PDF viewer.
      pluginInitFunctions[request.viewId] = responseFunction;
      flush(request.viewId);
    }
  );
}());
