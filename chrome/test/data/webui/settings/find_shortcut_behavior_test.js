// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('find-shortcut', () => {
  /** @typedef {{
   *    becomeActiveFindShortcutListener: !Function,
   *    removeSelfAsFindShortcutListener: !Function,
   *  }}
   */
  let Listener;

  /**
   * @type {PromiseResolver<!{modalContextOpen: boolean, self: HTMLElement}>}
   */
  let waits;
  /** @type {number} */
  let nextWait;

  /**
   * @param {!Array} fns
   * @return {!Promise}
   */
  const promiseSeries = fns =>
      fns.reduce((acc, fn) => acc.then(fn), Promise.resolve());

  /**
   * @param {!HTMLElement} testElement
   * @return {!function(!Function)): !Promise}
   */
  const listenScope = testElement => wrapped => promiseSeries([
    () => testElement.becomeActiveFindShortcutListener(),
    wrapped,
    () => testElement.removeSelfAsFindShortcutListener(),
  ]);

  /**
   * @param {!Array<!HTMLElement>} expectedSelves
   * @param {?boolean} expectedModalContextOpen
   * @return {!Promise}
   */
  const checkMultiple = (expectedSelves, expectedModalContextOpen) => {
    waits = expectedSelves.map(() => new PromiseResolver());
    nextWait = 0;
    MockInteractions.pressAndReleaseKeyOn(
        window, 70, cr.isMac ? 'meta' : 'ctrl', 'f');
    return Promise.all(waits.map(wait => wait.promise)).then(argss => {
      argss.forEach((args, index) => {
        assertEquals(expectedSelves[index], args.self);
        assertEquals(!!expectedModalContextOpen, args.modalContextOpen);
      });
    });
  };

  /**
   * @param {!HTMLElement} expectedSelf
   * @param {?boolean} expectedModalContextOpen
   * @return {!Promise}
   */
  const check = (expectedSelf, expectedModalContextOpen) =>
      checkMultiple([expectedSelf], expectedModalContextOpen);

  /**
   * @param {!HTMLElement} expectedSelf
   * @param {?boolean} expectedModalContextOpen
   * @return {!Promise}
   */
  const wrappedCheck = (expectedSelf, expectedModalContextOpen) => listenScope(
      expectedSelf)(() => check(expectedSelf, expectedModalContextOpen));

  suiteSetup(() => {
    document.body.innerHTML = `
        <dom-module id="find-shortcut-element">
          <template></template>
        </dom-module>
      `;

    Polymer({
      is: 'find-shortcut-element',
      behaviors: [settings.FindShortcutBehavior],

      handledResponse: true,

      handleFindShortcut(modalContextOpen) {
        assert(nextWait < waits.length);
        waits[nextWait++].resolve({modalContextOpen, self: this});
        return this.handledResponse;
      },
    });
  });

  setup(() => {
    PolymerTest.clearBody();
  });

  test('no listeners are okay', () => checkMultiple([]));

  test('handled', () => {
    document.body.innerHTML = `<find-shortcut-element></find-shortcut-element>`;
    const testElement = document.body.querySelector('find-shortcut-element');
    return wrappedCheck(testElement);
  });

  test('handled with modal context open', () => {
    document.body.innerHTML = `
        <find-shortcut-element></find-shortcut-element>
        <cr-dialog></cr-dialog>`;
    const testElement = document.body.querySelector('find-shortcut-element');
    const dialog = document.body.querySelector('cr-dialog');
    dialog.showModal();
    return wrappedCheck(testElement, true);
  });

  test('handled with modal context closed', () => {
    document.body.innerHTML = `
        <find-shortcut-element></find-shortcut-element>
        <cr-dialog></cr-dialog>`;
    const testElement = document.body.querySelector('find-shortcut-element');
    const dialog = document.body.querySelector('cr-dialog');
    dialog.showModal();
    assertTrue(dialog.open);
    const whenCloseFired = test_util.eventToPromise('close', dialog);
    dialog.close();
    return whenCloseFired.then(() => wrappedCheck(testElement));
  });

  test('last listener is active', () => {
    document.body.innerHTML = `
        <find-shortcut-element></find-shortcut-element>
        <find-shortcut-element></find-shortcut-element>`;
    const testElements =
        document.body.querySelectorAll('find-shortcut-element');
    return promiseSeries([
      () => listenScope(testElements[0])(() => wrappedCheck(testElements[1])),
      () => listenScope(testElements[1])(() => wrappedCheck(testElements[0])),
    ]);
  });

  test('removing self when not active throws exception', () => {
    document.body.innerHTML = `<find-shortcut-element></find-shortcut-element>`;
    const testElement = document.body.querySelector('find-shortcut-element');
    assertThrows(() => testElement.removeSelfAsFindShortcutListener());
  });

  test('becoming active when already active throws exception', () => {
    document.body.innerHTML = `<find-shortcut-element></find-shortcut-element>`;
    const testElement = document.body.querySelector('find-shortcut-element');
    return listenScope(testElement)(() => {
      assertThrows(() => testElement.becomeActiveFindShortcutListener());
    });
  });
});
