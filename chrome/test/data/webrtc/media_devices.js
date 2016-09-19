/**
 * Copyright 2014 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * Queries for media devices on the current system using the getMediaDevices
 * API.
 *
 * Returns the list of devices available.
 */
function getMediaDevices() {
  navigator.getMediaDevices(function(devices) {
    returnToTest(JSON.stringify(devices));
  });
}

/**
 * Queries for media sources on the current system using the getSources API.
 *
 * Returns the list of sources available.
 */
function getSources() {
  MediaStreamTrack.getSources(function(sources) {
    returnToTest(JSON.stringify(sources));
  });
}

