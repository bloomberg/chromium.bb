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

      var numWindowSeparators = function(card) {
        return Polymer.dom(card.root).
            querySelectorAll(':not([hidden])#window-separator').length;
      };

      var getCards = function() {
        return Polymer.dom(element.root).
            querySelectorAll('history-synced-device-card');
      }

      suiteSetup(function() {
        app = $('history-app');
        // Not rendered until selected.
        assertEquals(null, app.$$('#history-synced-device-manager'));

        app.selectedPage_ = 'history-synced-device-manager';
        return flush().then(function() {
          element = app.$$('#history-synced-device-manager');
          assertTrue(!!element);
        });
      });

      test('single card, single window', function() {
        var sessionList = [
          createSession(
              'Nexus 5',
              [createWindow(['http://www.google.com', 'http://example.com'])]
          )
        ];
        setForeignSessions(sessionList, true);

        return flush().then(function() {
          var card = element.$$('history-synced-device-card');
          assertEquals(2, card.tabs.length);
        });
      });

      test('two cards, multiple windows', function() {
        var sessionList = [
          createSession(
              'Nexus 5',
              [createWindow(['http://www.google.com', 'http://example.com'])]
          ),
          createSession(
              'Nexus 6',
              [
                createWindow(['http://test.com']),
                createWindow(['http://www.gmail.com', 'http://badssl.com'])
              ]
          ),
        ];
        setForeignSessions(sessionList, true);

        return flush().then(function() {
          var cards = getCards();
          assertEquals(2, cards.length);

          // Ensure separators between windows are added appropriately.
          assertEquals(1, numWindowSeparators(cards[0]));
          assertEquals(2, numWindowSeparators(cards[1]));
        });
      });

      test('updating sessions', function() {
        var session1 = createSession(
            'Chromebook',
            [createWindow(['http://www.example.com', 'http://crbug.com'])]);
        session1.timestamp = 1000;

        var session2 =
            createSession('Nexus 5', [createWindow(['http://www.google.com'])]);

        setForeignSessions([session1, session2], true);

        return flush().then(function() {
          var session1updated = createSession('Chromebook', [
            createWindow(['http://www.example.com', 'http://crbug.com/new']),
            createWindow(['http://web.site'])
          ]);
          session1updated.timestamp = 1234;

          setForeignSessions([session1updated, session2], true);

          return flush();
        }).then(function() {
          // There should only be two cards.
          var cards = getCards();
          assertEquals(2, cards.length);

          // There are now 2 windows in the first device.
          assertEquals(2, numWindowSeparators(cards[0]));

          // Check that the actual link changes.
          assertEquals(
              'http://crbug.com/new',
              Polymer.dom(cards[0].root)
                  .querySelectorAll('.website-title')[1].children[0].$.container
                  .textContent.trim());
        });
      });

      test('two cards, multiple windows, search', function() {
        var sessionList = [
          createSession(
              'Nexus 5',
              [createWindow(['http://www.google.com', 'http://example.com'])]
          ),
          createSession(
              'Nexus 6',
              [
                createWindow(['http://www.gmail.com', 'http://badssl.com']),
                createWindow(['http://test.com']),
                createWindow(['http://www.gmail.com', 'http://bagssl.com'])
              ]
          ),
        ];
        setForeignSessions(sessionList, true);

        return flush().then(function() {
          var cards = getCards();
          assertEquals(2, cards.length);

          // Ensure separators between windows are added appropriately.
          assertEquals(1, numWindowSeparators(cards[0]));
          assertEquals(3, numWindowSeparators(cards[1]));
          element.searchedTerm = 'g';

          return flush();
        }).then(function() {
          var cards = getCards();

          assertEquals(1, numWindowSeparators(cards[0]));
          assertEquals(1, cards[0].tabs.length);
          assertEquals('http://www.google.com', cards[0].tabs[0].title);
          assertEquals(2, numWindowSeparators(cards[1]));
          assertEquals(3, cards[1].tabs.length);
          assertEquals('http://www.gmail.com', cards[1].tabs[0].title);
          assertEquals('http://www.gmail.com', cards[1].tabs[1].title);
          assertEquals('http://bagssl.com', cards[1].tabs[2].title);
        });
      });

      test('show sign in promo', function() {
        updateSignInState(false);
        return flush().then(function() {
          assertFalse(element.$['sign-in-guide'].hidden);
          updateSignInState(true);
          return flush();
        }).then(function() {
          assertTrue(element.$['sign-in-guide'].hidden);
        });
      });

      test('no synced tabs message', function() {
        // When user is not logged in, there is no synced tabs.
        element.signInState_ = false;
        element.syncedDevices_ = [];
        return flush().then(function() {
          assertTrue(element.$['no-synced-tabs'].hidden);

          var cards = getCards();
          assertEquals(0, cards.length);

          updateSignInState(true);

          return flush();
        }).then(function() {
          // When user signs in, first show loading message.
          assertFalse(element.$['no-synced-tabs'].hidden);
          var loading = loadTimeData.getString('loading');
          assertNotEquals(
              -1, element.$['no-synced-tabs'].textContent.indexOf(loading));

          var sessionList = [];
          setForeignSessions(sessionList, true);
          return flush();
        }).then(function() {
          cards = getCards();
          assertEquals(0, cards.length);
          // If no synced tabs are fetched, show 'no synced tabs'.
          assertFalse(element.$['no-synced-tabs'].hidden);
          var noSyncedResults = loadTimeData.getString('noSyncedResults');
          assertNotEquals(
              -1,
              element.$['no-synced-tabs'].textContent.indexOf(noSyncedResults));

          var sessionList = [
            createSession(
                'Nexus 5',
                [createWindow(['http://www.google.com', 'http://example.com'])]
            )
          ];
          setForeignSessions(sessionList, true);

          return flush();
        }).then(function() {
          cards = getCards();
          assertEquals(1, cards.length);
          // If there are any synced tabs, hide the 'no synced tabs' message.
          assertTrue(element.$['no-synced-tabs'].hidden);

          updateSignInState(false);
          return flush();
        }).then(function() {
          // When user signs out, don't show the message.
          assertTrue(element.$['no-synced-tabs'].hidden);
        });
      });

      teardown(function() {
        element.syncedDevices = [];
        element.searchedTerm = '';
      });
    });
  }
  return {
    registerTests: registerTests
  };
});
