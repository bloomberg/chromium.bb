// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('cr-drawer', function() {
  var drawer;

  setup(function() {
    PolymerTest.clearBody();
    document.body.innerHTML = `
      <dialog is="cr-drawer" id="drawer">
        <div class="drawer-header">Test</div>
        <div class="drawer-content">Test content</div>
      </dialog>
    `;

    drawer = document.getElementById('drawer');
  });

  test('open and close', function(done) {
    drawer.openDrawer();
    assertTrue(drawer.open);

    listenOnce(drawer, 'transitionend', function() {
      listenOnce(drawer, 'close', function() {
        assertFalse(drawer.open);
        done();
      });

      // Clicking the content does not close the drawer.
      MockInteractions.tap(document.querySelector('.drawer-content'));
      assertFalse(drawer.classList.contains('closing'));

      drawer.dispatchEvent(new MouseEvent('click', {
        bubbles: true,
        cancelable: true,
        clientX: 300,  // Must be larger than the drawer width (256px).
        clientY: 300,
      }));

      // Clicking outside the drawer does close it.
      assertTrue(drawer.classList.contains('closing'));
    });
  });
});
