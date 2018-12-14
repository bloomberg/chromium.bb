// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for cr-search-field. */
suite('cr-search-field', function() {
  /** @type {?CrSearchFieldElement} */
  let field = null;

  /** @type {?Array<string>} */
  let searches = null;

  /** @param {string} term */
  function simulateSearch(term) {
    field.$.searchInput.value = term;
    field.onSearchTermInput();
    field.onSearchTermSearch();
  }

  setup(function() {
    PolymerTest.clearBody();
    field = document.createElement('cr-search-field');
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

  // Test that no initial 'search-changed' event is fired during
  // construction and initialization of the cr-toolbar-search-field element.
  test('no initial search-changed event', function() {
    const onSearchChanged = () => {
      assertNotReached('Should not have fired search-changed event');
    };

    // Need to attach listener event before the element is created, to catch
    // the unnecessary initial event.
    document.body.addEventListener('search-changed', onSearchChanged);
    document.body.innerHTML = '<cr-search-field></cr-search-field>';
    // Remove event listener on |body| so that other tests are not affected.
    document.body.removeEventListener('search-changed', onSearchChanged);
  });

  test('clear search button clears and refocuses input', function() {
    field.click();

    simulateSearch('query1');
    Polymer.dom.flush();
    assertTrue(field.hasSearchText);

    field.$$('#clearSearch').click();
    assertEquals('', field.getValue());
    assertEquals(field.$.searchInput, field.root.activeElement);
    assertFalse(field.hasSearchText);
  });

  test('notifies on new searches and setValue', function() {
    field.click();
    simulateSearch('query1');
    Polymer.dom.flush();
    assertEquals('query1', field.getValue());

    field.$$('#clearSearch').click();
    assertEquals('', field.getValue());

    simulateSearch('query2');
    // Ignore identical query.
    simulateSearch('query2');

    field.setValue('foo');
    // Ignore identical query.
    field.setValue('foo');

    field.setValue('');
    assertEquals(['query1', '', 'query2', 'foo', ''].join(), searches.join());
  });

  test('does not notify on setValue with noEvent=true', function() {
    field.click();
    field.setValue('foo', true);
    field.setValue('bar');
    field.setValue('baz', true);
    assertEquals(['bar'].join(), searches.join());
  });
});
