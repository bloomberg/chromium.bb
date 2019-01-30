// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('destination_list_test', function() {
  /** @enum {string} */
  const TestNames = {
    FilterDestinations: 'FilterDestinations',
    FireDestinationSelected: 'FireDestinationSelected',
  };

  const suiteName = 'DestinationListTest';

  suite(suiteName, function() {
    /** @type {?PrintPreviewDestinationListElement} */
    let list = null;

    /** @override */
    setup(function() {
      // Create destinations
      const destinations = [
        new print_preview.Destination(
            'id1', print_preview.DestinationType.LOCAL,
            print_preview.DestinationOrigin.LOCAL, 'One',
            print_preview.DestinationConnectionStatus.ONLINE,
            {description: 'ABC'}),
        new print_preview.Destination(
            'id2', print_preview.DestinationType.LOCAL,
            print_preview.DestinationOrigin.LOCAL, 'Two',
            print_preview.DestinationConnectionStatus.ONLINE,
            {description: 'XYZ'}),
        new print_preview.Destination(
            'id3', print_preview.DestinationType.GOOGLE,
            print_preview.DestinationOrigin.COOKIES, 'Three',
            print_preview.DestinationConnectionStatus.ONLINE,
            {description: 'ABC', tags: ['__cp__location=123']}),
        new print_preview.Destination(
            'id4', print_preview.DestinationType.GOOGLE,
            print_preview.DestinationOrigin.COOKIES, 'Four',
            print_preview.DestinationConnectionStatus.ONLINE,
            {description: 'XYZ', tags: ['__cp__location=123']}),
        new print_preview.Destination(
            'id5', print_preview.DestinationType.GOOGLE,
            print_preview.DestinationOrigin.COOKIES, 'Five',
            print_preview.DestinationConnectionStatus.ONLINE,
            {description: 'XYZ', tags: ['__cp__location=123']})
      ];

      // Set up list
      document.body.innerHTML = `
          <print-preview-destination-list id="testList" has-action-link=true
              loading-destinations=false list-name="test">
          </print-preview-destination-list>`;
      list = document.body.querySelector('#testList');
      list.searchQuery = null;
      list.destinations = destinations;
      list.loadingDestinations = false;
      Polymer.dom.flush();
    });

    // Tests that the list correctly shows and hides destinations based on the
    // value of the search query.
    test(assert(TestNames.FilterDestinations), function() {
      const items = list.shadowRoot.querySelectorAll(
          'print-preview-destination-list-item');
      const noMatchHint = list.$$('.no-destinations-message');

      // Query is initialized to null. All items are shown and the hint is
      // hidden.
      items.forEach(item => assertFalse(item.hidden));
      assertTrue(noMatchHint.hidden);

      // Searching for "e" should show "One", "Three", and "Five".
      list.searchQuery = /(e)/i;
      Polymer.dom.flush();
      assertEquals(undefined, Array.from(items).find(item => {
        return !item.hidden &&
            (item.destination.displayName == 'Two' ||
             item.destination.displayName == 'Four');
      }));
      assertTrue(noMatchHint.hidden);

      // Searching for "ABC" should show "One" and "Three".
      list.searchQuery = /(ABC)/i;
      Polymer.dom.flush();
      assertEquals(undefined, Array.from(items).find(item => {
        return !item.hidden && item.destination.displayName != 'One' &&
            item.destination.displayName != 'Three';
      }));
      assertTrue(noMatchHint.hidden);

      // Searching for "F" should show "Four" and "Five"
      list.searchQuery = /(F)/i;
      Polymer.dom.flush();
      assertEquals(undefined, Array.from(items).find(item => {
        return !item.hidden && item.destination.displayName != 'Four' &&
            item.destination.displayName != 'Five';
      }));
      assertTrue(noMatchHint.hidden);

      // Searching for UVW should show no destinations and display the "no
      // match" hint.
      list.searchQuery = /(UVW)/i;
      Polymer.dom.flush();
      items.forEach(item => assertTrue(item.hidden));
      assertFalse(noMatchHint.hidden);

      // Searching for 123 should show destinations "Three", "Four", and "Five".
      list.searchQuery = /(123)/i;
      Polymer.dom.flush();
      assertEquals(undefined, Array.from(items).find(item => {
        return !item.hidden &&
            (item.destination.displayName == 'One' ||
             item.destination.displayName == 'Two');
      }));
      assertTrue(noMatchHint.hidden);

      // Clearing the query restores the original state.
      list.searchQuery = /()/i;
      Polymer.dom.flush();
      items.forEach(item => assertFalse(item.hidden));
      assertTrue(noMatchHint.hidden);
    });

    // Tests that the list correctly fires the destination selected event when
    // the destination is clicked or the enter key is pressed.
    test(assert(TestNames.FireDestinationSelected), function() {
      const items = list.shadowRoot.querySelectorAll(
          'print-preview-destination-list-item');
      let whenDestinationSelected = test_util.eventToPromise(
          'destination-selected', list);
      items[0].click();
      return whenDestinationSelected.then(event => {
        assertEquals(items[0], event.detail);
        whenDestinationSelected = test_util.eventToPromise(
            'destination-selected', list);
        MockInteractions.keyEventOn(
            items[1], 'keydown', 13, undefined, 'Enter');
        return whenDestinationSelected;
      }).then(event => {
        assertEquals(items[1], event.detail);
      });
    });
  });

  return {
    suiteName: suiteName,
    TestNames: TestNames,
  };
});
