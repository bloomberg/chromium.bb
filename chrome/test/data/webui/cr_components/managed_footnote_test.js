// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('managed-footnote', function() {
  suiteSetup(function() {
    loadTimeData.data = {
      isManaged: false,
      managedByOrg: '',
    };
  });

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
  function setupTestElement(isManaged, message) {
    loadTimeData.overrideValues({
      isManaged: isManaged,
      managedByOrg: message,
    });
    const footnote = document.createElement('managed-footnote');
    document.body.appendChild(footnote);
    Polymer.dom.flush();
    return footnote;
  }

  test('Hidden When isManaged Is False', function() {
    const footnote = setupTestElement(false, '');
    assertEquals('none', getComputedStyle(footnote).display);
  });

  test('Reads Attributes From loadTimeData', function() {
    const message = 'the quick brown fox jumps over the lazy dog';
    const footnote = setupTestElement(true, message);
    assertNotEquals('none', getComputedStyle(footnote).display);
    assertTrue(footnote.shadowRoot.textContent.includes(message));
  });

  test('Substitutes URL', function() {
    const message =
        'Your <a target="_blank" href="$1">browser is managed</a> by your ' +
        'organization';
    const targetMessage = 'Your browser is managed by your organization';
    const supportUrl = 'https://support.google.com/chromebook/answer/1331549';

    const footnote = setupTestElement(true, message);
    assertTrue(footnote.shadowRoot.textContent.includes(targetMessage));
    // The <a> element should have the right link.
    assertEquals(supportUrl, footnote.$$('a').href);
  });
});
