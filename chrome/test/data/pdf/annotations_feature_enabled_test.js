// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function animationFrame() {
  return new Promise(resolve => requestAnimationFrame(resolve));
}

function contentElement() {
  return document.elementFromPoint(innerWidth / 2, innerHeight / 2);
}

async function testAsync(f) {
  try {
    await f();
    chrome.test.succeed();
  } catch (e) {
    chrome.test.fail(e);
  }
}

chrome.test.runTests([
  function testAnnotationsEnabled() {
    const toolbar = document.body.querySelector('#toolbar');
    chrome.test.assertTrue(toolbar.pdfAnnotationsEnabled);
    chrome.test.assertTrue(
        toolbar.shadowRoot.querySelector('#annotate') != null);
    chrome.test.succeed();
  },
  function testEnterAndExitAnnotationMode() {
    testAsync(async () => {
      chrome.test.assertEq('EMBED', contentElement().tagName);

      // Enter annotation mode.
      $('toolbar').toggleAnnotation();
      await viewer.loaded;
      chrome.test.assertEq(
          'VIEWER-INK-HOST', contentElement().tagName);

      // Exit annotation mode.
      $('toolbar').toggleAnnotation();
      await viewer.loaded;
      chrome.test.assertEq('EMBED', contentElement().tagName);
    });
  },
  function testViewportToCameraConversion() {
    testAsync(async () => {
      // Enter annotation mode.
      $('toolbar').toggleAnnotation();
      await viewer.loaded;

      const inkHost = contentElement();
      const cameras = [];
      inkHost.ink_.setCamera = camera => cameras.push(camera);

      viewer.viewport_.setZoom(1);
      viewer.viewport_.setZoom(2);
      chrome.test.assertEq(2, cameras.length);

      window.scrollTo(100, 100);
      await animationFrame();

      chrome.test.assertEq(3, cameras.length);

      const expectations = [
        {top: 44.25, left: -106.5, right: 718.5, bottom: -442.5},
        {top: 23.25, left: -3.75, right: 408.75, bottom: -220.125},
        {top: -14.25, left: 33.75, right: 446.25, bottom: -257.625},
      ];

      for (const expectation of expectations) {
        const actual = cameras.shift();
        chrome.test.assertEq(expectation.top, actual.top);
        chrome.test.assertEq(expectation.left, actual.left);
        chrome.test.assertEq(expectation.bottom, actual.bottom);
        chrome.test.assertEq(expectation.right, actual.right);
      }
    });
  },
  function testPenOptions() {
    testAsync(async () => {
      // Still in annotation mode after previous test.
      const inkHost = contentElement();
      let tool = null;
      inkHost.ink_.setAnnotationTool = value => tool = value;

      // Pen defaults.
      document.querySelector('* /deep/ #pen').click();
      chrome.test.assertEq('pen', tool.tool);
      chrome.test.assertEq(0.1429, tool.size);
      chrome.test.assertEq('#000000', tool.color);


      // Selected size and color.
      document.querySelector(
          '* /deep/ #pen /deep/ #sizes [value="1"]').click();
      document.querySelector(
          '* /deep/ #pen /deep/ #colors [value="#00b0ff"]').click();
      await animationFrame();
      chrome.test.assertEq('pen', tool.tool);
      chrome.test.assertEq(1, tool.size);
      chrome.test.assertEq('#00b0ff', tool.color);


      // Eraser defaults.
      document.querySelector('* /deep/ #eraser').click();
      chrome.test.assertEq('eraser', tool.tool);
      chrome.test.assertEq(1, tool.size);
      chrome.test.assertEq(null, tool.color);


      // Pen keeps previous settings.
      document.querySelector('* /deep/ #pen').click();
      chrome.test.assertEq('pen', tool.tool);
      chrome.test.assertEq(1, tool.size);
      chrome.test.assertEq('#00b0ff', tool.color);


      // Highlighter defaults.
      document.querySelector('* /deep/ #highlighter').click();
      chrome.test.assertEq('highlighter', tool.tool);
      chrome.test.assertEq(0.7143, tool.size);
      chrome.test.assertEq('#ffbc00', tool.color);


      // Need to expand to use this color.
      document.querySelector(
          '* /deep/ #highlighter /deep/ #colors [value="#d1c4e9"]').click();
      chrome.test.assertEq('#ffbc00', tool.color);

      // Selected size and expanded color.
      document.querySelector(
        '* /deep/ #highlighter /deep/ #sizes [value="1"]').click();
      document.querySelector(
        '* /deep/ #highlighter /deep/ #colors #expand').click();
      document.querySelector(
        '* /deep/ #highlighter /deep/ #colors [value="#d1c4e9"]').click();
      chrome.test.assertEq('highlighter', tool.tool);
      chrome.test.assertEq(1, tool.size);
      chrome.test.assertEq('#d1c4e9', tool.color);
    });
  },
  function testPointerEvents() {
    testAsync(async () => {
      // Still in annotation mode after previous test.
      const inkHost = contentElement();
      const events = [];
      inkHost.ink_.dispatchPointerEvent = (type, init) =>
          events.push({type: type, init: init});

      const mouse = {pointerId: 1, pointerType: 'mouse', buttons: 1};
      const pen = {
        pointerId: 2,
        pointerType: 'pen',
        pressure: 0.5,
        clientX: 3,
        clientY: 4,
        buttons: 0,
      };
      const touch1 = {pointerId: 11, pointerType: 'touch'};
      const touch2 = {pointerId: 22, pointerType: 'touch'};

      function checkExpectations(expectations) {
        chrome.test.assertEq(expectations.length, events.length);
        while (expectations.length) {
          const event = events.shift();
          const expectation = expectations.shift();
          chrome.test.assertEq(expectation.type, event.type);
          for (const key of Object.keys(expectation.init)) {
            chrome.test.assertEq(expectation.init[key], event.init[key]);
          }
        }
      }

      // Normal sequence.
      inkHost.dispatchEvent(new PointerEvent('pointerdown', pen));
      inkHost.dispatchEvent(new PointerEvent('pointermove', pen));
      inkHost.dispatchEvent(new PointerEvent('pointerup', pen));
      checkExpectations([
        {type: 'pointerdown', init: pen},
        {type: 'pointermove', init: pen},
        {type: 'pointerup', init: pen},
      ]);

      // Multi-touch gesture should cancel and suppress first pointer.
      inkHost.dispatchEvent(new PointerEvent('pointerdown', touch1));
      inkHost.dispatchEvent(new PointerEvent('pointerdown', touch2));
      inkHost.dispatchEvent(new PointerEvent('pointermove', touch1));
      inkHost.dispatchEvent(new PointerEvent('pointerup', touch1));
      checkExpectations([
        {type: 'pointerdown', init: touch1},
        {type: 'pointercancel', init: touch1},
      ]);

      // Pointers which are not active should be suppressed.
      inkHost.dispatchEvent(new PointerEvent('pointerdown', mouse));
      inkHost.dispatchEvent(new PointerEvent('pointerdown', pen));
      inkHost.dispatchEvent(new PointerEvent('pointerdown', touch1));
      inkHost.dispatchEvent(new PointerEvent('pointermove', mouse));
      inkHost.dispatchEvent(new PointerEvent('pointermove', pen));
      inkHost.dispatchEvent(new PointerEvent('pointermove', touch1));
      inkHost.dispatchEvent(new PointerEvent('pointerup', mouse));
      inkHost.dispatchEvent(new PointerEvent('pointermove', pen));
      checkExpectations([
        {type: 'pointerdown', init: mouse},
        {type: 'pointermove', init: mouse},
        {type: 'pointerup', init: mouse},
      ]);

      // pointerleave should cause mouseup
      inkHost.dispatchEvent(new PointerEvent('pointerdown', mouse));
      inkHost.dispatchEvent(new PointerEvent('pointerleave', mouse));
      checkExpectations([
        {type: 'pointerdown', init: mouse},
        {type: 'pointerup', init: mouse},
      ]);

      // pointerleave does not apply to non-mouse pointers
      inkHost.dispatchEvent(new PointerEvent('pointerdown', pen));
      inkHost.dispatchEvent(new PointerEvent('pointerleave', pen));
      inkHost.dispatchEvent(new PointerEvent('pointerup', pen));
      checkExpectations([
        {type: 'pointerdown', init: pen},
        {type: 'pointerup', init: pen},
      ]);

      // Browser will cancel touch on pen input
      inkHost.dispatchEvent(new PointerEvent('pointerdown', touch1));
      inkHost.dispatchEvent(new PointerEvent('pointercancel', touch1));
      inkHost.dispatchEvent(new PointerEvent('pointerdown', pen));
      inkHost.dispatchEvent(new PointerEvent('pointerup', pen));
      checkExpectations([
        {type: 'pointerdown', init: touch1},
        {type: 'pointercancel', init: touch1},
        {type: 'pointerdown', init: pen},
        {type: 'pointerup', init: pen},
      ]);
    });
  },
]);
