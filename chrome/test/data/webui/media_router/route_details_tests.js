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

      /**
       * First fake sink created before each test.
       * @type {media_router.Sink}
       */
      var fakeSinkOne;

      /**
       * Second fake sink created before each test.
       * @type {media_router.Sink}
       */
      var fakeSinkTwo;

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
            'Video 1', 1, true, 'chrome-extension://123/custom_view.html');
        fakeRouteTwo = new media_router.Route('route id 2', 'sink id 2',
            'Video 2', 2, false);
        fakeSinkOne = new media_router.Sink('sink id 1', 'Living Room',
            media_router.SinkIconType.CAST,
            media_router.SinkStatus.ACTIVE, [0, 1, 2]);
        fakeSinkTwo = new media_router.Sink('sink id 2', 'my device',
            media_router.SinkIconType.CAST,
            media_router.SinkStatus.ACTIVE, [0, 1, 2]);

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

      // Tests the initial expected text.
      test('initial text setting', function() {
        // <paper-button> text is styled as upper case.
        checkSpanText(loadTimeData.getString('stopCastingButton')
            .toUpperCase(), 'close-route-button');
        checkSpanText('', 'route-information');
      });

      // Tests when |route| exists but |sink| is null.
      test('route is set', function() {
        // |route| is null.
        assertEquals(null, details.route);
        checkDefaultViewIsShown();

        // Set |route| to be non-null.
        details.route = fakeRouteOne;
        assertEquals(fakeRouteOne, details.route);
        checkSpanText(loadTimeData.getStringF('castingActivityStatus',
            fakeRouteOne.description), 'route-information');
        assertEquals(null, details.sink);
        checkDefaultViewIsShown();

        // Set |route| to a different route.
        details.route = fakeRouteTwo;
        assertEquals(fakeRouteTwo, details.route);
        checkSpanText(loadTimeData.getStringF('castingActivityStatus',
            fakeRouteTwo.description), 'route-information');
        checkDefaultViewIsShown();
      });

      // Tests when |sink| exists but |route| is null.
      test('sink is set', function() {
        // |sink| is null.
        assertEquals(null, details.sink);
        checkSpanText('', 'route-information');

        // Set |sink| to be non-null. 'route-information' should be updated.
        details.sink = fakeSinkOne;
        assertEquals(fakeSinkOne, details.sink);
        assertEquals(null, details.route);
        checkSpanText('', 'route-information');

        // Set |sink| to be a different sink. 'route-information' text should
        // be updated.
        details.sink = fakeSinkTwo;
        assertEquals(fakeSinkTwo, details.sink);
        checkSpanText('', 'route-information');
      });

      // Tests when |route| and |sink| both exist.
      test('sink and route are set', function() {
        details.route = fakeRouteOne;
        details.sink = fakeSinkOne;
        assertEquals(fakeSinkOne, details.sink);
        assertEquals(fakeRouteOne, details.route);
        checkSpanText(loadTimeData.getStringF('castingActivityStatus',
            fakeRouteOne.description), 'route-information');
      });

      // Tests when |route| and |sink| are both null.
      test('sink and route are null', function() {
        assertEquals(null, details.route);
        assertEquals(null, details.sink);
        checkSpanText('', 'route-information');
      });

      // Tests when |route| and |sink| both exist and |route| has custom
      // controller and it loads.
      test('route has custom controller and loading succeeds', function(done) {
        var loadInvoked = false;
        details.$['custom-controller'].load = function(url) {
          loadInvoked = true;
          assertEquals('chrome-extension://123/custom_view.html', url);
          return Promise.resolve();
        };

        details.route = fakeRouteOne;
        details.sink = fakeSinkOne;
        setTimeout(function() {
          assertTrue(loadInvoked);
          checkCustomControllerIsShown();
          done();
        });
      });

      // Tests when |route| and |sink| both exist and |route| has custom
      // controller but it fails to load.
      test('route has custom controller but loading fails', function(done) {
        var loadInvoked = false;
        details.$['custom-controller'].load = function(url) {
          loadInvoked = true;
          return Promise.reject();
        };

        details.route = fakeRouteOne;
        details.sink = fakeSinkOne;
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
