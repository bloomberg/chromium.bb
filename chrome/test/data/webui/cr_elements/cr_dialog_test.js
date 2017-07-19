// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('cr-dialog', function() {
  function pressEnter(element) {
    MockInteractions.keyEventOn(element, 'keypress', 13, undefined, 'Enter');
  }

  setup(function() {
    PolymerTest.clearBody();
  });

  test('focuses title on show', function() {
    document.body.innerHTML = `
      <dialog is="cr-dialog">
        <div slot="title">title</div>
        <div slot="body"><button>button</button></div>
      </dialog>`;

    var dialog = document.body.querySelector('dialog');
    var button = document.body.querySelector('button');

    assertNotEquals(dialog, document.activeElement);
    assertNotEquals(button, document.activeElement);

    dialog.showModal();

    expectEquals(dialog, document.activeElement);
    expectNotEquals(button, document.activeElement);
  });

  test('enter keys should trigger action buttons once', function() {
    document.body.innerHTML = `
      <dialog is="cr-dialog">
        <div slot="title">title</div>
        <div slot="body">
          <button class="action-button">button</button>
          <button id="other-button">other button</button>
        </div>
      </dialog>`;

    var dialog = document.body.querySelector('dialog');
    var actionButton = document.body.querySelector('.action-button');

    dialog.showModal();

    // MockInteractions triggers event listeners synchronously.
    var clickedCounter = 0;
    actionButton.addEventListener('click', function() {
      clickedCounter++;
    });

    // Enter key on the action button should only fire the click handler once.
    MockInteractions.tap(actionButton, 'keypress', 13, undefined, 'Enter');
    assertEquals(1, clickedCounter);

    // Enter keys on other buttons should be ignored.
    clickedCounter = 0;
    var otherButton = document.body.querySelector('#other-button');
    assertTrue(!!otherButton);
    pressEnter(otherButton);
    assertEquals(0, clickedCounter);

    // Enter keys on the close icon in the top-right corner should be ignored.
    pressEnter(dialog.getCloseButton());
    assertEquals(0, clickedCounter);
  });

  test('enter keys find the first non-hidden non-disabled button', function() {
    document.body.innerHTML = `
      <dialog is="cr-dialog">
        <div slot="title">title</div>
        <div slot="body">
          <button id="hidden" class="action-button" hidden>hidden</button>
          <button class="action-button" disabled>disabled</button>
          <button class="action-button" disabled hidden>disabled hidden</button>
          <button id="active" class="action-button">active</button>
        </div>
      </dialog>`;

    var dialog = document.body.querySelector('dialog');
    var hiddenButton = document.body.querySelector('#hidden');
    var actionButton = document.body.querySelector('#active');
    dialog.showModal();

    // MockInteractions triggers event listeners synchronously.
    hiddenButton.addEventListener('click', function() {
      assertNotReached('Hidden button received a click.');
    })
    var clicked = false;
    actionButton.addEventListener('click', function() {
      clicked = true;
    });

    pressEnter(dialog);
    assertTrue(clicked);
  });

  test('enter keys from paper-inputs (only) are processed', function() {
    document.body.innerHTML = `
      <dialog is="cr-dialog">
        <div slot="title">title</div>
        <div slot="body">
          <paper-input></paper-input>
          <foobar></foobar>
          <button class="action-button">active</button>
        </div>
      </dialog>`;

    var dialog = document.body.querySelector('dialog');

    var inputElement = document.body.querySelector('paper-input');
    var otherElement = document.body.querySelector('foobar');
    var actionButton = document.body.querySelector('.action-button');
    assertTrue(!!inputElement);
    assertTrue(!!otherElement);
    assertTrue(!!actionButton);

    // MockInteractions triggers event listeners synchronously.
    var clickedCounter = 0;
    actionButton.addEventListener('click', function() {
      clickedCounter++;
    });

    pressEnter(otherElement);
    assertEquals(0, clickedCounter);

    pressEnter(inputElement);
    assertEquals(1, clickedCounter);
  });

  test('focuses [autofocus] instead of title when present', function() {
    document.body.innerHTML = `
      <dialog is="cr-dialog">
        <div slot="title">title</div>
        <div slot="body"><button autofocus>button</button></div>
      </dialog>`;

    var dialog = document.body.querySelector('dialog');
    var button = document.body.querySelector('button');

    assertNotEquals(dialog, document.activeElement);
    assertNotEquals(button, document.activeElement);

    dialog.showModal();

    expectNotEquals(dialog, document.activeElement);
    expectEquals(button, document.activeElement);
  });

  // Ensuring that intersectionObserver does not fire any callbacks before the
  // dialog has been opened.
  test('body scrollable border not added before modal shown', function(done) {
    document.body.innerHTML = `
      <dialog is="cr-dialog">
        <div slot="title">title</div>
        <div slot="body">body</div>
      </dialog>`;

    var dialog = document.body.querySelector('dialog');
    assertFalse(dialog.open);
    var bodyContainer = dialog.$$('.body-container');
    assertTrue(!!bodyContainer);

    // Waiting for 1ms because IntersectionObserver fires one message loop after
    // dialog.attached.
    setTimeout(function() {
      assertFalse(bodyContainer.classList.contains('top-scrollable'));
      assertFalse(bodyContainer.classList.contains('bottom-scrollable'));
      done();
    }, 1);
  });

  test('dialog body scrollable border when appropriate', function(done) {
    document.body.innerHTML = `
      <dialog is="cr-dialog">
        <div slot="title">title</div>
        <div slot="body">
          <div style="height: 100px">tall content</div>
        </div>
      </dialog>`;

    var dialog = document.body.querySelector('dialog');
    var bodyContainer = dialog.$$('.body-container');
    assertTrue(!!bodyContainer);

    dialog.showModal();  // Attach the dialog for the first time here.

    var observerCount = 0;

    // Needs to setup the observer before attaching, since InteractionObserver
    // calls callback before MutationObserver does.
    var observer = new MutationObserver(function(changes) {
      // Only care about class mutations.
      if (changes[0].attributeName != 'class')
        return;

      observerCount++;
      switch (observerCount) {
        case 1:  // Triggered when scrolled to bottom.
          assertFalse(bodyContainer.classList.contains('bottom-scrollable'));
          assertTrue(bodyContainer.classList.contains('top-scrollable'));
          bodyContainer.scrollTop = 0;
          break;
        case 2:  // Triggered when scrolled back to top.
          assertTrue(bodyContainer.classList.contains('bottom-scrollable'));
          assertFalse(bodyContainer.classList.contains('top-scrollable'));
          bodyContainer.scrollTop = 2;
          break;
        case 3:  // Triggered when finally scrolling to middle.
          assertTrue(bodyContainer.classList.contains('bottom-scrollable'));
          assertTrue(bodyContainer.classList.contains('top-scrollable'));
          observer.disconnect();
          done();
          break;
      }
    });
    observer.observe(bodyContainer, {attributes: true});

    // Height is normally set via CSS, but mixin doesn't work with innerHTML.
    bodyContainer.style.height = '60px';  // Element has "min-height: 60px".
    bodyContainer.scrollTop = 100;
  });

  test('dialog cannot be cancelled when `no-cancel` is set', function() {
    document.body.innerHTML = `
      <dialog is="cr-dialog" no-cancel>
        <div slot="title">title</div>
      </dialog>`;

    var dialog = document.body.querySelector('dialog');
    dialog.showModal();

    assertTrue(dialog.getCloseButton().hidden);

    // Hitting escape fires a 'cancel' event. Cancelling that event prevents the
    // dialog from closing.
    var e = new Event('cancel', {cancelable: true});
    dialog.dispatchEvent(e);
    assertTrue(e.defaultPrevented);

    dialog.noCancel = false;

    var e = new Event('cancel', {cancelable: true});
    dialog.dispatchEvent(e);
    assertFalse(e.defaultPrevented);
  });
});
