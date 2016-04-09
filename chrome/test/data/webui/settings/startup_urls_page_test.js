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
      'addStartupPage',
      'loadStartupPages',
      'useCurrentPages',
      'validateStartupPage',
    ]);

    /** @private {boolean} */
    this.urlIsValid_ = true;
  }

  TestStartupUrlsPageBrowserProxy.prototype = {
    __proto__: settings.TestBrowserProxy.prototype,

    /** @param {boolean} isValid */
    setUrlValidity: function(isValid) {
      this.urlIsValid_ = isValid;
    },

    /** @override */
    addStartupPage: function(url) {
      this.methodCalled('addStartupPage', url);
      return Promise.resolve(this.urlIsValid_);
    },

    /** @override */
    loadStartupPages: function() {
      this.methodCalled('loadStartupPages');
    },

    /** @override */
    useCurrentPages: function() {
      this.methodCalled('useCurrentPages');
    },

    /** @override */
    validateStartupPage: function(url) {
      this.methodCalled('validateStartupPage', url);
      return Promise.resolve(this.urlIsValid_);
    },
  };

  suite('StartupUrlDialog', function() {
    /** @type {?SettingsStartupUrlDialogElement} */
    var dialog = null;

    var browserProxy = null;

    /**
     * Triggers an 'input' event on the given text input field, which triggers
     * validation to occur.
     * @param {!PaperInputElement} element
     */
    function pressSpace(element) {
      // The actual key code is irrelevant for these tests.
      MockInteractions.keyEventOn(element, 'input', 32 /* space key code */);
    }

    setup(function() {
      browserProxy = new TestStartupUrlsPageBrowserProxy();
      settings.StartupUrlsPageBrowserProxyImpl.instance_ = browserProxy;
      PolymerTest.clearBody();
      dialog = document.createElement('settings-startup-url-dialog');
      document.body.appendChild(dialog);
    });

    teardown(function() { dialog.remove(); });

    // Test that validation occurs as the user is typing, and that the action
    // button is updated accordingly.
    test('Validation', function() {
      assertTrue(dialog.$.dialog.opened);
      var addButton = dialog.$.add;
      assertTrue(!!addButton);
      assertTrue(addButton.disabled);

      var inputElement = dialog.$.url;
      assertTrue(!!inputElement);

      var expectedUrl = "dummy-foo.com";
      inputElement.value = expectedUrl;
      browserProxy.setUrlValidity(false);
      pressSpace(inputElement);

      return browserProxy.whenCalled('validateStartupPage').then(function(url) {
        assertEquals(expectedUrl, url);
        assertTrue(addButton.disabled);

        browserProxy.setUrlValidity(true);
        browserProxy.resetResolver('validateStartupPage');
        pressSpace(inputElement);

        return browserProxy.whenCalled('validateStartupPage');
      }).then(function() {
        assertFalse(addButton.disabled);
      });
    });

    test('AddStartupPage', function() {
      assertTrue(dialog.$.dialog.opened);
      var addButton = dialog.$.add;
      addButton.disabled = false;

      // Test that the dialog remains open if the user somehow manages to submit
      // an invalid URL.
      browserProxy.setUrlValidity(false);
      MockInteractions.tap(addButton);
      return browserProxy.whenCalled('addStartupPage').then(function() {
        assertTrue(dialog.$.dialog.opened);

        // Test that dialog is closed if the user submits a valid URL.
        browserProxy.setUrlValidity(true);
        browserProxy.resetResolver('addStartupPage');
        MockInteractions.tap(addButton);
        return browserProxy.whenCalled('addStartupPage');
      }).then(function() {
        assertFalse(dialog.$.dialog.opened);
      });
    });
  });

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

    // Test that the page is requesting information from the browser.
    test('Initialization', function() {
      return browserProxy.whenCalled('loadStartupPages');
    });

    test('UseCurrentPages', function() {
      var useCurrentPagesButton = page.$.useCurrentPages;
      assertTrue(!!useCurrentPagesButton);
      MockInteractions.tap(useCurrentPagesButton);
      return browserProxy.whenCalled('useCurrentPages');
    });

    test('AddPage_OpensDialog', function() {
      var addPageButton = page.$.addPage;
      assertTrue(!!addPageButton);
      assertFalse(!!page.$$('settings-startup-url-dialog'));

      MockInteractions.tap(addPageButton);
      Polymer.dom.flush();
      assertTrue(!!page.$$('settings-startup-url-dialog'));
    });
  });
});
