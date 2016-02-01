// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/*
 * Waits for queued up tasks to finish before proceeding. Inspired by:
 * https://github.com/Polymer/web-component-tester/blob/master/browser/environment/helpers.js#L97
 */
function flush(callback) {
  Polymer.dom.flush();
  window.setTimeout(callback, 0);
}
