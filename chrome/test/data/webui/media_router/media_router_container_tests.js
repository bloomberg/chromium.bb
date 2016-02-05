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
        'first-run-flow',
        'first-run-flow-cloud-pref',
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
      // Checks whether all other elements are not visible.
      var checkElementsVisibleWithId = function(elementIdList) {
        for (var i = 0; i < elementIdList.length; i++)
          checkElementVisibleWithId(true, elementIdList[i]);

        for (var j = 0; j < hiddenCheckElementIdList.length; j++) {
          if (elementIdList.indexOf(hiddenCheckElementIdList[j]) == -1)
            checkElementVisibleWithId(false, hiddenCheckElementIdList[j]);
        }
      };

      // Checks the visibility of an element with |elementId| in |container|.
      // An element is considered visible if it exists and its |hidden| property
      // is |false|.
      var checkElementVisibleWithId = function(visible, elementId) {
        var element = container.$$('#' + elementId);
        var elementVisible = !!element && !element.hidden &&
            element.style.display != 'none';
        assertEquals(visible, elementVisible, elementId);
      };

      // Checks whether |expected| and the text in the |element| are equal.
      var checkElementText = function(expected, element) {
        assertEquals(expected.trim(), element.textContent.trim());
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
          new media_router.CastMode(0x1, 'Description 0', 'google.com'),
          new media_router.CastMode(0x2, 'Description 1', null),
          new media_router.CastMode(0x4, 'Description 2', null),
        ];

        fakeCastModeListWithNonDefaultModesOnly = [
          new media_router.CastMode(0x2, 'Description 1', null),
          new media_router.CastMode(0x4, 'Description 2', null),
          new media_router.CastMode(0x8, 'Description 3', null),
        ];

        fakeRouteList = [
          new media_router.Route('id 1', 'sink id 1',
                                 'Title 1', 0, true, false),
          new media_router.Route('id 2', 'sink id 2',
                                 'Title 2', 1, false, true),
        ];

        fakeRouteListWithLocalRoutesOnly = [
          new media_router.Route('id 1', 'sink id 1',
                                 'Title 1', 0, true, false),
          new media_router.Route('id 2', 'sink id 2',
                                 'Title 2', 1, true, false),
        ];

        var castModeBitset = 0x2 | 0x4 | 0x8;
        fakeSinkList = [
          new media_router.Sink('sink id 1', 'Sink 1', null, null,
              media_router.SinkIconType.CAST,
              media_router.SinkStatus.ACTIVE, castModeBitset),
          new media_router.Sink('sink id 2', 'Sink 2', null, null,
              media_router.SinkIconType.CAST,
              media_router.SinkStatus.ACTIVE, castModeBitset),
          new media_router.Sink('sink id 3', 'Sink 3', null, null,
              media_router.SinkIconType.CAST,
              media_router.SinkStatus.PENDING, castModeBitset),
        ];

        fakeBlockingIssue = new media_router.Issue(
            'issue id 1', 'Issue Title 1', 'Issue Message 1', 0, 1,
            'route id 1', true, 1234);

        fakeNonBlockingIssue = new media_router.Issue(
            'issue id 2', 'Issue Title 2', 'Issue Message 2', 0, 1,
            'route id 2', false, 1234);

        container.castModeList = fakeCastModeList;

        // Allow for the media router container to be created and attached.
        setTimeout(done);
      });

      // Tests for 'acknowledge-first-run-flow' event firing when the
      // 'first-run-button' button is clicked.
      test('first run button click', function(done) {
        container.showFirstRunFlow = true;

        setTimeout(function() {
          container.addEventListener('acknowledge-first-run-flow',
              function(data) {
            assertFalse(data.detail.optedIntoCloudServices);
            done();
          });
          MockInteractions.tap(container.shadowRoot.getElementById(
              'first-run-button'));
        });
      });

      // Tests for 'acknowledge-first-run-flow' event firing when the
      // 'first-run-button' button is clicked and the cloud preference checkbox
      // is also shown.
      test('first run button with cloud pref click', function(done) {
        container.showFirstRunFlow = true;
        container.showFirstRunFlowCloudPref = true;

        setTimeout(function() {
          container.addEventListener('acknowledge-first-run-flow',
              function(data) {
            assertTrue(data.detail.optedIntoCloudServices);
            done();
          });
          MockInteractions.tap(container.shadowRoot.getElementById(
              'first-run-button'));
        });
      });

      // Tests for 'acknowledge-first-run-flow' event firing when the
      // 'first-run-button' button is clicked after the cloud preference
      // checkbox is deselected.
      test('first run button with cloud pref deselected click',
          function(done) {
        container.showFirstRunFlow = true;
        container.showFirstRunFlowCloudPref = true;

        setTimeout(function() {
          container.addEventListener('acknowledge-first-run-flow',
              function(data) {
            assertFalse(data.detail.optedIntoCloudServices);
            done();
          });
          MockInteractions.tap(container.shadowRoot.getElementById(
              'first-run-cloud-checkbox'));
          MockInteractions.tap(container.shadowRoot.getElementById(
              'first-run-button'));
        });
      });

      // Tests for 'create-route' event firing when a sink with no associated
      // route is clicked.
      test('select sink without a route', function(done) {
        container.allSinks = fakeSinkList;

        setTimeout(function() {
          var sinkList =
              container.$['sink-list'].querySelectorAll('paper-item');
          container.addEventListener('create-route', function(data) {
            // Container is initially in auto mode since a cast mode has not
            // been selected.
            assertEquals(media_router.CastModeType.AUTO,
                container.shownCastModeValue_);
            assertEquals(fakeSinkList[2].id, data.detail.sinkId);

            // The preferred compatible cast mode on the sink is used, since
            // the we did not choose a cast mode on the container.
            assertEquals(0x2, data.detail.selectedCastModeValue);
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
        container.castModeList = fakeCastModeListWithNonDefaultModesOnly;

        MockInteractions.tap(container.$['container-header'].
            $['arrow-drop-icon']);
        checkCurrentView(media_router.MediaRouterView.CAST_MODE_LIST);

        setTimeout(function() {
          var castModeList =
              container.$$('#cast-mode-list').querySelectorAll('paper-item');

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

        // The container is currently in auto cast mode, since we have not
        // picked a cast mode explicitly, and the sinks is not compatible
        // with exactly one cast mode.
        assertEquals(media_router.AUTO_CAST_MODE.description,
            container.headerText);
        assertFalse(container.userHasSelectedCastMode_);

        container.castModeList = fakeCastModeListWithNonDefaultModesOnly;

        // Switch to cast mode list view.
        MockInteractions.tap(container.$['container-header'].
            $['arrow-drop-icon']);
        setTimeout(function() {
          var castModeList =
              container.$$('#cast-mode-list').querySelectorAll('paper-item');
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
        container.castModeList = fakeCastModeList;

        // Switch to cast mode list view.
        MockInteractions.tap(container.$['container-header'].
            $['arrow-drop-icon']);
        setTimeout(function() {
          var castModeList =
              container.$$('#cast-mode-list').querySelectorAll('paper-item');

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

      // Tests that text shown for sink with domain matches the name and domain.
      test('sink with domain text', function(done) {
        // Sink 1 - sink, no domain -> text = name
        // Sink 2 - sink, domain -> text = sink + domain
        container.allSinks = [
            new media_router.Sink('sink id 1', 'Sink 1', null, null,
                media_router.SinkIconType.HANGOUT,
                media_router.SinkStatus.ACTIVE, [1, 2, 3]),
            new media_router.Sink('sink id 2', 'Sink 2',
                null, 'example.com',
                media_router.SinkIconType.HANGOUT,
                media_router.SinkStatus.ACTIVE, [1, 2, 3]),
        ];

        setTimeout(function() {
          var sinkList =
              container.$['sink-list'].querySelectorAll('paper-item');
          assertEquals(2, sinkList.length);

          // |sinkList[0]| has sink name only.
          checkElementText(container.allSinks[0].name, sinkList[0]);
          // |sinkList[1]| contains sink name and domain.
          assertTrue(sinkList[1].textContent.trim().startsWith(
              container.allSinks[1].name.trim()));
          assertTrue(sinkList[1].textContent.trim().indexOf(
              container.allSinks[1].domain.trim()) != -1);
          done();
        });
      });

      // Tests the text shown for the sink list.
      test('initial sink list route text', function(done) {
        // Sink 1 - no sink description, no route -> no subtext
        // Sink 2 - sink description, no route -> subtext = sink description
        // Sink 3 - no sink description, route -> subtext = route description
        // Sink 4 - sink description, route -> subtext = route description
        container.allSinks = [
            new media_router.Sink('sink id 1', 'Sink 1', null, null,
                media_router.SinkIconType.CAST,
                media_router.SinkStatus.ACTIVE, [1, 2, 3]),
            new media_router.Sink('sink id 2', 'Sink 2',
                'Sink 2 description', null,
                media_router.SinkIconType.CAST,
                media_router.SinkStatus.ACTIVE, [1, 2, 3]),
            new media_router.Sink('sink id 3', 'Sink 3', null, null,
                media_router.SinkIconType.CAST,
                media_router.SinkStatus.PENDING, [1, 2, 3]),
            new media_router.Sink('sink id 4', 'Sink 4',
                'Sink 4 description', null,
                media_router.SinkIconType.CAST,
                media_router.SinkStatus.PENDING, [1, 2, 3])
        ];

        container.routeList = [
            new media_router.Route('id 3', 'sink id 3', 'Title 3', 0, true),
            new media_router.Route('id 4', 'sink id 4', 'Title 4', 1, false),
        ];

        setTimeout(function() {
          var sinkSubtextList =
              container.$['sink-list'].querySelectorAll('.sink-subtext');

          // There will only be 3 sink subtext entries, because Sink 1 does not
          // have any subtext.
          assertEquals(3, sinkSubtextList.length);

          checkElementText(container.allSinks[1].description,
              sinkSubtextList[0]);

          // Route description overrides sink description for subtext.
          checkElementText(container.routeList[0].description,
              sinkSubtextList[1]);

          checkElementText(container.routeList[1].description,
              sinkSubtextList[2]);
          done();
        });
      });

      // Tests the expected view when there is only one local active route and
      // media_router_container is created for the first time.
      test('initial view with one local route', function() {
        container.allSinks = fakeSinkList;
        container.routeList = fakeRouteList;
        container.maybeShowRouteDetailsOnOpen();

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
        container.maybeShowRouteDetailsOnOpen();
        checkCurrentView(media_router.MediaRouterView.ROUTE_DETAILS);

        container.routeList = [];
        checkCurrentView(media_router.MediaRouterView.SINK_LIST);
      });

      // Tests for expected visible UI when the view is CAST_MODE_LIST.
      test('cast mode list state visibility', function(done) {
        container.showCastModeList_();
        setTimeout(function() {
          checkElementsVisibleWithId(['cast-mode-list',
                                      'container-header',
                                      'device-missing']);

          // Sc.et a non-blocking issue. The issue should stay hidden.
          container.issue = fakeNonBlockingIssue;
          setTimeout(function() {
            checkElementsVisibleWithId(['cast-mode-list',
                                        'container-header',
                                        'device-missing']);

            // Set a blocking issue. The issue should stay hidden.
            container.issue = fakeBlockingIssue;
            setTimeout(function() {
              checkElementsVisibleWithId(['container-header',
                                          'device-missing',
                                          'issue-banner']);
              done();
            });
          });
        });
      });

      // Tests for the expected visible UI when interacting with the first run
      // flow.
      test('first run button visibility', function(done) {
        container.showFirstRunFlow = true;

        setTimeout(function() {
          checkElementVisibleWithId(true, 'first-run-flow');
          MockInteractions.tap(container.shadowRoot.getElementById(
              'first-run-button'));

          setTimeout(function() {
            checkElementVisibleWithId(false, 'first-run-flow');
            done();
          });
        });
      });

      // Tests for the expected visible UI when interacting with the first run
      // flow with cloud services preference.
      test('first run button visibility', function(done) {
        container.showFirstRunFlow = true;
        container.showFirstRunFlowCloudPref = true;

        setTimeout(function() {
          checkElementsVisibleWithId(['container-header',
                                      'device-missing',
                                      'first-run-flow',
                                      'first-run-flow-cloud-pref',
                                      'sink-list-view']);
          MockInteractions.tap(container.shadowRoot.getElementById(
              'first-run-button'));

          setTimeout(function() {
            checkElementsVisibleWithId(['container-header',
                                        'device-missing',
                                        'sink-list-view']);
            done();
          });
        });
      });

      // Tests for expected visible UI when the view is ROUTE_DETAILS.
      test('route details visibility', function(done) {
        container.showRouteDetails_();
        setTimeout(function() {
          checkElementsVisibleWithId(['container-header',
                                      'device-missing',
                                      'route-details']);
          done();
        });
      });

      test('updated route in route details', function(done) {
        container.allSinks = fakeSinkList;
        var description = 'Title';
        var route = new media_router.Route(
            'id 1', 'sink id 1', description, 0, true, false);
        container.routeList = [route];
        container.showRouteDetails_(route);
        setTimeout(function() {
          // Note that sink-list-view is hidden.
          checkElementsVisibleWithId(
              ['container-header', 'route-details', 'sink-list']);
          assertTrue(!!container.currentRoute_);
          assertEquals(description, container.currentRoute_.description);

          var newDescription = 'Foo';
          route.description = newDescription;
          container.routeList = [route];
          setTimeout(function() {
            // Note that sink-list-view is hidden.
            checkElementsVisibleWithId(
                ['container-header', 'route-details', 'sink-list']);
            assertTrue(!!container.currentRoute_);
            assertEquals(newDescription, container.currentRoute_.description);
            done();
          });
        });
      });

      // Tests for expected visible UI when the view is ROUTE_DETAILS, and there
      // is a non-blocking issue.
      test('route details visibility non blocking issue', function(done) {
        container.showRouteDetails_();

        // Set a non-blocking issue. The issue should be shown.
        container.issue = fakeNonBlockingIssue;
        setTimeout(function() {
          checkElementsVisibleWithId(['container-header',
                                      'device-missing',
                                      'route-details']);
          done();
        });
      });

      // Tests for expected visible UI when the view is ROUTE_DETAILS, and there
      // is a blocking issue.
      test('route details visibility with blocking issue', function(done) {
        container.showRouteDetails_();

        // Set a blocking issue. The issue should be shown, and everything
        // else, hidden.
        container.issue = fakeBlockingIssue;
        setTimeout(function() {
          checkElementsVisibleWithId(['container-header',
                                      'device-missing',
                                      'issue-banner']);
          done();
         });
      });

      // Tests for expected visible UI when the view is SINK_LIST.
      test('sink list state visibility', function() {
        container.showSinkList_();
        checkElementsVisibleWithId(['container-header',
                                    'device-missing',
                                    'sink-list-view']);

        // Set an non-empty sink list.
        container.allSinks = fakeSinkList;
        checkElementsVisibleWithId(['container-header',
                                    'sink-list',
                                    'sink-list-view']);
      });

      // Tests for expected visible UI when the view is SINK_LIST, and there is
      // a non blocking issue.
      test('sink list visibility non blocking issue', function(done) {
        container.showSinkList_();

        // Set an non-empty sink list.
        container.allSinks = fakeSinkList;

        // Set a non-blocking issue. The issue should be shown.
        container.issue = fakeNonBlockingIssue;
        setTimeout(function() {
          checkElementsVisibleWithId(['container-header',
                                      'issue-banner',
                                      'sink-list',
                                      'sink-list-view']);
          done();
        });
      });

      // Tests for expected visible UI when the view is SINK_LIST, and there is
      // a blocking issue.
      test('sink list visibility blocking issue', function(done) {
        container.showSinkList_();

        // Set an non-empty sink list.
        container.allSinks = fakeSinkList;

        // Set a blocking issue. The issue should be shown, and everything
        // else, hidden.
        container.issue = fakeBlockingIssue;
        setTimeout(function() {
          checkElementsVisibleWithId(['container-header',
                                      'issue-banner',
                                      'sink-list']);
          done();
        });
      });

      // Tests all sinks are always shown in auto mode, and that the mode will
      // switch if the sinks support only 1 case mode.
      test('sink list in auto mode', function(done) {
        container.allSinks = fakeSinkList;
        setTimeout(function() {
          // Container is initially in auto mode since a cast mode has not been
          // selected.
          assertEquals(media_router.AUTO_CAST_MODE.description,
              container.headerText);
          assertEquals(media_router.CastModeType.AUTO,
              container.shownCastModeValue_);
          assertFalse(container.userHasSelectedCastMode_);
          var sinkList =
              container.$['sink-list'].querySelectorAll('paper-item');

          // All sinks are shown in auto mode.
          assertEquals(3, sinkList.length);

          // When sink list changes to only 1 compatible cast mode, the mode is
          // switched, and all sinks are shown.
          container.allSinks = [
            new media_router.Sink('sink id 10', 'Sink 10', null, null,
                media_router.SinkIconType.CAST,
                media_router.SinkStatus.ACTIVE, 0x4),
            new media_router.Sink('sink id 20', 'Sink 20', null, null,
                media_router.SinkIconType.CAST,
                media_router.SinkStatus.ACTIVE, 0x4),
            new media_router.Sink('sink id 30', 'Sink 30', null, null,
                media_router.SinkIconType.CAST,
                media_router.SinkStatus.PENDING, 0x4),
          ];

          setTimeout(function() {
            assertEquals(fakeCastModeList[2].description, container.headerText);
            assertEquals(fakeCastModeList[2].type,
                container.shownCastModeValue_);
            assertFalse(container.userHasSelectedCastMode_);

            var sinkList =
                container.$['sink-list'].querySelectorAll('paper-item');
            assertEquals(3, sinkList.length);

            // When compatible cast modes size is no longer exactly 1, switch
            // back to auto mode, and all sinks are shown.
            container.allSinks = fakeSinkList;
            setTimeout(function() {
              assertEquals(media_router.AUTO_CAST_MODE.description,
                  container.headerText);
              assertEquals(media_router.CastModeType.AUTO,
                  container.shownCastModeValue_);
              assertFalse(container.userHasSelectedCastMode_);
              var sinkList =
                  container.$['sink-list'].querySelectorAll('paper-item');

              // All sinks are shown in auto mode.
              assertEquals(3, sinkList.length);

              done();
            });
          });
        });
      });

      // Tests that the sink list does not contain any sinks that are not
      // compatible with the selected cast mode and are not associated with a
      // route.
      test('sink list in user selected cast mode', function(done) {
        var newSinks = [
          new media_router.Sink('sink id 10', 'Sink 10', null, null,
              media_router.SinkIconType.CAST,
              media_router.SinkStatus.ACTIVE, 0x4 | 0x8),
          new media_router.Sink('sink id 20', 'Sink 20', null, null,
              media_router.SinkIconType.CAST,
              media_router.SinkStatus.ACTIVE, 0x2 | 0x4 | 0x8),
          new media_router.Sink('sink id 30', 'Sink 30', null, null,
              media_router.SinkIconType.CAST,
              media_router.SinkStatus.PENDING, 0x4 | 0x8),
        ];

        container.allSinks = newSinks;
        container.routeList = [
          new media_router.Route('id 1', 'sink id 30',
                                 'Title 1', 1, false, false),
        ];

        setTimeout(function() {
          var sinkList =
              container.$['sink-list'].querySelectorAll('paper-item');

          // Since we haven't selected a cast mode, we don't filter sinks.
          assertEquals(3, sinkList.length);

          MockInteractions.tap(container.$['container-header'].
              $['arrow-drop-icon']);
          setTimeout(function() {
            // Cast mode 1 is selected, and the sink list is filtered.
            var castModeList =
                container.$$('#cast-mode-list').querySelectorAll('paper-item');
            MockInteractions.tap(castModeList[1]);
            assertEquals(fakeCastModeList[1].description, container.headerText);
            assertEquals(fakeCastModeList[1].type,
                container.shownCastModeValue_);

            setTimeout(function() {
              var sinkList =
                  container.$['sink-list'].querySelectorAll('paper-item');

              // newSinks[0] got filtered out since it is not compatible with
              // cast mode 1.
              // 'Sink 20' should be on the list because it contains the
              // selected cast mode. (sinkList[0] = newSinks[1])
              // 'Sink 30' should be on the list because it has a route.
              // (sinkList[1] = newSinks[2])
              assertEquals(2, sinkList.length);
              checkElementText(newSinks[1].name, sinkList[0]);

              // |sinkList[1]| contains route title in addition to sink name.
              assertTrue(sinkList[1].textContent.trim().startsWith(
                  newSinks[2].name.trim()));

              // Cast mode is not switched back even if there are no sinks
              // compatible with selected cast mode, because we explicitly
              // selected that cast mode.
              container.allSinks = [];
              setTimeout(function() {
                assertEquals(fakeCastModeList[1].description,
                    container.headerText);
                assertEquals(fakeCastModeList[1].type,
                    container.shownCastModeValue_);
                var sinkList =
                    container.$['sink-list'].querySelectorAll('paper-item');
                assertEquals(0, sinkList.length);
                done();
              });
            });
          });
        });
      });

      // Container remains in auto mode even if the cast mode list changed.
      test('cast mode list updated in auto mode', function(done) {
        assertEquals(media_router.AUTO_CAST_MODE.description,
            container.headerText);
        assertEquals(media_router.CastModeType.AUTO,
            container.shownCastModeValue_);
        assertFalse(container.userHasSelectedCastMode_);

        container.castModeList = fakeCastModeList.slice(1);
        setTimeout(function() {
          assertEquals(media_router.AUTO_CAST_MODE.description,
              container.headerText);
          assertEquals(media_router.CastModeType.AUTO,
              container.shownCastModeValue_);
          assertFalse(container.userHasSelectedCastMode_);
          done();
        });
      });

      // If the container is not in auto mode, and the mode it is currently in
      // no longer exists in the list of cast modes, then switch back to auto
      // mode.
      test('cast mode list updated in selected cast mode', function(done) {
        assertEquals(media_router.AUTO_CAST_MODE.description,
            container.headerText);
        assertEquals(media_router.CastModeType.AUTO,
            container.shownCastModeValue_);
        assertFalse(container.userHasSelectedCastMode_);

        MockInteractions.tap(container.$['container-header'].
            $['arrow-drop-icon']);
        setTimeout(function() {
          var castModeList =
                container.$$('#cast-mode-list').querySelectorAll('paper-item');
          MockInteractions.tap(castModeList[0]);
          setTimeout(function() {
            assertEquals(fakeCastModeList[0].description, container.headerText);
            assertEquals(fakeCastModeList[0].type,
                container.shownCastModeValue_);
            assertTrue(container.userHasSelectedCastMode_);
            container.castModeList = fakeCastModeList.slice(1);
            setTimeout(function() {
              assertEquals(media_router.AUTO_CAST_MODE.description,
                  container.headerText);
              assertEquals(media_router.CastModeType.AUTO,
                  container.shownCastModeValue_);
              assertFalse(container.userHasSelectedCastMode_);
              done();
            });
          });
        });
      });

      test('creating route with selected cast mode', function(done) {
        container.allSinks = fakeSinkList;
        MockInteractions.tap(container.$['container-header'].
            $['arrow-drop-icon']);
        setTimeout(function() {
          // Select cast mode 2.
          var castModeList =
              container.$$('#cast-mode-list').querySelectorAll('paper-item');
          MockInteractions.tap(castModeList[1]);
          assertEquals(fakeCastModeList[1].description, container.headerText);
          setTimeout(function() {
            var sinkList =
                container.$['sink-list'].querySelectorAll('paper-item');
            container.addEventListener('create-route', function(data) {
              assertEquals(fakeSinkList[2].id, data.detail.sinkId);
              // Cast mode 2 is used, since we selected it explicitly.
              assertEquals(fakeCastModeList[1].type,
                           data.detail.selectedCastModeValue);
              done();
            });
            // All sinks are compatible with cast mode 2.
            assertEquals(fakeSinkList.length, sinkList.length);
            // Tap on a sink without a route, which should fire a 'create-route'
            // event.
            MockInteractions.tap(sinkList[2]);
          });
        });
      });

      // Tests that after a different cast mode is selected, the sink list will
      // change based on the sinks compatibility with the new cast mode.
      test('changing cast mode changes sink list', function(done) {
        container.allSinks = fakeSinkList;

        MockInteractions.tap(container.$['container-header'].
            $['arrow-drop-icon']);
        setTimeout(function() {
          var castModeList =
                container.$$('#cast-mode-list').querySelectorAll('paper-item');
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
    });
  }

  return {
    registerTests: registerTests,
  };
});
