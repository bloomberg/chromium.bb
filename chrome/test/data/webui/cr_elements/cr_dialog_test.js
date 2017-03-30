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

  test('dialog body indicates over-scroll when appropriate', function() {
    document.body.innerHTML = `
      <dialog is="cr-dialog" show-scroll-borders>
        <div class="title">title</div>
        <div class="body">body</div>
      </dialog>`;

    var dialog = document.body.querySelector('dialog');
    var innerBody = document.body.querySelector('.body');
    var bodyContainer = dialog.$$('.body-container');
    assertTrue(!!bodyContainer);

    // Height is normally set via CSS, but mixin doesn't work with innerHTML.
    bodyContainer.style.height = '1px';
    innerBody.style.height = '100px';
    dialog.showModal();

    return PolymerTest.flushTasks()
        .then(function() {
          assertTrue(bodyContainer.classList.contains('bottom-scrollable'));
          bodyContainer.scrollTop = 100;
          return PolymerTest.flushTasks();
        })
        .then(function() {
          assertFalse(bodyContainer.classList.contains('bottom-scrollable'));
        });
  });
});
