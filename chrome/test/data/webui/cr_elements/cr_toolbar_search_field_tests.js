// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for cr-toolbar-search-field. */
cr.define('cr_toolbar_search_field', function() {
  function registerTests() {
    suite('cr-toolbar-search-field', function() {
      /** @type {?CrToolbarSearchFieldElement} */
      var field = null;

      /** @type {?Array<string>} */
      var searches = null;

      /** @param {string} term */
      function simulateSearch(term) {
        field.$.searchInput.bindValue = term;
        field.onSearchTermSearch();
      }

      setup(function() {
        PolymerTest.clearBody();
        field = document.createElement('cr-toolbar-search-field');
        searches = [];
        field.addEventListener('search-changed', function(event) {
          searches.push(event.detail);
        });
        document.body.appendChild(field);
      });

      teardown(function() {
        field.remove();
        field = null;
        searches = null;
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

      test('notifies on new searches', function() {
        MockInteractions.tap(field);
        simulateSearch('query1');
        assertEquals('query1', field.getValue());

        MockInteractions.tap(field.$.clearSearch);
        assertFalse(field.showingSearch);
        assertEquals('', field.getValue());

        simulateSearch('query2');
        // Expecting identical query to be ignored.
        simulateSearch('query2');

        assertEquals(['query1', '', 'query2'].join(), searches.join());
      });

      test('notifies on setValue', function() {
        MockInteractions.tap(field);
        field.setValue('foo');
        field.setValue('');
        field.setValue('bar');
        // Expecting identical query to be ignored.
        field.setValue('bar');
        field.setValue('baz');
        assertEquals(['foo', '', 'bar', 'baz'].join(), searches.join());
      });

      test('blur does not close field when a search is active', function() {
        MockInteractions.tap(field);
        simulateSearch('test');
        MockInteractions.blur(field.$.searchInput);

        assertTrue(field.showingSearch);
      });

      test('opens when value is changed', function() {
        // Change search value without explicity opening the field first.
        // Similar to what happens when pasting or dragging into the input
        // field.
        simulateSearch('test');

        assertFalse(field.$.clearSearch.hidden);
        assertTrue(field.showingSearch);

        MockInteractions.tap(field.$.clearSearch);
        assertFalse(field.showingSearch);
      });
    });
  }

  return {
    registerTests: registerTests,
  };
});
