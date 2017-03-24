// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('settings_test', function() {

  suite('SearchSettingsTest', function() {
    var searchManager;

    // Don't import script if already imported (happens in Vulcanized mode).
    suiteSetup(function() {
      if (!window.settings || !settings.getSearchManager) {
        return PolymerTest.loadScript(
            'chrome://md-settings/search_settings.js');
      }
    });

    setup(function() {
      searchManager = settings.getSearchManager();
      PolymerTest.clearBody();
    });

    /**
     * Test that the DOM of a node is modified as expected when a search hit
     * occurs within that node.
     */
    test('normal highlighting', function() {
      var optionText = 'FooSettingsFoo';

      document.body.innerHTML =
          `<settings-section hidden-by-search>
             <div id="mydiv">${optionText}</div>
           </settings-section>`;

      var section = document.querySelector('settings-section');
      var div = document.querySelector('#mydiv');

      assertTrue(section.hiddenBySearch);
      return searchManager.search('settings', section).then(function() {
        assertFalse(section.hiddenBySearch);

        var highlightWrapper = div.querySelector('.search-highlight-wrapper');
        assertTrue(!!highlightWrapper);

        var originalContent = highlightWrapper.querySelector(
            '.search-highlight-original-content');
        assertTrue(!!originalContent);
        assertEquals(optionText, originalContent.textContent);

        var searchHits = highlightWrapper.querySelectorAll(
            '.search-highlight-hit');
        assertEquals(1, searchHits.length);
        assertEquals('Settings', searchHits[0].textContent);

        // Check that original DOM structure is restored when search
        // highlights are cleared.
        return searchManager.search('', section);
      }).then(function() {
        assertEquals(0, div.children.length);
        assertEquals(optionText, div.textContent);
      });
    });

    /**
     * Tests that a search hit within a <select> node causes the parent
     * settings-section to be shown, but the DOM of the <select> is not
     * modified.
     */
    test('<select> highlighting', function() {
      document.body.innerHTML =
          `<settings-section hidden-by-search>
             <select>
               <option>Foo</option>
               <option>Settings</option>
               <option>Baz</option>
             </select>
           </settings-section>`;

      var section = document.querySelector('settings-section');
      var select = section.querySelector('select');

      assertTrue(section.hiddenBySearch);
      return searchManager.search('settings', section).then(function() {
        assertFalse(section.hiddenBySearch);

        var highlightWrapper = select.querySelector(
            '.search-highlight-wrapper');
        assertFalse(!!highlightWrapper);

        // Check that original DOM structure is present even after search
        // highlights are cleared.
        return searchManager.search('', section);
      }).then(function() {
        var options = select.querySelectorAll('option');
        assertEquals(3, options.length);
        assertEquals('Foo', options[0].textContent);
        assertEquals('Settings', options[1].textContent);
        assertEquals('Baz', options[2].textContent);
      });
    });

    test('ignored elements are ignored', function() {
      var text = 'hello';
      document.body.innerHTML =
          `<settings-section hidden-by-search>
             <cr-events>${text}</cr-events>
             <dialog>${text}</dialog>
             <iron-icon>${text}</iron-icon>
             <iron-list>${text}</iron-list>
             <paper-icon-button>${text}</paper-icon-button>
             <paper-ripple>${text}</paper-ripple>
             <paper-slider>${text}</paper-slider>
             <paper-spinner>${text}</paper-spinner>
             <style>${text}</style>
             <template>${text}</template>
           </settings-section>`;

      var section = document.querySelector('settings-section');
      assertTrue(section.hiddenBySearch);

      return searchManager.search(text, section).then(function() {
        assertTrue(section.hiddenBySearch);
      });
    });

    // Test that multiple requests for the same text correctly highlight their
    // corresponding part of the tree without affecting other parts of the tree.
    test('multiple simultaneous requests for the same text', function() {
      document.body.innerHTML =
          `<settings-section hidden-by-search>
             <div><span>Hello there</span></div>
           </settings-section>
           <settings-section hidden-by-search>
             <div><span>Hello over there</span></div>
           </settings-section>
           <settings-section hidden-by-search>
             <div><span>Nothing</span></div>
           </settings-section>`;

      var sections = Array.prototype.slice.call(
          document.querySelectorAll('settings-section'));

      return Promise.all(
        sections.map(function(section) {
          return searchManager.search('there', section);
        }),
      ).then(function(requests) {
        assertTrue(requests[0].didFindMatches());
        assertFalse(sections[0].hiddenBySearch);
        assertTrue(requests[1].didFindMatches());
        assertFalse(sections[1].hiddenBySearch);
        assertFalse(requests[2].didFindMatches());
        assertTrue(sections[2].hiddenBySearch);
      });
    });
  });
});
