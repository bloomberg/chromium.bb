// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('cr-radio-button', function() {
  let radioButton;

  setup(function() {
    PolymerTest.clearBody();
    document.body.innerHTML = `
      <cr-radio-button>
        label <a>link</a>
      </cr-radio-button>
    `;

    radioButton = document.querySelector('cr-radio-button');
  });

  function assertChecked() {
    assertTrue(radioButton.hasAttribute('checked'));
    assertEquals('true', radioButton.getAttribute('aria-checked'));
    assertTrue(
        getComputedStyle(radioButton.$$('.disc')).backgroundColor !=
        'rgba(0, 0, 0, 0)');
  }

  function assertNotChecked() {
    assertFalse(radioButton.hasAttribute('checked'));
    assertEquals('false', radioButton.getAttribute('aria-checked'));
    assertEquals(
        'rgba(0, 0, 0, 0)',
        getComputedStyle(radioButton.$$('.disc')).backgroundColor);
  }

  function assertDisabled() {
    assertEquals('-1', radioButton.getAttribute('tabindex'));
    assertTrue(radioButton.hasAttribute('disabled'));
    assertEquals('true', radioButton.getAttribute('aria-disabled'));
    assertEquals('none', getComputedStyle(radioButton).pointerEvents);
    assertTrue('1' != getComputedStyle(radioButton).opacity);
  }

  function assertNotDisabled() {
    assertEquals('0', radioButton.getAttribute('tabindex'));
    assertFalse(radioButton.hasAttribute('disabled'));
    assertEquals('false', radioButton.getAttribute('aria-disabled'));
    assertEquals('1', getComputedStyle(radioButton).opacity);
  }

  // Setting selection by mouse/keyboard is paper-radio-group's job, so
  // these tests simply set states programatically and make sure the element
  // is visually correct.
  test('Checked', function() {
    assertNotChecked();
    radioButton.checked = true;
    assertChecked();
    radioButton.checked = false;
    assertNotChecked();
  });

  test('Disabled', function() {
    assertNotDisabled();
    radioButton.disabled = true;
    assertDisabled();
    radioButton.disabled = false;
    assertNotChecked();
  });

  test('Ripple', function() {
    assertFalse(!!radioButton.$$('paper-ripple'));
    radioButton.fire('focus');
    assertTrue(!!radioButton.$$('paper-ripple'));
    assertTrue(radioButton.$$('paper-ripple').holdDown);
    radioButton.fire('pointerup');
    assertFalse(radioButton.$$('paper-ripple').holdDown);
  });

  test('Click on links does not propagate', function(done) {
    document.body.addEventListener('click', () => {
      assertTrue(false);
    });

    const link = document.querySelector('a');
    link.click();
    setTimeout(done);
  });
});
