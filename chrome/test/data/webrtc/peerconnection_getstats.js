/**
 * Copyright 2016 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

// Public interface to tests. These are expected to be called with
// ExecuteJavascript invocations from the browser tests and will return answers
// through the DOM automation controller.

/**
 * Verifies that the promise-based |RTCPeerConnection.getStats| returns stats.
 *
 * Returns ok-got-stats on success.
 */
function verifyStatsGeneratedPromise() {
  peerConnection_().getStats()
    .then(function(report) {
      if (report == null || report.size == 0)
        throw new failTest('report is null or empty.');
      // Sanity check that applies to all stats.
      var ids = new Set();
      report.forEach(function(stats) {
        if (typeof(stats.id) !== 'string')
          throw failTest('stats.id is not a string.');
        if (ids.has(stats.id))
          throw failTest('stats.id is not a unique identifier.');
        ids.add(stats.id);
        if (typeof(stats.timestamp) !== 'number' || stats.timestamp <= 0)
          throw failTest('stats.timestamp is not a positive number.');
        if (typeof(stats.type) !== 'string')
          throw failTest('stats.type is not a string.');
      });
      // TODO(hbos): When the new stats collection API is more mature (and
      // certainly before unflagging the new stats API) add a whitelist of
      // allowed stats to prevent accidentally exposing stats to the web that
      // are not in the spec and that verifies type information. Status at
      // crbug.com/627816. Stats collection is tested in the WebRTC repo and
      // automatically surfaced to Blink, but there should be a process of
      // having to land a Blink CL in order to expose a new RTCStats dictionary.
      returnToTest('ok-got-stats');
    },
    function(e) {
      throw failTest('Promise was rejected: ' + e);
    });
}
