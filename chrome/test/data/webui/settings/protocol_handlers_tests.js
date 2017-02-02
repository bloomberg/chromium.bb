// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for protocol_handlers. */
cr.define('protocol_handlers', function() {
  function registerTests() {
    suite('ProtocolHandlers', function() {
      /**
       * A dummy protocol handler element created before each test.
       * @type {ProtocolHandlers}
       */
      var testElement;

      /**
       * A list of ProtocolEntry fixtures.
       * @type {!Array<!ProtocolEntry>}
       */
      var protocols = [
        {
          default_handler: 0,
          handlers: [
            {
              host: 'www.google.com',
              protocol: 'mailto',
              spec: 'http://www.google.com/%s'
            }
          ],
          has_policy_recommendations: false,
          is_default_handler_set_by_user: true,
          protocol: 'mailto'
        }, {
          default_handler: 0,
          handlers: [
            {
              host: 'www.google1.com',
              protocol: 'webcal',
              spec: 'http://www.google1.com/%s'
            }, {
              host: 'www.google2.com',
              protocol: 'webcal',
              spec: 'http://www.google2.com/%s'
            }
          ],
          has_policy_recommendations: false,
          is_default_handler_set_by_user: true,
          protocol: 'webcal'
        }
      ];

      /**
       * The mock proxy object to use during test.
       * @type {TestSiteSettingsPrefsBrowserProxy}
       */
      var browserProxy = null;

      setup(function() {
        browserProxy = new TestSiteSettingsPrefsBrowserProxy();
        settings.SiteSettingsPrefsBrowserProxyImpl.instance_ = browserProxy;
      });

      teardown(function() {
        testElement.remove();
        testElement = null;
      });

      /** @return {!Promise} */
      function initPage() {
        browserProxy.reset();
        PolymerTest.clearBody();
        testElement = document.createElement('protocol-handlers');
        document.body.appendChild(testElement);
        return browserProxy.whenCalled('observeProtocolHandlers').
            then(Polymer.dom.flush.bind(Polymer.dom));
      }

      test('empty list', function() {
        return initPage().then(function(){
          var listFrames = testElement.root.querySelectorAll('.list-frame');
          assertEquals(0, listFrames.length);
        });
      });

      test('non-empty list', function() {
        browserProxy.setProtocolHandlers(protocols);

        return initPage().then(function(){
          var listFrames = testElement.root.querySelectorAll('.list-frame');
          var listItems = testElement.root.querySelectorAll('.list-item');
          // There are two protocols: ["mailto", "webcal"].
          assertEquals(2, listFrames.length);
          // There are three total handlers within the two protocols.
          assertEquals(3, listItems.length);

          // Check that item hosts are rendered correctly.
          var hosts = testElement.root.querySelectorAll('.protocol-host');
          assertEquals('www.google.com', hosts[0].textContent);
          assertEquals('www.google1.com', hosts[1].textContent);
          assertEquals('www.google2.com', hosts[2].textContent);

          // Check that item default subtexts are rendered correctly.
          var defText = testElement.root.querySelectorAll('.protocol-default');
          assertFalse(defText[0].hidden);
          assertFalse(defText[1].hidden);
          assertTrue(defText[2].hidden);
        });
      });

      /**
       * A reusable function to test different action buttons.
       * @param {string} button id of the button to test.
       * @param {string} handler name of browserProxy handler to react.
       * @return {!Promise}
       */
      function testButtonFlow(button, browserProxyHandler) {
        var menuButtons, functionButton, dialog;

        return initPage().then(function(){
          // Initiating the elements
          menuButtons = testElement.root.
              querySelectorAll('paper-icon-button');
          functionButton = testElement.$[button];
          dialog = testElement.$$('dialog[is=cr-action-menu]');
          assertEquals(3, menuButtons.length);

          // Test the button for the first protocol handler
          MockInteractions.tap(menuButtons[0]);
          assertTrue(dialog.open);

          MockInteractions.tap(functionButton);

          return browserProxy.whenCalled(browserProxyHandler);
        }).then(function(args) {
          // BrowserProxy's handler is expected to be called with arguments as
          // [protocol, url].
          assertEquals(protocols[0].protocol, args[0]);
          assertEquals(protocols[0].handlers[0].spec, args[1]);

          var dialog = testElement.$$('dialog[is=cr-action-menu]');
          assertFalse(dialog.open);

          // Test the button for the second protocol handler
          browserProxy.reset();
          MockInteractions.tap(menuButtons[1]);
          assertTrue(dialog.open);
          MockInteractions.tap(functionButton);

          return browserProxy.whenCalled(browserProxyHandler);
        }).then(function(args) {
          assertEquals(protocols[1].protocol, args[0]);
          assertEquals(protocols[1].handlers[0].spec, args[1]);
        });
      }

      test('remove button works', function() {
        browserProxy.setProtocolHandlers(protocols);
        return testButtonFlow('removeButton', 'removeProtocolHandler');
      });

      test('default button works', function() {
        browserProxy.setProtocolHandlers(protocols);
        return testButtonFlow('defaultButton', 'setProtocolDefault');
      });
    });
  }

  return {
    registerTests: registerTests,
  };
});
