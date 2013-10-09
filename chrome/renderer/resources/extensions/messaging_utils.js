// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Routines used to normalize arguments to messaging functions.

function alignSendMessageArguments(args) {
  // Align missing (optional) function arguments with the arguments that
  // schema validation is expecting, e.g.
  //   extension.sendRequest(req)     -> extension.sendRequest(null, req)
  //   extension.sendRequest(req, cb) -> extension.sendRequest(null, req, cb)
  if (!args || !args.length)
    return null;
  var lastArg = args.length - 1;

  // responseCallback (last argument) is optional.
  var responseCallback = null;
  if (typeof(args[lastArg]) == 'function')
    responseCallback = args[lastArg--];

  // request (second argument) is required.
  var request = args[lastArg--];

  // targetId (first argument, extensionId in the manifest) is optional.
  var targetId = null;
  if (lastArg >= 0)
    targetId = args[lastArg--];

  if (lastArg != -1)
    return null;
  return [targetId, request, responseCallback];
}

exports.alignSendMessageArguments = alignSendMessageArguments;
