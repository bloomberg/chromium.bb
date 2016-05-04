// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('md_history.history_synced_tabs_test', function() {
  function createSession(name, windows) {
    return {
      collapsed: false,
      deviceType: '',
      name: name,
      modifiedTime: '2 seconds ago',
      tag: name,
      timestamp: 0,
      windows: windows
    };
  }

  function createWindow(tabUrls) {
    var tabs = tabUrls.map(function(tabUrl) {
      return {sessionId: 0, timestamp: 0, title: tabUrl, url: tabUrl};
    });

    return {
      tabs: tabs,
      userVisibleTimestamp: "A while ago"
    };
  }

  function registerTests() {
    suite('synced-tabs', function() {
      var app;
      var element;
      var sidebarElement;

      suiteSetup(function() {
        app = $('history-app');
        element = app.$['history-synced-device-manager'];
        sidebarElement = app.$['history-side-bar'];
      });

      test('sidebar displayed', function(done) {
        setForeignSessions([], true);
        flush(function() {
          assertFalse(sidebarElement.hidden);
          setForeignSessions([], false);
          flush(function() {
            assertTrue(sidebarElement.hidden);
            done();
          });
        });

      });

      test('single card, single window', function(done) {
        var sessionList = [
          createSession(
              'Nexus 5',
              [createWindow(['http://www.google.com', 'http://example.com'])]
          )
        ];
        setForeignSessions(sessionList, true);

        flush(function() {
          var card = element.$$('history-synced-device-card');
          assertEquals(2, card.tabs.length);
          done();
        });
      });

      test('two cards, multiple windows', function(done) {
        var sessionList = [
          createSession(
              'Nexus 5',
              [createWindow(['http://www.google.com', 'http://example.com'])]
          ),
          createSession(
              'Nexus 5',
              [
                createWindow(['http://test.com']),
                createWindow(['http://www.gmail.com', 'http://badssl.com'])
              ]
          ),
        ];
        setForeignSessions(sessionList, true);

        flush(function() {
          var cards = Polymer.dom(element.root)
                          .querySelectorAll('history-synced-device-card');
          assertEquals(2, cards.length);

          // Ensure separators between windows are added appropriately.
          assertEquals(
              1, Polymer.dom(cards[0].root)
                     .querySelectorAll('#window-separator')
                     .length);
          assertEquals(
              2, Polymer.dom(cards[1].root)
                     .querySelectorAll('#window-separator')
                     .length);
          done();
        });
      });

      test('updating sessions', function(done) {
        var session1 = createSession(
            'Chromebook',
            [createWindow(['http://www.example.com', 'http://crbug.com'])]);
        session1.timestamp = 1000;

        var session2 =
            createSession('Nexus 5', [createWindow(['http://www.google.com'])]);

        setForeignSessions([session1, session2], true);

        flush(function() {
          var session1updated = createSession('Chromebook', [
            createWindow(['http://www.example.com', 'http://crbug.com/new']),
            createWindow(['http://web.site'])
          ]);
          session1updated.timestamp = 1234;

          setForeignSessions([session1updated, session2], true);

          flush(function() {
            // There should only be two cards.
            var cards = Polymer.dom(element.root)
                .querySelectorAll('history-synced-device-card');
            assertEquals(2, cards.length);

            // There are now 2 windows in the first device.
            assertEquals(
                2, Polymer.dom(cards[0].root)
                       .querySelectorAll('#window-separator')
                       .length);

            // Check that the actual link changes.
            assertEquals(
                'http://crbug.com/new',
                Polymer.dom(cards[0].root)
                    .querySelectorAll('.website-title')[1]
                    .textContent.trim());

            done();
          });
        });
      });

      teardown(function() {
        element.syncedDevices = [];
      });
    });
  }
  return {
    registerTests: registerTests
  };
});
