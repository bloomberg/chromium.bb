// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
  'use strict';

  /**
   * The timeout in ms before which a stream is closed.
   * @const {number}
   */
  var STREAM_TIMEOUT = 10000;

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
   * @return {boolean} Whether the stream has been sent to the viewer.
   */
  function flush(viewId) {
    if (viewId in streams && viewId in pluginInitFunctions) {
      var streamDetails = streams[viewId];
      // If the stream has been aborted we pass the stream details to the PDF
      // viewer with the stream URL removed. This allows the viewer to report an
      // error to the user rather than trying to load from an invalid stream
      // URL.
      if (streamDetails.aborted)
        delete streamDetails.streamUrl;
      pluginInitFunctions[viewId](streamDetails);
      delete streams[viewId];
      delete pluginInitFunctions[viewId];
      if (!streamDetails.aborted) {
        // Once a stream has been opened, by the plugin, it is safe to abort.
        // Abort after STREAM_TIMEOUT in case the plugin was closed without
        // reading from the stream.
        //
        // This is a temporary work-around until the streamsPrivate API is made
        // more robust to its clients failing to finish reading from or abort
        // streams.
        setTimeout(function() {
          chrome.streamsPrivate.abort(streamDetails.streamUrl);
        }, STREAM_TIMEOUT);
      }
      return true;
    }
    return false;
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
      if (!flush(streamDetails.viewId)) {
        // If a stream has not been claimed by a PDF viewer instance within
        // STREAM_TIMEOUT, abort the stream.
        setTimeout(function() {
          if (streamDetails.viewId in streams) {
            chrome.streamsPrivate.abort(streamDetails.streamUrl);
            streamDetails.aborted = true;
          }
        }, STREAM_TIMEOUT);
      }
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
