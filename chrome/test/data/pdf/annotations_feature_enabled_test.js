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
  }
]);
