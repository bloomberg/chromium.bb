// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var tests = [
  /**
   * Test named destinations.
   */
  function testParamsParser() {
    var paramsParser = new OpenPDFParamsParser(function(name) {
      if (name == 'RU')
        paramsParser.onNamedDestinationReceived(26);
      else if (name == 'US')
        paramsParser.onNamedDestinationReceived(0);
      else if (name == 'UY')
        paramsParser.onNamedDestinationReceived(22);
      else
        paramsParser.onNamedDestinationReceived(-1);
    });

    var url = "http://xyz.pdf";

    // Checking #nameddest.
    paramsParser.getViewportFromUrlParams(
        url + "#RU", function(viewportPosition) {
          chrome.test.assertEq(viewportPosition.page, 26);
    });

    // Checking #nameddest=name.
    paramsParser.getViewportFromUrlParams(
        url + "#nameddest=US", function(viewportPosition) {
          chrome.test.assertEq(viewportPosition.page, 0);
    });

    // Checking #page=pagenum nameddest.The document first page has a pagenum
    // value of 1.
    paramsParser.getViewportFromUrlParams(
        url + "#page=6", function(viewportPosition) {
          chrome.test.assertEq(viewportPosition.page, 5);
    });

    // Checking #zoom=scale.
    paramsParser.getViewportFromUrlParams(
        url + "#zoom=200", function(viewportPosition) {
          chrome.test.assertEq(viewportPosition.zoom, 2);
    });

    // Checking #zoom=scale,left,top.
    paramsParser.getViewportFromUrlParams(
        url + "#zoom=200,100,200", function(viewportPosition) {
          chrome.test.assertEq(viewportPosition.zoom, 2);
          chrome.test.assertEq(viewportPosition.position.x, 100);
          chrome.test.assertEq(viewportPosition.position.y, 200);
    });

    // Checking #nameddest=name and zoom=scale.
    paramsParser.getViewportFromUrlParams(
        url + "#nameddest=UY&zoom=150", function(viewportPosition) {
          chrome.test.assertEq(viewportPosition.page, 22);
          chrome.test.assertEq(viewportPosition.zoom, 1.5);
    });

    // Checking #page=pagenum and zoom=scale.
    paramsParser.getViewportFromUrlParams(
        url + "#page=2&zoom=250", function(viewportPosition) {
          chrome.test.assertEq(viewportPosition.page, 1);
          chrome.test.assertEq(viewportPosition.zoom, 2.5);
    });

    // Checking #nameddest=name and zoom=scale,left,top.
    paramsParser.getViewportFromUrlParams(
        url + "#nameddest=UY&zoom=150,100,200", function(viewportPosition) {
          chrome.test.assertEq(viewportPosition.page, 22);
          chrome.test.assertEq(viewportPosition.zoom, 1.5);
          chrome.test.assertEq(viewportPosition.position.x, 100);
          chrome.test.assertEq(viewportPosition.position.y, 200);
    });

    // Checking #page=pagenum and zoom=scale,left,top.
    paramsParser.getViewportFromUrlParams(
        url + "#page=2&zoom=250,100,200", function(viewportPosition) {
          chrome.test.assertEq(viewportPosition.page, 1);
          chrome.test.assertEq(viewportPosition.zoom, 2.5);
          chrome.test.assertEq(viewportPosition.position.x, 100);
          chrome.test.assertEq(viewportPosition.position.y, 200);
    });

    // Checking #toolbar=0 to disable the toolbar.
    var uiParams = paramsParser.getUiUrlParams(url + "#toolbar=0");
    chrome.test.assertFalse(uiParams.toolbar);
    uiParams = paramsParser.getUiUrlParams(url + "#toolbar=1");
    chrome.test.assertTrue(uiParams.toolbar);

    chrome.test.succeed();
  }
];

var scriptingAPI = new PDFScriptingAPI(window, window);
scriptingAPI.setLoadCallback(function() {
  chrome.test.runTests(tests);
});
