// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for route-controls. */
cr.define('route_controls', function() {
  function registerTests() {
    suite('RouteControls', function() {
      /**
       * Route Controls created before each test.
       * @type {RouteControls}
       */
      var controls;

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

      var assertElementText = function(expected, elementId) {
        assertEquals(expected, controls.$$('#' + elementId).innerText);
      };

      var isElementShown = function(elementId) {
        return !controls.$$('#' + elementId).hasAttribute('hidden');
      };

      var assertElementShown = function(elementId) {
        assertTrue(isElementShown(elementId));
      };

      var assertElementHidden = function(elementId) {
        assertFalse(isElementShown(elementId));
      };

      // Creates an instance of RouteStatus with the given parameters. If a
      // parameter is not set, it defaults to an empty string, zero, or false.
      var createRouteStatus = function(params = {}) {
        return new media_router.RouteStatus(
            params.title ? params.title : '',
            params.status ? params.status : '', !!params.canPlayPause,
            !!params.canMute, !!params.canSetVolume, !!params.canSeek,
            params.playState ? params.playState :
                               media_router.PlayState.PLAYING,
            !!params.isPaused, !!params.isMuted,
            params.volume ? params.volume : 0,
            params.duration ? params.duration : 0,
            params.currentTime ? params.currentTime : 0);
      };

      // Import route_controls.html before running suite.
      suiteSetup(function() {
        return PolymerTest.importHtml(
            'chrome://media-router/elements/route_controls/' +
            'route_controls.html');
      });

      // Initialize a route-controls before each test.
      setup(function(done) {
        PolymerTest.clearBody();
        controls = document.createElement('route-controls');
        document.body.appendChild(controls);

        // Initialize routes and sinks.
        fakeRouteOne = new media_router.Route(
            'route id 1', 'sink id 1', 'Video 1', 1, true, false);
        fakeRouteTwo = new media_router.Route(
            'route id 2', 'sink id 2', 'Video 2', 2, false, true);

        // Allow for the route controls to be created and attached.
        setTimeout(done);
      });

      // Tests the initial expected text.
      test('initial text setting', function() {
        // Set |route|.
        controls.onRouteUpdated(fakeRouteOne);
        assertElementText(
            loadTimeData.getStringF(
                'castingActivityStatus', fakeRouteOne.description),
            'route-description');

        // Set |route| to a different route.
        controls.onRouteUpdated(fakeRouteTwo);
        assertElementText(
            loadTimeData.getStringF(
                'castingActivityStatus', fakeRouteTwo.description),
            'route-description');
      });

      // Tests that the route title and status are shown when RouteStatus is
      // updated.
      test('update route text', function() {
        var title = 'test title';
        var status = 'test status';
        controls.routeStatus =
            createRouteStatus({title: title, status: status});

        assertElementText(title, 'route-title');
        assertElementText(status, 'route-description');
      });

      // Tests that media controls are shown and hidden when RouteStatus is
      // updated.
      test('media controls visibility', function() {
        // Create a RouteStatus with no controls.
        controls.routeStatus = createRouteStatus();
        assertElementHidden('route-play-pause-button');
        assertElementHidden('route-time-controls');
        assertElementHidden('route-volume-button');
        assertElementHidden('route-volume-slider');

        controls.routeStatus =
            createRouteStatus({canPlayPause: true, canSeek: true});

        assertElementShown('route-play-pause-button');
        assertElementShown('route-time-controls');
        assertElementHidden('route-volume-button');
        assertElementHidden('route-volume-slider');

        controls.routeStatus =
            createRouteStatus({canMute: true, canSetVolume: true});

        assertElementHidden('route-play-pause-button');
        assertElementHidden('route-time-controls');
        assertElementShown('route-volume-button');
        assertElementShown('route-volume-slider');
      });

      // Tests that the play button sends a command to the browser API.
      test('send play command', function(done) {
        document.addEventListener('mock-play-current-media', function(data) {
          done();
        });

        controls.routeStatus = createRouteStatus(
            {canPlayPause: true, playState: media_router.PlayState.PAUSED});
        MockInteractions.tap(controls.$$('#route-play-pause-button'));
      });

      // Tests that the pause button sends a command to the browser API.
      test('send pause command', function(done) {
        document.addEventListener('mock-pause-current-media', function(data) {
          done();
        });

        controls.routeStatus = createRouteStatus(
            {canPlayPause: true, playState: media_router.PlayState.PLAYING});
        MockInteractions.tap(controls.$$('#route-play-pause-button'));
      });

      // Tests that the mute button sends a command to the browser API.
      test('send mute command', function(done) {
        function waitForMuteEvent(data) {
          // Remove the event listener to avoid interfering with other tests.
          document.removeEventListener(
              'mock-set-current-media-mute', waitForMuteEvent);
          if (data.detail.mute) {
            done();
          } else {
            done('Expected the "Mute" command but received "Unmute".');
          }
        }
        document.addEventListener(
            'mock-set-current-media-mute', waitForMuteEvent);

        controls.routeStatus =
            createRouteStatus({canMute: true, isMuted: false});
        MockInteractions.tap(controls.$$('#route-volume-button'));
      });

      // Tests that the unmute button sends a command to the browser API.
      test('send unmute command', function(done) {
        function waitForUnmuteEvent(data) {
          // Remove the event listener to avoid interfering with other tests.
          document.removeEventListener(
              'mock-set-current-media-mute', waitForUnmuteEvent);
          if (data.detail.mute) {
            done('Expected the "Unmute" command but received "Mute".');
          } else {
            done();
          }
        }
        document.addEventListener(
            'mock-set-current-media-mute', waitForUnmuteEvent);

        controls.routeStatus =
            createRouteStatus({canMute: true, isMuted: true});
        MockInteractions.tap(controls.$$('#route-volume-button'));
      });

      // // Tests that the seek slider sends a command to the browser API.
      test('send seek command', function(done) {
        var currentTime = 500;
        var duration = 1200;
        document.addEventListener('mock-seek-current-media', function(data) {
          if (data.detail.time == currentTime) {
            done();
          } else {
            done(
                'Expected the time to be ' + currentTime + ' but instead got ' +
                data.detail.time);
          }
        });

        controls.routeStatus =
            createRouteStatus({canSeek: true, duration: duration});

        // In actual usage, the change event gets fired when the user interacts
        // with the slider.
        controls.$$('#route-time-slider').value = currentTime;
        controls.$$('#route-time-slider').fire('change');
      });

      // Tests that the volume slider sends a command to the browser API.
      test('send set volume command', function(done) {
        var volume = 0.45;
        document.addEventListener(
            'mock-set-current-media-volume', function(data) {
              if (data.detail.volume == volume) {
                done();
              } else {
                done(
                    'Expected the volume to be ' + volume +
                    ' but instead got ' + data.detail.volume);
              }
            });

        controls.routeStatus = createRouteStatus({canSetVolume: true});

        // In actual usage, the change event gets fired when the user interacts
        // with the slider.
        controls.$$('#route-volume-slider').value = volume;
        controls.$$('#route-volume-slider').fire('change');
      });

      test('increment current time while playing', function(done) {
        var initialTime = 50;
        controls.routeStatus = createRouteStatus({
          canSeek: true,
          playState: media_router.PlayState.PLAYING,
          duration: 100,
          currentTime: initialTime,
        });

        // Check that the current time has been incremented after a second.
        setTimeout(function() {
          controls.routeStatus.playState = media_router.PlayState.PAUSED;
          var pausedTime = controls.routeStatus.currentTime;
          assertTrue(pausedTime > initialTime);

          // Check that the current time stayed the same after a second, now
          // that the media is paused.
          setTimeout(function() {
            assertEquals(pausedTime, controls.routeStatus.currentTime);
            done();
          }, 1000);
        }, 1000);
      });
    });
  }

  return {
    registerTests: registerTests,
  };
});
