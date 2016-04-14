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
      'editStartupPage',
      'loadStartupPages',
      'removeStartupPage',
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
    editStartupPage: function(modelIndex, url) {
      this.methodCalled('editStartupPage', [modelIndex, url]);
      return Promise.resolve(this.urlIsValid_);
    },

    /** @override */
    loadStartupPages: function() {
      this.methodCalled('loadStartupPages');
    },

    /** @override */
    removeStartupPage: function(modelIndex) {
      this.methodCalled('removeStartupPage', modelIndex);
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
    });

    teardown(function() { dialog.remove(); });

    test('Initialization_Add', function() {
      document.body.appendChild(dialog);
      assertTrue(dialog.$.dialog.opened);

      // Assert that the "Add" button is disabled.
      var actionButton = dialog.$.actionButton;
      assertTrue(!!actionButton);
      assertTrue(actionButton.disabled);

      // Assert that the text field is empty.
      var inputElement = dialog.$.url;
      assertTrue(!!inputElement);
      assertEquals('', inputElement.value);
    });

    test('Initialization_Edit', function() {
      dialog.model = createSampleUrlEntry();
      document.body.appendChild(dialog);
      assertTrue(dialog.$.dialog.opened);

      // Assert that the "Edit" button is enabled.
      var actionButton = dialog.$.actionButton;
      assertTrue(!!actionButton);
      assertFalse(actionButton.disabled);
      // Assert that the text field is pre-populated.
      var inputElement = dialog.$.url;
      assertTrue(!!inputElement);
      assertEquals(dialog.model.url, inputElement.value);
    });

    // Test that validation occurs as the user is typing, and that the action
    // button is updated accordingly.
    test('Validation', function() {
      document.body.appendChild(dialog);

      var actionButton = dialog.$.actionButton;
      assertTrue(actionButton.disabled);
      var inputElement = dialog.$.url;

      var expectedUrl = "dummy-foo.com";
      inputElement.value = expectedUrl;
      browserProxy.setUrlValidity(false);
      pressSpace(inputElement);

      return browserProxy.whenCalled('validateStartupPage').then(function(url) {
        assertEquals(expectedUrl, url);
        assertTrue(actionButton.disabled);

        browserProxy.setUrlValidity(true);
        browserProxy.resetResolver('validateStartupPage');
        pressSpace(inputElement);

        return browserProxy.whenCalled('validateStartupPage');
      }).then(function() {
        assertFalse(actionButton.disabled);
      });
    });

    /**
     * Tests that the appropritae browser proxy method is called when the action
     * button is tapped.
     * @param {string} proxyMethodName
     */
    function testProxyCalled(proxyMethodName) {
      var actionButton = dialog.$.actionButton;
      actionButton.disabled = false;

      // Test that the dialog remains open if the user somehow manages to submit
      // an invalid URL.
      browserProxy.setUrlValidity(false);
      MockInteractions.tap(actionButton);
      return browserProxy.whenCalled(proxyMethodName).then(function() {
        assertTrue(dialog.$.dialog.opened);

        // Test that dialog is closed if the user submits a valid URL.
        browserProxy.setUrlValidity(true);
        browserProxy.resetResolver(proxyMethodName);
        MockInteractions.tap(actionButton);
        return browserProxy.whenCalled(proxyMethodName);
      }).then(function() {
        assertFalse(dialog.$.dialog.opened);
      });
    }

    test('AddStartupPage', function() {
      document.body.appendChild(dialog);
      return testProxyCalled('addStartupPage');
    });

    test('EditStartupPage', function() {
      dialog.model = createSampleUrlEntry();
      document.body.appendChild(dialog);
      return testProxyCalled('editStartupPage');
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

    test('EditPage_OpensDialog', function() {
      assertFalse(!!page.$$('settings-startup-url-dialog'));
      page.fire(settings.EDIT_STARTUP_URL_EVENT, createSampleUrlEntry());
      Polymer.dom.flush();
      assertTrue(!!page.$$('settings-startup-url-dialog'));
    });
  });

  /** @return {!StartupPageInfo} */
  function createSampleUrlEntry() {
    return {
      modelIndex: 2,
      title: 'Test page',
      tooltip: 'test tooltip',
      url: 'chrome://foo',
    };
  }

  suite('StartupUrlEntry', function() {
    /** @type {?SettingsStartupUrlEntryElement} */
    var element = null;

    var browserProxy = null;

    setup(function() {
      browserProxy = new TestStartupUrlsPageBrowserProxy();
      settings.StartupUrlsPageBrowserProxyImpl.instance_ = browserProxy;
      PolymerTest.clearBody();
      element = document.createElement('settings-startup-url-entry');
      element.model = createSampleUrlEntry();
      document.body.appendChild(element);

      // Bring up the popup menu for the following tests to use.
      assertFalse(!!element.$$('iron-dropdown'));
      MockInteractions.tap(element.$.dots);
      Polymer.dom.flush();
      assertTrue(!!element.$$('iron-dropdown'));
    });

    teardown(function() { element.remove(); });

    test('MenuOptions_Remove', function() {
      var removeButton = element.shadowRoot.querySelector('#remove')
      MockInteractions.tap(removeButton);
      return browserProxy.whenCalled('removeStartupPage').then(
          function(modelIndex) {
            assertEquals(element.model.modelIndex, modelIndex);
          });
    });
  });
});
