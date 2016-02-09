// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for route-details. */
cr.define('route_details', function() {
  function registerTests() {
    suite('RouteDetails', function() {
      /**
       * Route Details created before each test.
       * @type {RouteDetails}
       */
      var details;

      /**
       * First fake route created before each test.
       * @type {media_router.Route}
       */
      var fakeRouteOne;

      /**
       * Second fake route created before each test.
       * @type {media_router.Route}
       */
      var fakeRouteTwo;

      // Checks whether |expected| and the text in the span element in
      // the |elementId| element are equal.
      var checkSpanText = function(expected, elementId) {
        assertEquals(expected,
            details.$[elementId].querySelector('span').innerText);
      };

      // Checks the default route view is shown.
      var checkDefaultViewIsShown = function() {
        assertFalse(details.$['route-information'].hasAttribute('hidden'));
        assertTrue(details.$['custom-controller'].hasAttribute('hidden'));
      };

      // Checks the default route view is shown.
      var checkStartCastButtonIsShown = function() {
        assertFalse(
            details.$['start-casting-to-route-button'].hasAttribute('hidden'));
      };

      // Checks the default route view is not shown.
      var checkStartCastButtonIsNotShown = function() {
        assertTrue(
            details.$['start-casting-to-route-button'].hasAttribute('hidden'));
      };

      // Checks the custom controller is shown.
      var checkCustomControllerIsShown = function() {
        assertTrue(details.$['route-information'].hasAttribute('hidden'));
        assertFalse(details.$['custom-controller'].hasAttribute('hidden'));
      };

      // Checks whether |expected| and the text in the |elementId| element
      // are equal given an id.
      var checkElementTextWithId = function(expected, elementId) {
        assertEquals(expected, details.$[elementId].innerText);
      };

      // Import route_details.html before running suite.
      suiteSetup(function() {
        return PolymerTest.importHtml(
            'chrome://media-router/elements/route_details/' +
            'route_details.html');
      });

      // Initialize a route-details before each test.
      setup(function(done) {
        PolymerTest.clearBody();
        details = document.createElement('route-details');
        document.body.appendChild(details);

        // Initialize routes and sinks.
        fakeRouteOne = new media_router.Route('route id 1', 'sink id 1',
            'Video 1', 1, true, false,
            'chrome-extension://123/custom_view.html');
        fakeRouteTwo = new media_router.Route('route id 2', 'sink id 2',
            'Video 2', 2, false, true);

        // Allow for the route details to be created and attached.
        setTimeout(done);
      });

      // Tests for 'close-route-click' event firing when the
      // 'close-route-button' button is clicked.
      test('close route button click', function(done) {
        details.addEventListener('close-route-click', function() {
          done();
        });
        MockInteractions.tap(details.$['close-route-button']);
      });

      // Tests for 'start-casting-to-route-click' event firing when the
      // 'start-casting-to-route-button' button is clicked.
      test('start casting to route button click', function(done) {
        details.addEventListener(
            'start-casting-to-route-click', function() { done(); });
        MockInteractions.tap(details.$['start-casting-to-route-button']);
      });

      // Tests the initial expected text.
      test('initial text setting', function() {
        // <paper-button> text is styled as upper case.
        checkSpanText(loadTimeData.getString('stopCastingButton')
            .toUpperCase(), 'close-route-button');
        checkSpanText(
            loadTimeData.getString('startCastingButton'),
            'start-casting-to-route-button');
        checkSpanText('', 'route-information');
      });

      // Tests when |route| is null or set.
      test('route is null or set', function() {
        // |route| is null.
        assertEquals(null, details.route);
        checkDefaultViewIsShown();

        // Set |route| to be non-null.
        details.route = fakeRouteOne;
        assertEquals(fakeRouteOne, details.route);
        checkSpanText(loadTimeData.getStringF('castingActivityStatus',
            fakeRouteOne.description), 'route-information');
        checkDefaultViewIsShown();
        checkStartCastButtonIsNotShown();

        // Set |route| to a different route.
        details.route = fakeRouteTwo;
        assertEquals(fakeRouteTwo, details.route);
        checkSpanText(loadTimeData.getStringF('castingActivityStatus',
            fakeRouteTwo.description), 'route-information');
        checkDefaultViewIsShown();
        checkStartCastButtonIsShown();
      });

      // Tests when |route| exists, has a custom controller, and it loads.
      test('route has custom controller and loading succeeds', function(done) {
        var loadInvoked = false;
        details.$['custom-controller'].load = function(url) {
          loadInvoked = true;
          assertEquals('chrome-extension://123/custom_view.html', url);
          return Promise.resolve();
        };

        details.route = fakeRouteOne;
        setTimeout(function() {
          assertTrue(loadInvoked);
          checkCustomControllerIsShown();
          done();
        });
      });

      // Tests when |route| exists, has a custom controller, but fails to load.
      test('route has custom controller but loading fails', function(done) {
        var loadInvoked = false;
        details.$['custom-controller'].load = function(url) {
          loadInvoked = true;
          return Promise.reject();
        };

        details.route = fakeRouteOne;
        setTimeout(function() {
          assertTrue(loadInvoked);
          checkDefaultViewIsShown();
          done();
        });
      });
    });
  }

  return {
    registerTests: registerTests,
  };
});
