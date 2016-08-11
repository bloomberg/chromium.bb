// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('settings_subpage', function() {
  function registerTests() {
    suite('SettingsSubpage', function() {
      test('can navigate to parent', function() {
        PolymerTest.clearBody();

        // Choose CERTIFICATES since it is not a descendant of BASIC.
        settings.navigateTo(settings.Route.CERTIFICATES);
        assertEquals(settings.Route.CERTIFICATES, settings.getCurrentRoute());

        var subpage = document.createElement('settings-subpage');
        document.body.appendChild(subpage);

        MockInteractions.tap(subpage.$$('paper-icon-button'));
        assertEquals(settings.Route.PRIVACY, settings.getCurrentRoute());
      });

      test('can navigate to grandparent using window.back()', function(done) {
        PolymerTest.clearBody();

        settings.navigateTo(settings.Route.BASIC);
        settings.navigateTo(settings.Route.SYNC);
        assertEquals(settings.Route.SYNC, settings.getCurrentRoute());

        var subpage = document.createElement('settings-subpage');
        document.body.appendChild(subpage);

        MockInteractions.tap(subpage.$$('paper-icon-button'));

        // Since the previous history entry is an ancestor, we expect
        // window.history.back() to be called and a popstate event to be fired.
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
