// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Custom bindings for the mime handler API.
 */

var binding =
    apiBridge || require('binding').Binding.create('mimeHandlerPrivate');
var utils = require('utils');

var NO_STREAM_ERROR =
    'Streams are only available from a mime handler view guest.';
var STREAM_ABORTED_ERROR = 'Stream has been aborted.';

var servicePromise = Promise.all([
    requireAsync('content/public/renderer/frame_interfaces'),
    requireAsync('extensions/common/api/mime_handler.mojom'),
]).then(function(modules) {
  var frameInterfaces = modules[0];
  var mojom = modules[1];
  return new mojom.MimeHandlerServicePtr(
      frameInterfaces.getInterface(mojom.MimeHandlerService.name));
});

// Stores a promise to the GetStreamInfo() result to avoid making additional
// calls in response to getStreamInfo() calls.
var streamInfoPromise;

function throwNoStreamError() {
  throw new Error(NO_STREAM_ERROR);
}

function createStreamInfoPromise() {
  return servicePromise.then(function(service) {
    return service.getStreamInfo();
  }).then(function(result) {
    if (!result.stream_info)
      throw new Error(STREAM_ABORTED_ERROR);
    return result.stream_info;
  }, throwNoStreamError);
}

function constructStreamInfoDict(streamInfo) {
  var headers = {};
  for (var header of streamInfo.response_headers) {
    headers[header[0]] = header[1];
  }
  return {
    mimeType: streamInfo.mime_type,
    originalUrl: streamInfo.original_url,
    streamUrl: streamInfo.stream_url,
    tabId: streamInfo.tab_id,
    embedded: !!streamInfo.embedded,
    responseHeaders: headers,
  };
}

binding.registerCustomHook(function(bindingsAPI) {
  var apiFunctions = bindingsAPI.apiFunctions;
  utils.handleRequestWithPromiseDoNotUse(
      apiFunctions, 'mimeHandlerPrivate', 'getStreamInfo',
      function() {
    if (!streamInfoPromise)
      streamInfoPromise = createStreamInfoPromise();
    return streamInfoPromise.then(constructStreamInfoDict);
  });

  utils.handleRequestWithPromiseDoNotUse(
      apiFunctions, 'mimeHandlerPrivate', 'abortStream',
      function() {
    return servicePromise.then(function(service) {
      return service.abortStream().then(function() {});
    }).catch(throwNoStreamError);
  });
});

if (!apiBridge)
  exports.$set('binding', binding.generate());
