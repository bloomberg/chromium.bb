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
});
