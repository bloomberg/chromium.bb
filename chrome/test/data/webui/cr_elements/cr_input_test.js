// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('cr-input', function() {
  let crInput;
  let input;

  setup(function() {
    PolymerTest.clearBody();
    crInput = document.createElement('cr-input');
    document.body.appendChild(crInput);
    input = crInput.$.input;
    Polymer.dom.flush();
  });

  test('AttributesCorrectlySupported', function() {
    assertFalse(input.autofocus);
    crInput.setAttribute('autofocus', 'autofocus');
    assertTrue(input.autofocus);

    assertFalse(input.disabled);
    crInput.setAttribute('disabled', 'disabled');
    assertTrue(input.disabled);

    assertFalse(input.hasAttribute('tabindex'));
    crInput.tabIndex = '-1';
    assertEquals(-1, input.tabIndex);
    crInput.tabIndex = '0';
    assertEquals(0, input.tabIndex);

    assertEquals('none', getComputedStyle(crInput.$.label).display);
    crInput.label = 'label';
    assertEquals('block', getComputedStyle(crInput.$.label).display);

    assertFalse(input.readOnly);
    crInput.setAttribute('readonly', 'readonly');
    assertTrue(input.readOnly);

    assertEquals('text', input.type);
    crInput.setAttribute('type', 'password');
    assertEquals('password', input.type);

    assertEquals(-1, input.maxLength);
    crInput.setAttribute('maxlength', 5);
    assertEquals(5, input.maxLength);
  });

  test('placeholderCorrectlyBound', function() {
    assertFalse(input.hasAttribute('placeholder'));

    crInput.placeholder = '';
    assertTrue(input.hasAttribute('placeholder'));

    crInput.placeholder = 'hello';
    assertEquals('hello', input.getAttribute('placeholder'));

    crInput.placeholder = null;
    assertFalse(input.hasAttribute('placeholder'));
  });

  test('valueSetCorrectly', function() {
    crInput.value = 'hello';
    assertEquals(crInput.value, input.value);

    // |value| is copied when typing triggers inputEvent.
    input.value = 'hello sir';
    input.dispatchEvent(new InputEvent('input'));
    assertEquals(crInput.value, input.value);
  });

  test('focusState', function() {
    const underline = crInput.$.underline;
    const label = crInput.$.label;
    const originalLabelColor = getComputedStyle(label).color;

    assertEquals('0', getComputedStyle(underline).opacity);
    assertEquals(0, underline.offsetWidth);

    let whenTransitionEnd =
        test_util.eventToPromise('transitionend', underline);

    input.focus();
    assertTrue(originalLabelColor != getComputedStyle(label).color);
    return whenTransitionEnd.then(() => {
      assertEquals('1', getComputedStyle(underline).opacity);
      assertTrue(0 != underline.offsetWidth);
    });
  });

  test('invalidState', function() {
    crInput.errorMessage = 'error';
    const errorLabel = crInput.$.error;
    const underline = crInput.$.underline;
    const label = crInput.$.label;
    const originalLabelColor = getComputedStyle(label).color;
    const originalLineColor = getComputedStyle(underline).borderBottomColor;

    assertEquals(crInput.errorMessage, errorLabel.textContent);
    assertEquals('0', getComputedStyle(underline).opacity);
    assertEquals(0, underline.offsetWidth);
    assertEquals('hidden', getComputedStyle(errorLabel).visibility);

    let whenTransitionEnd =
        test_util.eventToPromise('transitionend', underline);

    crInput.invalid = true;
    Polymer.dom.flush();
    assertTrue(crInput.hasAttribute('invalid'));
    assertEquals('visible', getComputedStyle(errorLabel).visibility);
    assertTrue(originalLabelColor != getComputedStyle(label).color);
    assertTrue(
        originalLineColor != getComputedStyle(underline).borderBottomColor);
    return whenTransitionEnd.then(() => {
      assertEquals('1', getComputedStyle(underline).opacity);
      assertTrue(0 != underline.offsetWidth);
    });
  });

  test('validation', function() {
    crInput.value = 'FOO';
    // Note that even with |autoValidate|, crInput.invalid only updates after
    // |value| is changed.
    crInput.autoValidate = true;
    assertFalse(crInput.hasAttribute('required'));
    assertFalse(crInput.invalid);

    crInput.setAttribute('required', 'required');
    assertTrue(input.required);
    assertFalse(crInput.invalid);

    crInput.value = '';
    assertTrue(crInput.invalid);
    crInput.value = 'BAR';
    assertFalse(crInput.invalid);

    const testPattern = '[a-z]+';
    crInput.setAttribute('pattern', testPattern);
    assertEquals(testPattern, input.pattern);
    crInput.value = 'FOO';
    assertTrue(crInput.invalid);
    crInput.value = 'foo';
    assertFalse(crInput.invalid);

    // Without |autoValidate|, crInput.invalid should not change even if input
    // value is not valid.
    crInput.autoValidate = false;
    crInput.value = 'ALL CAPS';
    assertFalse(crInput.invalid);
    assertFalse(input.checkValidity());
    crInput.value = '';
    assertFalse(crInput.invalid);
    assertFalse(input.checkValidity());
  });
});
