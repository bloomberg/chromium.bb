// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for media-router-container. */
cr.define('media_router_container', function() {
  function registerTests() {
    suite('MediaRouterContainer', function() {
      /**
       * Media Router Container created before each test.
       * @type {MediaRouterContainer}
       */
      var container;

      /**
       * The blocking issue to show.
       * @type {?media_router.Issue}
       */
      var fakeBlockingIssue;

      /**
       * The list of CastModes to show.
       * @type {!Array<!media_router.CastMode>}
       */
      var fakeCastModeList = [];

      /**
       * The list of CastModes to show with non-default modes only.
       * @type {!Array<!media_router.CastMode>}
       */
      var fakeCastModeListWithNonDefaultModesOnly = [];

      /**
       * The blocking issue to show.
       * @type {?media_router.Issue}
       */
      var fakeNonBlockingIssue;

      /**
       * The list of current routes.
       * @type {!Array<!media_router.Route>}
       */
      var fakeRouteList = [];

      /**
       * The list of available sinks.
       * @type {!Array<!media_router.Sink>}
       */
      var fakeSinkList = [];

      /**
       * The list of elements to check for visibility.
       * @const {!Array<string>}
       */
      var hiddenCheckElementIdList = [
        'cast-mode-list',
        'container-header',
        'device-missing',
        'issue-banner',
        'route-details',
        'sink-list',
        'sink-list-view',
      ];

      // Checks whether |view| matches the current view of |container|.
      var checkCurrentView = function(view) {
        assertEquals(view, container.currentView_);
      };

      // Checks whether the elements specified in |elementIdList| are visible.
      // Checks whether all other elements are hidden.
      var checkElementsVisibleWithId = function(elementIdList) {
        for (var i = 0; i < elementIdList.length; i++)
          checkElementHidden(false, container.$[elementIdList[i]]);

        for (var j = 0; j < hiddenCheckElementIdList.length; j++) {
          if (elementIdList.indexOf(hiddenCheckElementIdList[j]) == -1)
            checkElementHidden(true, container.$[hiddenCheckElementIdList[j]]);
        }
      };

      // Checks whether |element| is hidden.
      var checkElementHidden = function(hidden, element) {
        assertEquals(hidden, element.hidden);
      };

      // Checks whether |expected| and the text in the |elementId| element
      // are equal.
      var checkElementText = function(expected, element) {
        assertEquals(expected.trim(), element.textContent.trim());
      };

      // Checks whether |expected| and the text in the |elementId| element
      // are equal given an id.
      var checkElementTextWithId = function(expected, elementId) {
        checkElementText(expected, container.$[elementId]);
      };

      // Checks whether |expected| and the |property| in |container| are equal.
      var checkPropertyValue = function(expected, property) {
        assertEquals(expected.trim(), container.property);
      }

      // Import media_router_container.html before running suite.
      suiteSetup(function() {
        return PolymerTest.importHtml(
            'chrome://media-router/elements/media_router_container/' +
            'media_router_container.html');
      });

      // Initialize a media-router-container before each test.
      setup(function(done) {
        PolymerTest.clearBody();
        container = document.createElement('media-router-container');
        document.body.appendChild(container);

        // Initialize local variables.
        fakeCastModeList = [
          new media_router.CastMode(0, 'Description 0', 'google.com'),
          new media_router.CastMode(1, 'Description 1', null),
          new media_router.CastMode(2, 'Description 2', null),
        ];

        fakeCastModeListWithNonDefaultModesOnly = [
          new media_router.CastMode(1, 'Description 1', null),
          new media_router.CastMode(2, 'Description 2', null),
          new media_router.CastMode(3, 'Description 3', null),
        ];

        fakeRouteList = [
          new media_router.Route('id 1', 'sink id 1', 'Title 1', 0, true),
          new media_router.Route('id 2', 'sink id 2', 'Title 2', 1, false),
        ];

        fakeRouteListWithLocalRoutesOnly = [
          new media_router.Route('id 1', 'sink id 1', 'Title 1', 0, true),
          new media_router.Route('id 2', 'sink id 2', 'Title 2', 1, true),
        ];

        fakeSinkList = [
          new media_router.Sink('sink id 1', 'Sink 1',
              media_router.SinkIconType.CAST,
              media_router.SinkStatus.ACTIVE, [1, 2, 3]),
          new media_router.Sink('sink id 2', 'Sink 2',
              media_router.SinkIconType.CAST,
              media_router.SinkStatus.ACTIVE, [1, 2, 3]),
          new media_router.Sink('sink id 3', 'Sink 3',
              media_router.SinkIconType.CAST,
              media_router.SinkStatus.PENDING, [1, 2, 3]),
        ];

        fakeBlockingIssue = new media_router.Issue(
            'issue id 1', 'Issue Title 1', 'Issue Message 1', 0, 1,
            'route id 1', true, 1234);

        fakeNonBlockingIssue = new media_router.Issue(
            'issue id 2', 'Issue Title 2', 'Issue Message 2', 0, 1,
            'route id 2', false, 1234);

        container.initializeCastModes(
            fakeCastModeList, fakeCastModeList[1].type);

        // Allow for the media router container to be created and attached.
        setTimeout(done);
      });

      // Tests for 'create-route' event firing when a sink with no associated
      // route is clicked.
      test('select sink without a route', function(done) {
        container.allSinks = fakeSinkList;

        setTimeout(function() {
          var sinkList =
              container.$['sink-list'].querySelectorAll('paper-item');
          container.addEventListener('create-route', function(data) {
            assertEquals(fakeSinkList[2].id, data.detail.sinkId);
            assertEquals(container.selectedCastModeValue_,
                data.detail.selectedCastModeValue);
            done();
          });
          // Tap on a sink without a route, which should fire a 'create-route'
          // event.
          assertEquals(fakeSinkList.length, sinkList.length);
          MockInteractions.tap(sinkList[2]);
        });
      });

      // Tests that selecting a sink with an associated route will make the
      // |container| switch to ROUTE_DETAILS view.
      test('select sink with a route', function(done) {
        container.allSinks = fakeSinkList;
        container.routeList = fakeRouteList;

        setTimeout(function() {
          var sinkList =
              container.$['sink-list'].querySelectorAll('paper-item');

          // Start from the SINK_LIST view.
          container.showSinkList_();
          checkCurrentView(media_router.MediaRouterView.SINK_LIST);
          MockInteractions.tap(sinkList[0]);
          checkCurrentView(media_router.MediaRouterView.ROUTE_DETAILS);
          done();
        });
      });

      // Tests that |container| returns to SINK_LIST view and arrow drop icon
      // toggles after a cast mode is selected.
      test('select cast mode', function(done) {
        container.castModeList_ = fakeCastModeListWithNonDefaultModesOnly;

        MockInteractions.tap(container.$['container-header'].
            $['arrow-drop-icon']);
        checkCurrentView(media_router.MediaRouterView.CAST_MODE_LIST);

        setTimeout(function() {
          var castModeList =
              container.$['cast-mode-list'].querySelectorAll('paper-item');

          MockInteractions.tap(castModeList[2]);
          checkCurrentView(media_router.MediaRouterView.SINK_LIST);
          done();
        });
      });

      // Tests that clicking on the drop down icon will toggle |container|
      // between SINK_LIST and CAST_MODE_LIST views.
      test('click drop down icon', function() {
        checkCurrentView(media_router.MediaRouterView.SINK_LIST);

        MockInteractions.tap(container.$['container-header'].
            $['arrow-drop-icon']);
        checkCurrentView(media_router.MediaRouterView.CAST_MODE_LIST);

        MockInteractions.tap(container.$['container-header'].
            $['arrow-drop-icon']);
        checkCurrentView(media_router.MediaRouterView.SINK_LIST);
      });

      // Tests the header text. Choosing a cast mode updates the header text.
      test('header text with no default cast modes', function(done) {
        assertEquals(loadTimeData.getString('selectCastModeHeader'),
            container.selectCastModeHeaderText_);
        assertEquals(fakeCastModeList[1].description, container.headerText);

        container.castModeList_ = fakeCastModeListWithNonDefaultModesOnly;
        setTimeout(function() {
          var castModeList =
              container.$['cast-mode-list'].querySelectorAll('paper-item');
          assertEquals(fakeCastModeListWithNonDefaultModesOnly.length,
              castModeList.length);
          for (var i = 0; i < castModeList.length; i++) {
            MockInteractions.tap(castModeList[i]);
            assertEquals(
                fakeCastModeListWithNonDefaultModesOnly[i].description,
                container.headerText);
            checkElementText(
                fakeCastModeListWithNonDefaultModesOnly[i].description,
                castModeList[i]);
          }

          done();
        });
      });

      // Tests the header text when updated with a cast mode list with a mix of
      // default and non-default cast modes.
      test('cast modes with one default mode', function(done) {
        container.castModeList_ = fakeCastModeList;

        setTimeout(function() {
          var castModeList =
              container.$['cast-mode-list'].querySelectorAll('paper-item');

          for (var i = 0; i < fakeCastModeList.length; i++) {
            MockInteractions.tap(castModeList[i]);
            if (fakeCastModeList[i].type ==
                media_router.CastModeType.DEFAULT) {
              assertEquals(fakeCastModeList[i].description,
                  container.headerText);

              checkElementText(fakeCastModeList[i].host, castModeList[i]);
            } else {
              assertEquals(fakeCastModeList[i].description,
                  container.headerText);
              checkElementText(fakeCastModeList[i].description,
                  castModeList[i]);
            }
          }

          done();
        });
      });

      // Tests that text shown for each sink matches their names.
      test('sink list text', function(done) {
        container.allSinks = fakeSinkList;

        setTimeout(function() {
          var sinkList =
              container.$['sink-list'].querySelectorAll('paper-item');
          assertEquals(fakeSinkList.length, sinkList.length);
          for (var i = 0; i < fakeSinkList.length; i++) {
            checkElementText(fakeSinkList[i].name, sinkList[i]);
          }
          done();
        });
      });

      // Tests the text shown for the sink list.
      test('initial sink list route text', function(done) {
        container.allSinks = fakeSinkList;
        container.routeList = fakeRouteList;

        setTimeout(function() {
          var routeList =
              container.$['sink-list'].querySelectorAll('.route');
          assertEquals(fakeSinkList.length, routeList.length);
          checkElementText(fakeRouteList[0].description, routeList[0]);
          checkElementText(fakeRouteList[1].description, routeList[1]);
          checkElementText('', routeList[2]);
          done();
        });
      });

      // Tests the visibility of routes in the sink list.
      test('initial route visibility', function(done) {
        container.allSinks = fakeSinkList;
        container.routeList = fakeRouteList;

        setTimeout(function() {
          var routeList =
              container.$['sink-list'].querySelectorAll('.route');

          checkElementHidden(false, routeList[0]);
          checkElementHidden(false, routeList[1]);
          checkElementHidden(true, routeList[2]);
          done();
        });
      });

      // Tests the expected view when there is only one local active route and
      // media_router_container is created for the first time.
      test('initial view with one local route', function() {
        container.allSinks = fakeSinkList;
        container.routeList = fakeRouteList;

        checkCurrentView(media_router.MediaRouterView.ROUTE_DETAILS);
      });

      // Tests the expected view when there are multiple local active routes
      // and media_router_container is created for the first time.
      test('initial view with multiple local routes', function() {
        container.allSinks = fakeSinkList;
        container.routeList = fakeRouteListWithLocalRoutesOnly;

        checkCurrentView(media_router.MediaRouterView.SINK_LIST);
      });

      // Tests the expected view when there are no local active routes and
      // media_router_container is created for the first time.
      test('initial view with no local route', function() {
        container.allSinks = fakeSinkList;
        container.routeList = [];

        checkCurrentView(media_router.MediaRouterView.SINK_LIST);
      });

      // Tests the expected view when there are no local active routes and
      // media_router_container is created for the first time.
      test('view after route is closed remotely', function() {
        container.allSinks = fakeSinkList;
        container.routeList = fakeRouteList;
        checkCurrentView(media_router.MediaRouterView.ROUTE_DETAILS);

        container.routeList = [];
        checkCurrentView(media_router.MediaRouterView.SINK_LIST);
      });

      // Tests for expected visible UI when the view is CAST_MODE_LIST.
      test('cast mode list state visibility', function() {
        container.showCastModeList_();
        checkElementsVisibleWithId(['cast-mode-list',
                                    'container-header',
                                    'device-missing',
                                    'sink-list']);

        // Set a non-blocking issue. The issue should stay hidden.
        container.issue = fakeNonBlockingIssue;
        checkElementsVisibleWithId(['cast-mode-list',
                                    'container-header',
                                    'device-missing',
                                    'sink-list']);

        // Set a blocking issue. The issue should stay hidden.
        container.issue = fakeBlockingIssue;
        checkElementsVisibleWithId(['cast-mode-list',
                                    'container-header',
                                    'device-missing',
                                    'sink-list']);
      });

      // Tests for expected visible UI when the view is ROUTE_DETAILS.
      test('route details state visibility', function() {
        container.showRouteDetails_();
        checkElementsVisibleWithId(['device-missing',
                                    'route-details',
                                    'sink-list']);

        // Set a non-blocking issue. The issue should be shown.
        container.issue = fakeNonBlockingIssue;
        checkElementsVisibleWithId(['device-missing',
                                    'issue-banner',
                                    'route-details',
                                    'sink-list']);

        // Set a blocking issue. The issue should be shown, and everything
        // else, hidden.
        container.issue = fakeBlockingIssue;
        checkElementsVisibleWithId(['device-missing',
                                    'issue-banner',
                                    'sink-list']);
      });

      // Tests for expected visible UI when the view is SINK_LIST.
      test('sink list state visibility', function() {
        container.showSinkList_();
        checkElementsVisibleWithId(['container-header',
                                    'device-missing',
                                    'sink-list',
                                    'sink-list-view']);

        // Set an non-empty sink list.
        container.allSinks = fakeSinkList;
        checkElementsVisibleWithId(['container-header',
                                    'sink-list',
                                    'sink-list-view']);

        // Set a non-blocking issue. The issue should be shown.
        container.issue = fakeNonBlockingIssue;
        checkElementsVisibleWithId(['container-header',
                                    'issue-banner',
                                    'sink-list',
                                    'sink-list-view']);

        // Set a blocking issue. The issue should be shown, and everything
        // else, hidden.
        container.issue = fakeBlockingIssue;
        checkElementsVisibleWithId(['issue-banner', 'sink-list']);
      });

      // Tests that the sink list does not contain any sinks that are not
      // compatible with the current cast mode and are not associated with a
      // route.
      test('sink list filtering based on initial cast mode', function(done) {
        var newSinks = [
          new media_router.Sink('sink id 10', 'Sink 10',
              media_router.SinkIconType.CAST,
              media_router.SinkStatus.ACTIVE, [2, 3]),
          new media_router.Sink('sink id 20', 'Sink 20',
              media_router.SinkIconType.CAST,
              media_router.SinkStatus.ACTIVE, [1, 2, 3]),
          new media_router.Sink('sink id 30', 'Sink 30',
              media_router.SinkIconType.CAST,
              media_router.SinkStatus.PENDING, [2, 3]),
        ];

        container.allSinks = newSinks;
        container.routeList = [
          new media_router.Route('id 1', 'sink id 30', 'Title 1', 1, false),
        ];

        setTimeout(function() {
          var sinkList =
              container.$['sink-list'].querySelectorAll('paper-item');

          // newSinks[0] got filtered out since it is not compatible with cast
          // mode 1.
          // 'Sink 20' should be on the list because it contains the selected
          // cast mode. (sinkList[0] = newSinks[1])
          // 'Sink 30' should be on the list because it has a route.
          // (sinkList[1] = newSinks[2])
          assertEquals(2, sinkList.length);
          checkElementText(newSinks[1].name, sinkList[0]);

          // |sinkList[1]| contains route title in addition to sink name.
          assertTrue(sinkList[1].textContent.trim().startsWith(
              newSinks[2].name.trim()));

          done();
        });
      });

      // Tests that after a different cast mode is selected, the sink list will
      // change based on the sinks compatibility with the new cast mode.
      test('changing cast mode changes sink list', function(done) {
        container.allSinks = fakeSinkList;

        var castModeList =
              container.$['cast-mode-list'].querySelectorAll('paper-item');
        MockInteractions.tap(castModeList[0]);
        assertEquals(fakeCastModeList[0].description, container.headerText);

        setTimeout(function() {
          var sinkList =
              container.$['sink-list'].querySelectorAll('paper-item');

          // The sink list is empty because none of the sinks in fakeSinkList
          // is compatible with cast mode 0.
          assertEquals(0, sinkList.length);
          MockInteractions.tap(castModeList[2]);
          assertEquals(fakeCastModeList[2].description, container.headerText);

          setTimeout(function() {
            var sinkList =
                container.$['sink-list'].querySelectorAll('paper-item');
            assertEquals(3, sinkList.length);
            done();
          });
         });
      });
    });
  }

  return {
    registerTests: registerTests,
  };
});
