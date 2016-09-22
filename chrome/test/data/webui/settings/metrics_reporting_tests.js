// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('metrics reporting', function() {
  /** @type {settings.TestPrivacyPageBrowserProxy} */
  var testBrowserProxy;

  /** @type {SettingsPrivacyPageElement} */
  var page;

  setup(function() {
    testBrowserProxy = new TestPrivacyPageBrowserProxy();
    settings.PrivacyPageBrowserProxyImpl.instance_ = testBrowserProxy;
    PolymerTest.clearBody();
    page = document.createElement('settings-privacy-page');
  });

  teardown(function() { page.remove(); });

  test('changes to whether metrics reporting is enabled/managed', function() {
    return testBrowserProxy.whenCalled('getMetricsReporting').then(function() {
      Polymer.dom.flush();

      var checkbox = page.$.metricsReportingCheckbox;
      assertEquals(testBrowserProxy.metricsReporting.enabled, checkbox.checked);
      var indicatorVisible = !!page.$$('#indicator');
      assertEquals(testBrowserProxy.metricsReporting.managed, indicatorVisible);

      var changedMetrics = {
        enabled: !testBrowserProxy.metricsReporting.enabled,
        managed: !testBrowserProxy.metricsReporting.managed,
      };
      cr.webUIListenerCallback('metrics-reporting-change', changedMetrics);
      Polymer.dom.flush();

      assertEquals(changedMetrics.enabled, checkbox.checked);
      indicatorVisible = !!page.$$('#indicator');
      assertEquals(changedMetrics.managed, indicatorVisible);

      var toggled = !changedMetrics.enabled;

      MockInteractions.tap(checkbox);
      return testBrowserProxy.whenCalled('setMetricsReportingEnabled', toggled);
    });
  });
});
