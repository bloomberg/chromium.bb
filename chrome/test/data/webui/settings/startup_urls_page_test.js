// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('settings_startup_urls_page', function() {
  /**
   * @constructor
   * @implements {settings.StartupUrlsPageBrowserProxy}
   * @extends {settings.TestBrowserProxy}
   */
  function TestStartupUrlsPageBrowserProxy() {
    settings.TestBrowserProxy.call(this, [
      'validateStartupPage',
      'addStartupPage',
    ]);
  }

  TestStartupUrlsPageBrowserProxy.prototype = {
    __proto__: settings.TestBrowserProxy.prototype,

    urlsAreValid: false,

    /** @override */
    loadStartupPages: function() {},

    /** @override */
    validateStartupPage: function(url) {
      this.methodCalled('validateStartupPage');
      var resolver = new PromiseResolver;
      resolver.promise = Promise.resolve(this.urlsAreValid);
      return resolver;
    },

    /** @override */
    addStartupPage: function(url) {
      this.methodCalled('addStartupPage');
    },
  };

  suite('StartupUrlsPage', function() {
    /** @type {?SettingsStartupUrlsPageElement} */
    var page = null;

    var browserProxy = null;

    setup(function() {
      browserProxy = new TestStartupUrlsPageBrowserProxy();
      settings.StartupUrlsPageBrowserProxyImpl.instance_ = browserProxy;
      PolymerTest.clearBody();
      page = document.createElement('settings-startup-urls-page');
      document.body.appendChild(page);
    });

    teardown(function() { page.remove(); });

    test('validate', function() {
      browserProxy.whenCalled('validateStartupPage').then(function() {
        var addButton = page.$.add;
        assertTrue(!!addButton);
        assertFalse(browserProxy.urlsAreValid);
        assertTrue(addButton.disabled);

        browserProxy.resetResolver('validateStartupPage');
        browserProxy.whenCalled('validateStartupPage').then(function() {
          page.async(function() {
            assertFalse(addButton.disabled);
            MockInteractions.tap(addButton);
          });
        });

        browserProxy.urlsAreValid = true;
        page.$.newUrl.value = "overriding validation anyway";
      });

      return browserProxy.whenCalled('addStartupPage');
    });
  });
});
