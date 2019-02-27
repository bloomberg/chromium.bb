// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('managed-footnote', function() {
  setup(function() {
    PolymerTest.clearBody();
  });

  teardown(function() {
    PolymerTest.clearBody();
  });

  /**
   * Resets loadTimeData to the parameters, inserts a <managed-footnote> element
   * in the DOM, and returns it.
   * @param {boolean} isManaged Whether the footnote should be visible.
   * @param {string} message String to display inside the element, before href
   *     substitution.
   * @return {HTMLElement}
   */
  function setupTestElement(isManaged, message, managementPageUrl) {
    loadTimeData.overrideValues({
      chromeManagementUrl: managementPageUrl,
      isManaged: isManaged,
      managedByOrg: message,
    });
    const footnote = document.createElement('managed-footnote');
    document.body.appendChild(footnote);
    Polymer.dom.flush();
    return footnote;
  }

  test('Hidden When isManaged Is False', function() {
    const footnote = setupTestElement(false, '', '');
    assertEquals('none', getComputedStyle(footnote).display);
  });

  test('Reads Attributes From loadTimeData', function() {
    const message = 'the quick brown fox jumps over the lazy dog';
    const footnote = setupTestElement(true, message, '');
    assertNotEquals('none', getComputedStyle(footnote).display);
    assertTrue(footnote.shadowRoot.textContent.includes(message));
  });

  test('Responds to is-managed-changed events', function() {
    const footnote = setupTestElement(false, '', '');
    assertEquals('none', getComputedStyle(footnote).display);

    cr.webUIListenerCallback('is-managed-changed', [true]);
    assertNotEquals('none', getComputedStyle(footnote).display);
  });
});
