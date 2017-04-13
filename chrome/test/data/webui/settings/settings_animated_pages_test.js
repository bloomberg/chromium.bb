// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('settings-animated-pages', function() {
  test('focuses subpage trigger when exiting subpage', function(done) {
    document.body.innerHTML = `
      <settings-animated-pages
          section="${settings.Route.SEARCH_ENGINES.section}">
        <neon-animatable route-path="default">
          <button id="subpage-trigger"></button>
        </neon-animatable>
        <neon-animatable route-path="${settings.Route.SEARCH_ENGINES.path}">
          <button id="subpage-trigger"></button>
        </neon-animatable>
      </settings-animated-pages>`;

    var animatedPages = document.body.querySelector('settings-animated-pages');
    animatedPages.focusConfig = new Map();
    animatedPages.focusConfig.set(
        settings.Route.SEARCH_ENGINES.path, '#subpage-trigger');

    var trigger = document.body.querySelector('#subpage-trigger');
    assertTrue(!!trigger);
    trigger.addEventListener('focus', function() { done(); });

    // Trigger subpage exit navigation.
    settings.navigateTo(settings.Route.BASIC);
    settings.navigateTo(settings.Route.SEARCH_ENGINES);
    settings.navigateToPreviousRoute();
  });
});
