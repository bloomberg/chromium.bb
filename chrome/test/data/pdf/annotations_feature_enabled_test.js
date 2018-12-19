// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


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
  }
]);
