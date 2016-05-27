// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Tests for chrome://plugins
 */

/** @const {string} Path to source root. */
var ROOT_PATH = '../../../../';

/**
 * Test fixture for testing async methods of cr.js.
 * @constructor
 * @extends testing.Test
 */
function PluginsTest() {
  this.browserProxy = null;
}

PluginsTest.prototype = {
  __proto__: testing.Test.prototype,

  /** @override */
  browsePreload: 'chrome://plugins',

  /** @override */
  isAsync: true,

  /** @override */
  runAccessibilityChecks: false,

  /** @override */
  extraLibraries: [
    ROOT_PATH + 'third_party/mocha/mocha.js',
    ROOT_PATH + 'chrome/test/data/webui/mocha_adapter.js',
    ROOT_PATH + 'ui/webui/resources/js/promise_resolver.js',
    ROOT_PATH + 'ui/webui/resources/js/cr.js',
    ROOT_PATH + 'chrome/test/data/webui/settings/test_browser_proxy.js',
  ],

  preLoad: function() {
    /**
     * A test browser proxy for the chrome://plugins page.
     *
     * @constructor
     * @extends {TestBrowserProxyBase}
     */
    var TestBrowserProxy = function() {
      settings.TestBrowserProxy.call(this, [
        'getPluginsData',
        'getShowDetails',
        'saveShowDetailsToPrefs',
      ]);

      /**
       * The data to be returned by |getPluginsData_|.
       * @private
       */
      this.pluginsData_ = [];
    };

    TestBrowserProxy.prototype = {
      __proto__: settings.TestBrowserProxy.prototype,

      getPluginsData: function() {
        this.methodCalled('getPluginsData');
        return Promise.resolve({plugins: this.pluginsData_});
      },

      setPluginsData: function(pluginsData) {
        this.pluginsData_ = pluginsData;
      },

      getShowDetails: function() {
        this.methodCalled('getShowDetails');
        return Promise.resolve({show_details: false});
      },

      saveShowDetailsToPrefs: function(expanded) {
        this.methodCalled('saveShowDetailsToPrefs', expanded);
      },

      setClientPage: function() {
        // Nothing to do here.
      },
    };

    // A function that is called from chrome://plugins to allow this test to
    // replace the real Mojo browser proxy with a fake one, before any other
    // code runs.
    window.setupFn = function() {
      return importModules([
        'mojo/public/js/bindings',
        'mojo/public/js/connection',
        'chrome/browser/ui/webui/plugins/plugins.mojom',
        'content/public/renderer/frame_service_registry',
      ]).then(function(modules) {
        var bindings = modules[0];
        var connection = modules[1];
        var pluginsMojom = modules[2];
        var serviceProvider = modules[3];

        serviceProvider.addServiceOverrideForTesting(
            pluginsMojom.PluginsPageHandler.name, function(handle) {
              var stub = connection.bindHandleToStub(
                  handle, pluginsMojom.PluginsPageHandler);
              this.browserProxy = new TestBrowserProxy();
              bindings.StubBindings(stub).delegate = this.browserProxy;
            }.bind(this));
      }.bind(this));
    }.bind(this);
  },
};

TEST_F('PluginsTest', 'Plugins', function() {
  var browserProxy = this.browserProxy;

  var fakePluginData = {
    name: 'Group Name',
    description: 'description',
    version: 'version',
    update_url: 'http://update/',
    critical: true,
    enabled_mode: 'enabledByUser',
    id: 'plugin-name',
    always_allowed: false,
    plugin_files: [
      {
        path: '/foo/bar/baz/MyPlugin.plugin',
        name: 'MyPlugin',
        version: '1.2.3',
        description: 'My plugin',
        type: 'BROWSER PLUGIN',
        mime_types: [
          {
            description: 'Foo Media',
            file_extensions: ['pdf'],
            mime_type: 'application/x-my-foo'
          },
          {
            description: 'Bar Stuff',
            file_extensions: ['bar', 'baz'],
            mime_type: 'application/my-bar'
          },
        ],
        enabled_mode: 'enabledByUser',
      },
    ],
  };

  suite('PluginTest', function() {
    var EXPECTED_PLUGINS = 2;
    suiteSetup(function() {
      browserProxy.setPluginsData([fakePluginData, fakePluginData]);
      return Promise.all([
        browserProxy.whenCalled('getPluginsData'),
        browserProxy.whenCalled('getShowDetails'),
      ]);
    });

    teardown(function() { browserProxy.reset(); });

    test('PluginsUpdated', function() {
      var plugins = document.querySelectorAll('.plugin');
      assertEquals(EXPECTED_PLUGINS, plugins.length);

      pageImpl.onPluginsUpdated([fakePluginData]);
      plugins = document.querySelectorAll('.plugin');
      assertEquals(1, plugins.length);
    });

    /**
     * Test that clicking on the +/- icons results in the expected calls to the
     * browser.
     */
    test('ToggleDetails', function() {
      var collapseEl = document.querySelector('#collapse');
      var expandEl = document.querySelector('#expand');

      var clickEvent = new MouseEvent('click', {
        'view': window, 'bubbles': true, 'cancelable': true
      });

      assertEquals('none', collapseEl.style.display);
      assertNotEquals('none', expandEl.style.display);

      expandEl.dispatchEvent(clickEvent);
      return browserProxy.whenCalled('saveShowDetailsToPrefs').then(
          function(expanded) {
            // Booleans are converted to 0/1 by underlying Mojo layers.
            assertTrue(Boolean(expanded));
            assertNotEquals('none', collapseEl.style.display);
            assertEquals('none', expandEl.style.display);

            browserProxy.resetResolver('saveShowDetailsToPrefs');
            collapseEl.dispatchEvent(clickEvent);
            return browserProxy.whenCalled('saveShowDetailsToPrefs');
          }).then(function(expanded) {
            assertFalse(Boolean(expanded));
          });
    });
  });

  // Run all registered tests.
  mocha.run();
});
