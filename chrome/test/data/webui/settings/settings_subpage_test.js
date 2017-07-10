// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('settings_subpage', function() {
  suite('SettingsSubpage', function() {
    test('navigates to parent when there is no history', function() {
      PolymerTest.clearBody();

      // Pretend that we initially started on the CERTIFICATES route.
      window.history.replaceState(
          undefined, '', settings.routes.CERTIFICATES.path);
      settings.initializeRouteFromUrl();
      assertEquals(settings.routes.CERTIFICATES, settings.getCurrentRoute());

      var subpage = document.createElement('settings-subpage');
      document.body.appendChild(subpage);

      MockInteractions.tap(subpage.$$('button'));
      assertEquals(settings.routes.PRIVACY, settings.getCurrentRoute());
    });

    test('navigates to any route via window.back()', function(done) {
      PolymerTest.clearBody();

      settings.navigateTo(settings.routes.BASIC);
      settings.navigateTo(settings.routes.SYNC);
      assertEquals(settings.routes.SYNC, settings.getCurrentRoute());

      var subpage = document.createElement('settings-subpage');
      document.body.appendChild(subpage);

      MockInteractions.tap(subpage.$$('button'));

      window.addEventListener('popstate', function(event) {
        assertEquals(settings.routes.BASIC, settings.getCurrentRoute());
        done();
      });
    });
  });

  suite('SettingsSubpageSearch', function() {
    test('host autofocus propagates to <input>', function() {
      PolymerTest.clearBody();
      var element = document.createElement('settings-subpage-search');
      element.setAttribute('autofocus', true);
      document.body.appendChild(element);

      assertTrue(element.$$('input').hasAttribute('autofocus'));

      element.removeAttribute('autofocus');
      assertFalse(element.$$('input').hasAttribute('autofocus'));
    });
  });
});
