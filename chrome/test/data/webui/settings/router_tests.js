// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('routing', function() {
  test('no duplicate routes', function() {
    var urls = new Set();
    document.createElement('settings-router').canonicalRoutes_.forEach(
        function(route) {
          assertFalse(urls.has(route.url), route.url);
          urls.add(route.url);
        });
  });
});
