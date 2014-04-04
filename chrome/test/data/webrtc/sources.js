/**
 * Copyright 2014 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * Queries for video sources on the current system using the getSources API.
 *
 * This does not guarantee that a getUserMedia with video will succeed, as the
 * camera could be busy for instance.
 *
 * Returns has-video-source to the test if there is a webcam available,
 * no-video-sources otherwise.
 */
function HasVideoSourceOnSystem() {
  MediaStreamTrack.getSources(function(sources) {
    var hasVideoSource = false;
    sources.forEach(function(source) {
      if (source.kind == 'video')
        hasVideoSource = true;
    });

    if (hasVideoSource)
      returnToTest('has-video-source');
    else
      returnToTest('no-video-sources');
  });
}