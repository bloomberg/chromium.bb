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
        url + '#RU', function(viewportPosition) {
          chrome.test.assertEq(26, viewportPosition.page);
        });

    // Checking #nameddest=name.
    paramsParser.getViewportFromUrlParams(
        url + '#nameddest=US', function(viewportPosition) {
          chrome.test.assertEq(0, viewportPosition.page);
        });

    // Checking #page=pagenum nameddest.The document first page has a pagenum
    // value of 1.
    paramsParser.getViewportFromUrlParams(
        url + '#page=6', function(viewportPosition) {
          chrome.test.assertEq(5, viewportPosition.page);
        });

    // Checking #zoom=scale.
    paramsParser.getViewportFromUrlParams(
        url + '#zoom=200', function(viewportPosition) {
          chrome.test.assertEq(2, viewportPosition.zoom);
        });

    // Checking #zoom=scale,left,top.
    paramsParser.getViewportFromUrlParams(
        url + '#zoom=200,100,200', function(viewportPosition) {
          chrome.test.assertEq(2, viewportPosition.zoom);
          chrome.test.assertEq(100, viewportPosition.position.x);
          chrome.test.assertEq(200, viewportPosition.position.y);
        });

    // Checking #nameddest=name and zoom=scale.
    paramsParser.getViewportFromUrlParams(
        url + '#nameddest=UY&zoom=150', function(viewportPosition) {
          chrome.test.assertEq(22, viewportPosition.page);
          chrome.test.assertEq(1.5, viewportPosition.zoom);
        });

    // Checking #page=pagenum and zoom=scale.
    paramsParser.getViewportFromUrlParams(
        url + '#page=2&zoom=250', function(viewportPosition) {
          chrome.test.assertEq(1, viewportPosition.page);
          chrome.test.assertEq(2.5, viewportPosition.zoom);
        });

    // Checking #nameddest=name and zoom=scale,left,top.
    paramsParser.getViewportFromUrlParams(
        url + '#nameddest=UY&zoom=150,100,200', function(viewportPosition) {
          chrome.test.assertEq(22, viewportPosition.page);
          chrome.test.assertEq(1.5, viewportPosition.zoom);
          chrome.test.assertEq(100, viewportPosition.position.x);
          chrome.test.assertEq(200, viewportPosition.position.y);
        });

    // Checking #page=pagenum and zoom=scale,left,top.
    paramsParser.getViewportFromUrlParams(
        url + '#page=2&zoom=250,100,200', function(viewportPosition) {
          chrome.test.assertEq(1, viewportPosition.page);
          chrome.test.assertEq(2.5, viewportPosition.zoom);
          chrome.test.assertEq(100, viewportPosition.position.x);
          chrome.test.assertEq(200, viewportPosition.position.y);
        });

    // Checking #view=Fit.
    paramsParser.getViewportFromUrlParams(
        url + '#view=Fit', function(viewportPosition) {
          chrome.test.assertEq(FittingType.FIT_TO_PAGE, viewportPosition.view);
        });
    // Checking #view=FitH.
    paramsParser.getViewportFromUrlParams(
        url + '#view=FitH', function(viewportPosition) {
          chrome.test.assertEq(FittingType.FIT_TO_WIDTH, viewportPosition.view);
        });
    // Checking #view=FitV.
    paramsParser.getViewportFromUrlParams(
        url + '#view=FitV', function(viewportPosition) {
          chrome.test.assertEq(
              FittingType.FIT_TO_HEIGHT, viewportPosition.view);
        });
    // Checking #view=[wrong parameter].
    paramsParser.getViewportFromUrlParams(
        url + '#view=FitW', function(viewportPosition) {
          chrome.test.assertEq(undefined, viewportPosition.view);
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
