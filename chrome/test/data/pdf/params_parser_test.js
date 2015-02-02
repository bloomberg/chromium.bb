// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var tests = [
  /**
   * Test named destinations.
   */
  function testParamsParser() {
    var paramsParser = new OpenPDFParamsParser();
    // Assigning page number for #nameddests.
    paramsParser.namedDestinations['RU'] = 26;
    paramsParser.namedDestinations['US'] = 0;
    paramsParser.namedDestinations['UY'] = 22;

    var url = "http://xyz.pdf";

    // Checking #nameddest.
    var urlParams = paramsParser.getViewportFromUrlParams(url + "#RU");
    chrome.test.assertEq(urlParams.page, 26);

    // Checking #nameddest=name.
    urlParams = paramsParser.getViewportFromUrlParams(url + "#nameddest=US");
    chrome.test.assertEq(urlParams.page, 0);

    // Checking #page=pagenum nameddest.The document first page has a pagenum
    // value of 1.
    urlParams = paramsParser.getViewportFromUrlParams(url + "#page=6");
    chrome.test.assertEq(urlParams.page, 5);

    // Checking #zoom=scale.
    urlParams = paramsParser.getViewportFromUrlParams(url + "#zoom=200");
    chrome.test.assertEq(urlParams.zoom, 2);

    // Checking #zoom=scale,left,top.
    urlParams = paramsParser.getViewportFromUrlParams(url +
        "#zoom=200,100,200");
    chrome.test.assertEq(urlParams.zoom, 2);
    chrome.test.assertEq(urlParams.position.x, 100);
    chrome.test.assertEq(urlParams.position.y, 200);

    // Checking #nameddest=name and zoom=scale.
    urlParams = paramsParser.getViewportFromUrlParams(url +
        "#nameddest=UY&zoom=150");
    chrome.test.assertEq(urlParams.page, 22);
    chrome.test.assertEq(urlParams.zoom, 1.5);

    // Checking #page=pagenum and zoom=scale.
    urlParams = paramsParser.getViewportFromUrlParams(url +
        "#page=2&zoom=250");
    chrome.test.assertEq(urlParams.page, 1);
    chrome.test.assertEq(urlParams.zoom, 2.5);

    // Checking #nameddest=name and zoom=scale,left,top.
    urlParams = paramsParser.getViewportFromUrlParams(url +
        "#nameddest=UY&zoom=150,100,200");
    chrome.test.assertEq(urlParams.page, 22);
    chrome.test.assertEq(urlParams.zoom, 1.5);
    chrome.test.assertEq(urlParams.position.x, 100);
    chrome.test.assertEq(urlParams.position.y, 200);

    // Checking #page=pagenum and zoom=scale,left,top.
    urlParams = paramsParser.getViewportFromUrlParams(url +
        "#page=2&zoom=250,100,200");
    chrome.test.assertEq(urlParams.page, 1);
    chrome.test.assertEq(urlParams.zoom, 2.5);
    chrome.test.assertEq(urlParams.position.x, 100);
    chrome.test.assertEq(urlParams.position.y, 200);

    chrome.test.succeed();
  }
];

var scriptingAPI = new PDFScriptingAPI(window, window);
scriptingAPI.setLoadCallback(function() {
  chrome.test.runTests(tests);
});
