// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom bindings for the feedbackPrivate API.

var binding = require('binding').Binding.create('feedbackPrivate');

var feedbackPrivateNatives = requireNative('feedback_private');

binding.registerCustomHook(function(bindingsAPI) {
  var apiFunctions = bindingsAPI.apiFunctions;
  apiFunctions.setUpdateArgumentsPostValidate(
      "sendFeedback", function(feedbackInfo, callback) {
    var attachedFileBlobUrl = '';
    var screenshotBlobUrl = '';

    if (feedbackInfo.attachedFile)
      attachedFileBlobUrl =
          feedbackPrivateNatives.GetBlobUrl(feedbackInfo.attachedFile.data);
    if (feedbackInfo.screenshot)
      screenshotBlobUrl =
          feedbackPrivateNatives.GetBlobUrl(feedbackInfo.screenshot);

    feedbackInfo.attachedFileBlobUrl = attachedFileBlobUrl;
    feedbackInfo.screenshotBlobUrl = screenshotBlobUrl;

    return [feedbackInfo, callback];
  });
});

exports.binding = binding.generate();
