// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var media = {};

var doNothing = function() {};

// Silence the backend calls.
media.initialize = doNothing;
media.addAudioStream = doNothing;
media.cacheEntriesByKey = doNothing;
media.onReceiveEverything = doNothing;
media.onItemDeleted = doNothing;
media.onRendererTerminated = doNothing;
media.onNetUpdate = doNothing;
media.onReceiveConstants = doNothing;
media.onMediaEvent = doNothing;
