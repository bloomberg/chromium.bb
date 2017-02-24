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

    assertFalse(document.activeElement.matches('div.title'));
    assertFalse(document.activeElement.matches('button'));

    document.body.querySelector('dialog').showModal();

    expectTrue(document.activeElement.matches('div.title'));
    expectFalse(document.activeElement.matches('button'));
  });

  test('focuses [autofocus] instead of title when present', function() {
    document.body.innerHTML = `
      <dialog is="cr-dialog">
        <div class="title">title</div>
        <div class="body"><button autofocus>button</button></div>
      </dialog>`;

    assertFalse(document.activeElement.matches('button'));
    assertFalse(document.activeElement.matches('div.title'));

    document.body.querySelector('dialog').showModal();

    expectTrue(document.activeElement.matches('button'));
    expectFalse(document.activeElement.matches('div.title'));
  });
});
