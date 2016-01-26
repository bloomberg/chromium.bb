// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for media-router-header. */
cr.define('media_router_header', function() {
  function registerTests() {
    suite('MediaRouterHeader', function() {
      /**
       * Media Router Header created before each test.
       * @type {MediaRouterHeader}
       */
      var header;

      /**
       * The list of elements to check for visibility.
       * @const {!Array<string>}
       */
      var hiddenCheckElementIdList = [
        'arrow-drop-icon',
        'back-button',
        'close-button',
        'header-text'
      ];

      // Checks whether the current icon matches the icon used for the view.
      var checkArrowDropIcon = function(view) {
        assertEquals(header.computeArrowDropIcon_(view),
            header.$['arrow-drop-icon'].icon);
      };

      // Checks whether |element| is hidden.
      var checkElementHidden = function(hidden, element) {
        assertEquals(hidden, element.hidden);
      };

      // Checks whether the elements specified in |elementIdList| are visible.
      // Checks whether all other elements are hidden.
      var checkElementsVisibleWithId = function(elementIdList) {
        for (var i = 0; i < elementIdList.length; i++)
          checkElementHidden(false, header.$[elementIdList[i]]);

        for (var j = 0; j < hiddenCheckElementIdList.length; j++) {
          if (elementIdList.indexOf(hiddenCheckElementIdList[j]) == -1)
            checkElementHidden(true, header.$[hiddenCheckElementIdList[j]]);
        }
      };

      // Import media_router_header.html before running suite.
      suiteSetup(function() {
        return PolymerTest.importHtml(
            'chrome://media-router/elements/media_router_header/' +
            'media_router_header.html');
      });

      // Initialize an media-router-header before each test.
      setup(function(done) {
        PolymerTest.clearBody();
        header = document.createElement('media-router-header');
        document.body.appendChild(header);

        // Allow for the media router header to be created and attached.
        setTimeout(done);
      });

      // Tests for 'close-button-click' event firing when the close button
      // is clicked.
      test('close button click', function(done) {
        header.addEventListener('close-button-click', function() {
          done();
        });
        MockInteractions.tap(header.$['close-button']);
      });

      // Tests for 'back-click' event firing when the back button
      // is clicked.
      test('back button click', function(done) {
        header.addEventListener('back-click', function() {
          done();
        });
        MockInteractions.tap(header.$['back-button']);
      });

      // Tests for 'header-or-arrow-click' event firing when the arrow drop
      // button is clicked on the CAST_MODE_LIST view.
      test('arrow drop icon click', function(done) {
        header.view = media_router.MediaRouterView.CAST_MODE_LIST;
        header.addEventListener('header-or-arrow-click', function() {
          done();
        });
        MockInteractions.tap(header.$['arrow-drop-icon']);
      });

      // Tests for 'header-or-arrow-click' event firing when the arrow drop
      // button is clicked on the SINK_LIST view.
      test('arrow drop icon click', function(done) {
        header.view = media_router.MediaRouterView.SINK_LIST;
        header.addEventListener('header-or-arrow-click', function() {
          done();
        });
        MockInteractions.tap(header.$['arrow-drop-icon']);
      });

      // Tests for 'header-or-arrow-click' event firing when the header text is
      // clicked on the CAST_MODE_LIST view.
      test('header text click on cast mode list view', function(done) {
        header.view = media_router.MediaRouterView.CAST_MODE_LIST;
        header.addEventListener('header-or-arrow-click', function() {
          done();
        });
        MockInteractions.tap(header.$['header-text']);
      });

      // Tests for 'header-or-arrow-click' event firing when the header text is
      // clicked on the SINK_LIST view.
      test('header text click on sink list view', function(done) {
        header.view = media_router.MediaRouterView.SINK_LIST;
        header.addEventListener('header-or-arrow-click', function() {
          done();
        });
        MockInteractions.tap(header.$['header-text']);
      });

      // Tests for no event firing when the header text is clicked on certain
      // views.
      test('header text click without event firing', function(done) {
        header.addEventListener('header-or-arrow-click', function() {
          assertNotReached();
        });

        header.view = media_router.MediaRouterView.FILTER;
        MockInteractions.tap(header.$['header-text']);
        header.view = media_router.MediaRouterView.ISSUE;
        MockInteractions.tap(header.$['header-text']);
        header.view = media_router.MediaRouterView.ROUTE_DETAILS;
        MockInteractions.tap(header.$['header-text']);
        done();
      });

      // Tests the |computeArrowDropIcon_| function.
      test('compute arrow drop icon', function() {
        assertEquals('arrow-drop-up',
            header.computeArrowDropIcon_(
                media_router.MediaRouterView.CAST_MODE_LIST));
        assertEquals('arrow-drop-down',
            header.computeArrowDropIcon_(
                media_router.MediaRouterView.FILTER));
        assertEquals('arrow-drop-down',
            header.computeArrowDropIcon_(
                media_router.MediaRouterView.ISSUE));
        assertEquals('arrow-drop-down',
            header.computeArrowDropIcon_(
                media_router.MediaRouterView.ROUTE_DETAILS));
        assertEquals('arrow-drop-down',
            header.computeArrowDropIcon_(
                media_router.MediaRouterView.SINK_LIST));
      });

      // Tests for expected visible UI depending on the current view.
      test('visibility of UI depending on view', function() {
        header.view = media_router.MediaRouterView.CAST_MODE_LIST;
        checkElementsVisibleWithId(['arrow-drop-icon',
                                    'close-button',
                                    'header-text']);

        header.view = media_router.MediaRouterView.FILTER;
        checkElementsVisibleWithId(['close-button',
                                    'header-text']);

        header.view = media_router.MediaRouterView.ISSUE;
        checkElementsVisibleWithId(['close-button',
                                    'header-text']);

        header.view = media_router.MediaRouterView.ROUTE_DETAILS;
        checkElementsVisibleWithId(['back-button',
                                    'close-button',
                                    'header-text']);

        header.view = media_router.MediaRouterView.SINK_LIST;
        checkElementsVisibleWithId(['arrow-drop-icon',
                                    'close-button',
                                    'header-text']);
      });
    });
  }

  return {
    registerTests: registerTests,
  };
});
