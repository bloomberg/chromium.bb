// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('focus-row-behavior', function() {
  /** @type {FocusableIronListItemElement} */ let testElement;

  suiteSetup(function() {
    document.body.innerHTML = `
      <dom-module is="focus-row-element">
        <template>
          <div id="container" focus-row-container>
            <span>fake text</span>
            <button id="control" focus-row-control focus-type='fake-btn'>
              fake button
            </button>
            <button id="controlTwo" focus-row-control focus-type='fake-btn-two'>
              fake button two
            </button>
          </div>
        </template>
      </dom-module>
    `;

    Polymer({
      is: 'focus-row-element',
      behaviors: [FocusRowBehavior],
    });
  });

  setup(function(done) {
    PolymerTest.clearBody();

    testElement = document.createElement('focus-row-element');
    document.body.appendChild(testElement);

    // Block until FocusRowBehavior.attached finishes running async setup.
    Polymer.RenderStatus.afterNextRender(this, function() {
      done();
    });
  });

  test('item passes focus to first focusable child', function() {
    let focused = false;
    testElement.$.control.addEventListener('focus', function() {
      focused = true;
    });
    testElement.fire('focus');
    assertTrue(focused);
  });

  test('will focus a similar item that was last focused', function() {
    const lastButton = document.createElement('button');
    lastButton.setAttribute('focus-type', 'fake-btn-two');
    testElement.lastFocused = lastButton;

    let focused = false;
    testElement.$.controlTwo.addEventListener('focus', function() {
      focused = true;
    });
    testElement.fire('focus');
    assertTrue(focused);
  });

  test('mouse clicks on the row does not focus the controls', function() {
    let focused = false;
    testElement.$.control.addEventListener('focus', function() {
      focused = true;
    });
    MockInteractions.tap(testElement);
    // iron-list is responsible for firing 'focus' after taps, but is not used
    // in the test, so its necessary to manually fire 'focus' after tap.
    testElement.fire('focus');
    assertFalse(focused);
  });
});
