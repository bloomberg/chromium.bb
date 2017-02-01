// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('settings_rtl_tests', function() {
  /** @implements {settings.DirectionDelegate} */
  function TestDirectionDelegate(isRtl) {
    /** @override */
    this.isRtl = function() { return isRtl; };
  }

  function registerDrawerPanelTests() {
    suite('settings drawer panel RTL tests', function() {
      setup(function() {
        PolymerTest.clearBody();
      });

      test('test i18n processing flips drawer panel', function() {
        var ui = document.createElement('settings-ui');
        var appDrawer = ui.$.drawer;
        assertEquals('left', appDrawer.align);

        ui.directionDelegate = new TestDirectionDelegate(true /* isRtl */);
        assertEquals('right', appDrawer.align);

        ui.directionDelegate = new TestDirectionDelegate(false /* isRtl */);
        assertEquals('left', appDrawer.align);
      });
    });
  }

  return {
    registerDrawerPanelTests: registerDrawerPanelTests,
  };
});
