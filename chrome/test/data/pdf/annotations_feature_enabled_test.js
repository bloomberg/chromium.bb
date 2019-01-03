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
  }
]);
