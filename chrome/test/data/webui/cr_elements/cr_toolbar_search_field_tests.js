// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for cr-toolbar-search-field. */
cr.define('cr_toolbar_search_field', function() {
  function registerTests() {
    suite('cr-toolbar-search-field', function() {
      /** @type {?CrToolbarSearchFieldElement} */
      var field;
      /** @type {?MockSearchFieldDelegate} */
      var delegate;

      /** @param {string} term */
      function simulateSearch(term) {
        field.$.searchInput.bindValue = term;
        field.onSearchTermSearch();
      }

      /**
       * @constructor
       * @implements {SearchFieldDelegate}
       */
      function MockSearchFieldDelegate() {
        /** @type {!Array<string>} */
        this.searches = [];
      }

      MockSearchFieldDelegate.prototype = {
        /** @override */
        onSearchTermSearch: function(term) {
          this.searches.push(term);
        }
      };

      suiteSetup(function() {
        return PolymerTest.importHtml(
            'chrome://resources/cr_elements/cr_toolbar/' +
            'cr_toolbar_search_field.html');
      });

      setup(function() {
        PolymerTest.clearBody();
        // Constructing a new delegate resets the list of searches.
        delegate = new MockSearchFieldDelegate();
        field = document.createElement('cr-toolbar-search-field');
        field.setDelegate(delegate);
        document.body.appendChild(field);
      });

      teardown(function() {
        field.remove();
        field = null;
        delegate = null;
      });

      test('opens and closes correctly', function() {
        assertFalse(field.showingSearch);
        MockInteractions.tap(field);
        assertTrue(field.showingSearch);
        assertEquals(field.$.searchInput, field.root.activeElement);

        MockInteractions.blur(field.$.searchInput);
        assertFalse(field.showingSearch);

        MockInteractions.tap(field);
        assertEquals(field.$.searchInput, field.root.activeElement);

        MockInteractions.pressAndReleaseKeyOn(
            field.$.searchInput, 27, '', 'Escape');
        assertFalse(field.showingSearch, 'Pressing escape closes field.');
        assertNotEquals(field.$.searchInput, field.root.activeElement);
      });

      test('passes searches correctly', function() {
        MockInteractions.tap(field);
        simulateSearch('test');
        assertEquals('test', field.getValue());

        MockInteractions.tap(field.$.clearSearch);
        assertFalse(field.showingSearch);
        assertEquals('', field.getValue());

        assertEquals(['test', ''].join(), delegate.searches.join());
      });

      test('blur does not close field when a search is active', function() {
        MockInteractions.tap(field);
        simulateSearch('test');
        MockInteractions.blur(field.$.searchInput);

        assertTrue(field.showingSearch);
      });
    });
  }

  return {
    registerTests: registerTests,
  };
});
