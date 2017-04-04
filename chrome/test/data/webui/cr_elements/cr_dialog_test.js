// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('cr-dialog', function() {
  setup(function() {
    PolymerTest.clearBody();
  });

  test('focuses title on show', function() {
    document.body.innerHTML = `
      <dialog is="cr-dialog">
        <div class="title">title</div>
        <div class="body"><button>button</button></div>
      </dialog>`;

    var dialog = document.body.querySelector('dialog');
    var button = document.body.querySelector('button');

    assertNotEquals(dialog, document.activeElement);
    assertNotEquals(button, document.activeElement);

    dialog.showModal();

    expectEquals(dialog, document.activeElement);
    expectNotEquals(button, document.activeElement);
  });

  test('enter key clicks action button', function() {
    document.body.innerHTML = `
      <dialog is="cr-dialog">
        <div class="title">title</div>
        <div class="body">
          <button class="action-button" disabled>button</button>
          <button id="other-button">other button</button>
        </div>
      </dialog>`;

    var dialog = document.body.querySelector('dialog');
    var actionButton = document.body.querySelector('.action-button');

    dialog.showModal();

    // MockInteractions triggers event listeners synchronously.
    var clickedCounter = 0;
    actionButton.addEventListener('click', function() {
      clickedCounter += 1;
    });

    // Enter key should ignore disabled buttons.
    MockInteractions.keyEventOn(dialog, 'keypress', 13, undefined, 'Enter');
    assertEquals(0, clickedCounter);

    // Enter key should trigger enabled buttons.
    actionButton.disabled = false;
    MockInteractions.keyEventOn(dialog, 'keypress', 13, undefined, 'Enter');
    assertEquals(1, clickedCounter);

    // Enter keys on other buttons should be ignored.
    clickedCounter = 0;
    var otherButton = document.body.querySelector('#other-button');
    assertTrue(!!otherButton);
    MockInteractions.keyEventOn(
        otherButton, 'keypress', 13, undefined, 'Enter');
    assertEquals(0, clickedCounter);

    // Enter key on the action button should only fire the click handler once.
    MockInteractions.tap(actionButton, 'keypress', 13, undefined, 'Enter');
    assertEquals(1, clickedCounter);
  });

  test('focuses [autofocus] instead of title when present', function() {
    document.body.innerHTML = `
      <dialog is="cr-dialog">
        <div class="title">title</div>
        <div class="body"><button autofocus>button</button></div>
      </dialog>`;

    var dialog = document.body.querySelector('dialog');
    var button = document.body.querySelector('button');

    assertNotEquals(dialog, document.activeElement);
    assertNotEquals(button, document.activeElement);

    dialog.showModal();

    expectNotEquals(dialog, document.activeElement);
    expectEquals(button, document.activeElement);
  });

  test('dialog body scrollable border when appropriate', function(done) {
    document.body.innerHTML = `
      <dialog is="cr-dialog" show-scroll-borders>
        <div class="title">title</div>
        <div class="body">body</div>
      </dialog>`;

    var dialog = document.body.querySelector('dialog');
    var bodyContainer = dialog.$$('.body-container');
    assertTrue(!!bodyContainer);

    var observerCount = 0;

    // Needs to setup the observer before attaching, since InteractionObserver
    // calls callback before MutationObserver does.
    var observer = new MutationObserver(function() {
      observerCount++;
      if (observerCount == 1) {  // Triggered when scrolled to bottom.
        assertFalse(bodyContainer.classList.contains('bottom-scrollable'));
        bodyContainer.scrollTop = 0;
      } else if (observerCount == 2) {  // Triggered when scrolled back to top.
        assertTrue(bodyContainer.classList.contains('bottom-scrollable'));
        observer.disconnect();
        done();
      }
    });
    observer.observe(bodyContainer, {attributes: true});

    // Height is normally set via CSS, but mixin doesn't work with innerHTML.
    bodyContainer.style.height = '1px';
    bodyContainer.scrollTop = 100;
    dialog.showModal();  // Attach the dialog for the first time here.
  });
});
