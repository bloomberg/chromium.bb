// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Redirects to the same page on a different branch.
 *
 * @param {String} The path to redirect to.
 */
(function() {
  function redirectToBranch(value) {
    if (!value)
      return;
    var path = window.location.pathname.split('/')
    window.location = value + '/' + path[path.length - 1];
  }

  window.redirectToBranch = redirectToBranch;
})()
