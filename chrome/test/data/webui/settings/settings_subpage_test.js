// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('settings_subpage', function() {
  function registerTests() {
    suite('SettingsSubpage', function() {
      test('navigates to parent when there is no history', function() {
        PolymerTest.clearBody();

        // Pretend that we initially started on the CERTIFICATES route.
        window.history.replaceState(
            undefined, '', settings.Route.CERTIFICATES.path);
        settings.initializeRouteFromUrl();
        assertEquals(settings.Route.CERTIFICATES, settings.getCurrentRoute());

        var subpage = document.createElement('settings-subpage');
        document.body.appendChild(subpage);

        MockInteractions.tap(subpage.$$('paper-icon-button'));
        assertEquals(settings.Route.PRIVACY, settings.getCurrentRoute());
      });

      test('navigates to any route via window.back()', function(done) {
        PolymerTest.clearBody();

        settings.navigateTo(settings.Route.BASIC);
        settings.navigateTo(settings.Route.SYNC);
        assertEquals(settings.Route.SYNC, settings.getCurrentRoute());

        var subpage = document.createElement('settings-subpage');
        document.body.appendChild(subpage);

        MockInteractions.tap(subpage.$$('paper-icon-button'));

        window.addEventListener('popstate', function(event) {
          assertEquals(settings.Route.BASIC, settings.getCurrentRoute());
          done();
        });
      });
    });
  }

  return {
    registerTests: registerTests,
  };
});
