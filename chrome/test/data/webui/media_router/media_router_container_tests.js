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
        'cast-mode-header-text',
        'cast-mode-list',
        'container-header',
        'device-missing',
        'issue-banner',
        'route-details',
        'sink-list',
        'sink-list-header-text',
        'sink-list-view',
      ];

      // Checks whether the current icon matches the icon used for the view.
      var checkArrowDropIcon = function(view) {
        assertEquals(container.computeArrowDropIcon_(view),
            container.$['arrow-drop-icon'].icon);
      };

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
          new media_router.CastMode(0, 'Cast Mode 0', 'Description 0',
              'google.com'),
          new media_router.CastMode(1, 'Cast Mode 1', 'Description 1', null),
          new media_router.CastMode(2, 'Cast Mode 2', 'Description 2', null),
        ];

        fakeCastModeListWithNonDefaultModesOnly = [
          new media_router.CastMode(1, 'Cast Mode 1', 'Description 1', null),
          new media_router.CastMode(2, 'Cast Mode 2', 'Description 2', null),
          new media_router.CastMode(3, 'Cast Mode 3', 'Description 3', null),
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
              media_router.SinkStatus.ACTIVE, [1, 2, 3]),
          new media_router.Sink('sink id 2', 'Sink 2',
              media_router.SinkStatus.ACTIVE, [1, 2, 3]),
          new media_router.Sink('sink id 3', 'Sink 3',
              media_router.SinkStatus.PENDING, [1, 2, 3]),
        ];

        fakeBlockingIssue = new media_router.Issue(
            'issue id 1', 'Issue Title 1', 'Issue Message 1', 0, 1,
            'route id 1', true, 1234);

        fakeNonBlockingIssue = new media_router.Issue(
            'issue id 2', 'Issue Title 2', 'Issue Message 2', 0, 1,
            'route id 2', false, 1234);

        // Allow for the media router container to be created and attached.
        setTimeout(done);
      });

      // Tests for 'close-button-click' event firing when the close button
      // is clicked.
      test('close button click', function(done) {
        container.addEventListener('close-button-click', function() {
          done();
        });
        MockInteractions.tap(container.$['close-button']);
      });

      // Tests for 'create-route' event firing when a sink with no associated
      // route is clicked.
      test('select sink without a route', function(done) {
        container.sinkList = fakeSinkList;

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
          MockInteractions.tap(sinkList[2]);
        });
      });

      // Tests that selecting a sink with an associated route will make the
      // |container| switch to ROUTE_DETAILS view.
      test('select sink with a route', function(done) {
        container.sinkList = fakeSinkList;
        container.routeList = fakeRouteList;

        setTimeout(function() {
          var sinkList =
              container.$['sink-list'].querySelectorAll('paper-item');

          // Start from the SINK_LIST view.
          container.showSinkList_();
          checkCurrentView(container.CONTAINER_VIEW_.SINK_LIST);
          MockInteractions.tap(sinkList[0]);
          checkCurrentView(container.CONTAINER_VIEW_.ROUTE_DETAILS);
          done();
        });
      });

      // Tests that |container| returns to SINK_LIST view and arrow drop icon
      // toggles after a cast mode is selected.
      test('select cast mode', function(done) {
        container.castModeList = fakeCastModeListWithNonDefaultModesOnly;

        MockInteractions.tap(container.$['arrow-drop-icon']);
        checkArrowDropIcon(container.CONTAINER_VIEW_.CAST_MODE_LIST);
        checkCurrentView(container.CONTAINER_VIEW_.CAST_MODE_LIST);

        setTimeout(function() {
          var castModeList =
              container.$['cast-mode-list'].querySelectorAll('paper-item');

          MockInteractions.tap(castModeList[2]);
          checkArrowDropIcon(container.CONTAINER_VIEW_.SINK_LIST);
          checkCurrentView(container.CONTAINER_VIEW_.SINK_LIST);
          done();
        });
      });

      // Tests that clicking on the drop down icon will toggle |container|
      // between SINK_LIST and CAST_MODE_LIST views.
      test('click drop down icon', function() {
        checkCurrentView(container.CONTAINER_VIEW_.SINK_LIST);

        MockInteractions.tap(container.$['arrow-drop-icon']);
        checkArrowDropIcon(container.CONTAINER_VIEW_.CAST_MODE_LIST);
        checkCurrentView(container.CONTAINER_VIEW_.CAST_MODE_LIST);

        MockInteractions.tap(container.$['arrow-drop-icon']);
        checkArrowDropIcon(container.CONTAINER_VIEW_.SINK_LIST);
        checkCurrentView(container.CONTAINER_VIEW_.SINK_LIST);
      });

      // Tests the |computeArrowDropIcon_| function.
      test('compute arrow drop icon', function() {
        assertEquals('arrow-drop-up',
            container.computeArrowDropIcon_(
                container.CONTAINER_VIEW_.CAST_MODE_LIST));
        assertEquals('arrow-drop-down',
            container.computeArrowDropIcon_(
                container.CONTAINER_VIEW_.ROUTE_DETAILS));
        assertEquals('arrow-drop-down',
            container.computeArrowDropIcon_(
                container.CONTAINER_VIEW_.SINK_LIST));
      });

      // Tests the header text. Choosing a cast mode updates the header text.
      test('header text with no default cast modes', function(done) {
        checkElementTextWithId(loadTimeData.getString('selectCastModeHeader'),
            'cast-mode-header-text');

        var fakeHeaderText = 'fake header text';
        container.headerText = fakeHeaderText;
        checkElementTextWithId(fakeHeaderText, 'sink-list-header-text');

        // Set the cast mode list to update the header text when one is
        // selected.
        container.castModeList = fakeCastModeListWithNonDefaultModesOnly;

        setTimeout(function() {
          var castModeList =
              container.$['cast-mode-list'].querySelectorAll('paper-item');

          for (var i = 0; i < fakeCastModeListWithNonDefaultModesOnly.length;
              i++) {
            MockInteractions.tap(castModeList[i]);
            checkElementTextWithId(
                fakeCastModeListWithNonDefaultModesOnly[i].description,
                'sink-list-header-text');
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
        container.castModeList = fakeCastModeList;

        setTimeout(function() {
          var castModeList =
              container.$['cast-mode-list'].querySelectorAll('paper-item');

          for (var i = 0; i < fakeCastModeList.length; i++) {
            MockInteractions.tap(castModeList[i]);
            if (fakeCastModeList[i].type == media_router.CastModeType.DEFAULT) {
              checkElementTextWithId(fakeCastModeList[i].description,
                  'sink-list-header-text');
              checkElementText(fakeCastModeList[i].host,
                  castModeList[i]);
            } else {
              checkElementTextWithId(fakeCastModeList[i].description,
                  'sink-list-header-text');
              checkElementText(fakeCastModeList[i].description,
                  castModeList[i]);
            }
          }

          done();
        });
      });

      // Tests that text shown for each sink matches their names.
      test('sink list text', function(done) {
        container.sinkList = fakeSinkList;

        setTimeout(function() {
          var sinkList =
              container.$['sink-list'].querySelectorAll('paper-item');

          for (var i = 0; i < fakeSinkList.length; i++) {
            checkElementText(fakeSinkList[i].name, sinkList[i]);
          }
          done();
        });
      });

      // Tests the text shown for the sink list.
      test('initial sink list route text', function(done) {
        container.sinkList = fakeSinkList;
        container.routeList = fakeRouteList;

        setTimeout(function() {
          var routeList =
              container.$['sink-list'].querySelectorAll('.route');

          checkElementText(fakeRouteList[0].title, routeList[0]);
          checkElementText(fakeRouteList[1].title, routeList[1]);
          checkElementText('', routeList[2]);
          done();
        });
      });

      // Tests the visibility of routes in the sink list.
      test('initial route visibility', function(done) {
        container.sinkList = fakeSinkList;
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
        container.sinkList = fakeSinkList;
        container.routeList = fakeRouteList;

        checkCurrentView(container.CONTAINER_VIEW_.ROUTE_DETAILS);
      });

      // Tests the expected view when there are multiple local active routes
      // and media_router_container is created for the first time.
      test('initial view with multiple local routes', function() {
        container.sinkList = fakeSinkList;
        container.routeList = fakeRouteListWithLocalRoutesOnly;

        checkCurrentView(container.CONTAINER_VIEW_.SINK_LIST);
      });

      // Tests the expected view when there are no local active routes and
      // media_router_container is created for the first time.
      test('initial view with one local route', function() {
        container.sinkList = fakeSinkList;
        container.routeList = [];

        checkCurrentView(container.CONTAINER_VIEW_.SINK_LIST);
      });

      // Tests for expected visible UI when the view is CAST_MODE_LIST.
      test('cast mode list state visibility', function() {
        container.showCastModeList_();
        checkElementsVisibleWithId(['cast-mode-header-text',
                                    'cast-mode-list',
                                    'container-header',
                                    'device-missing',
                                    'sink-list']);

        // Set a non-blocking issue. The issue should stay hidden.
        container.issue = fakeNonBlockingIssue;
        checkElementsVisibleWithId(['cast-mode-header-text',
                                    'cast-mode-list',
                                    'container-header',
                                    'device-missing',
                                    'sink-list']);

        // Set a blocking issue. The issue should stay hidden.
        container.issue = fakeBlockingIssue;
        checkElementsVisibleWithId(['cast-mode-header-text',
                                    'cast-mode-list',
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
                                    'sink-list-header-text',
                                    'sink-list-view']);

        // Set an non-empty sink list.
        container.sinkList = fakeSinkList;
        checkElementsVisibleWithId(['container-header',
                                    'sink-list',
                                    'sink-list-header-text',
                                    'sink-list-view']);

        // Set a non-blocking issue. The issue should be shown.
        container.issue = fakeNonBlockingIssue;
        checkElementsVisibleWithId(['container-header',
                                    'issue-banner',
                                    'sink-list',
                                    'sink-list-header-text',
                                    'sink-list-view']);

        // Set a blocking issue. The issue should be shown, and everything
        // else, hidden.
        container.issue = fakeBlockingIssue;
        checkElementsVisibleWithId(['issue-banner', 'sink-list']);
      });
    });
  }

  return {
    registerTests: registerTests,
  };
});
