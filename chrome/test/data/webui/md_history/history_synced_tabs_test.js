// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('md_history.history_synced_tabs_test', function() {
  function createSession(name, windows) {
    return {
      collapsed: false,
      deviceType: '',
      name: name,
      modifiedTime: 0,
      tag: '',
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
      var element;
      var sidebarElement;

      suiteSetup(function() {
        element = $('history-synced-device-manager');
        sidebarElement = $('history-side-bar');
      });

      test('sidebar displayed', function() {
        setForeignSessions([], false);
        assertTrue(sidebarElement.hidden);
        setForeignSessions([], true);
        assertFalse(sidebarElement.hidden);
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

      teardown(function() {
        element.syncedDevices = [];
      });
    });
  }
  return {
    registerTests: registerTests
  };
});
