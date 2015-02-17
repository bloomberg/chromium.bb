// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function NavigateInCurrentTabCallback() {
  this.navigateInCurrentTabCalled = false;
  this.callback = function() {
    this.navigateInCurrentTabCalled = true;
  }.bind(this);
  this.reset = function() {
    this.navigateInCurrentTabCalled = false;
  };
}

function NavigateInNewTabCallback() {
  this.navigateInNewTabCalled = false;
  this.callback = function() {
    this.navigateInNewTabCalled = true;
  }.bind(this);
  this.reset = function() {
    this.navigateInNewTabCalled = false;
  };
}

var tests = [
  /**
   * Test navigation within the page, opening a url in the same tab and
   * opening a url in the new tab.
   */
  function testNavigate() {
    var mockWindow = new MockWindow(100, 100);
    var mockSizer = new MockSizer();
    var mockCallback = new MockViewportChangedCallback();
    var viewport = new Viewport(mockWindow, mockSizer, mockCallback.callback,
                                function() {}, function() {}, 0, 0);

    var paramsParser = new OpenPDFParamsParser(function(name) {
      if (name == 'US')
        paramsParser.onNamedDestinationReceived(0);
      else if (name == 'UY')
        paramsParser.onNamedDestinationReceived(2);
      else
        paramsParser.onNamedDestinationReceived(-1);
    });
    var url = "http://xyz.pdf";

    var navigateInCurrentTabCallback = new NavigateInCurrentTabCallback();
    var navigateInNewTabCallback = new NavigateInNewTabCallback();
    var navigator = new Navigator(url, viewport, paramsParser,
        navigateInCurrentTabCallback.callback,
        navigateInNewTabCallback.callback);

    var documentDimensions = new MockDocumentDimensions();
    documentDimensions.addPage(100, 100);
    documentDimensions.addPage(200, 200);
    documentDimensions.addPage(100, 400);
    viewport.setDocumentDimensions(documentDimensions);
    viewport.setZoom(1);

    mockCallback.reset();
    // This should move viewport to page 0.
    navigator.navigate(url + "#US", false);
    chrome.test.assertTrue(mockCallback.wasCalled);
    chrome.test.assertEq(0, viewport.position.x);
    chrome.test.assertEq(0, viewport.position.y);

    mockCallback.reset();
    navigateInNewTabCallback.reset();
    // This should open "http://xyz.pdf#US" in a new tab. So current tab
    // viewport should not update and viewport position should remain same.
    navigator.navigate(url + "#US", true);
    chrome.test.assertFalse(mockCallback.wasCalled);
    chrome.test.assertTrue(navigateInNewTabCallback.navigateInNewTabCalled);
    chrome.test.assertEq(0, viewport.position.x);
    chrome.test.assertEq(0, viewport.position.y);

    mockCallback.reset();
    // This should move viewport to page 2.
    navigator.navigate(url + "#UY", false);
    chrome.test.assertTrue(mockCallback.wasCalled);
    chrome.test.assertEq(0, viewport.position.x);
    chrome.test.assertEq(300, viewport.position.y);

    mockCallback.reset();
    navigateInCurrentTabCallback.reset();
    // #ABC is not a named destination in the page so viewport should not
    // update and viewport position should remain same. As this link will open
    // in the same tab.
    navigator.navigate(url + "#ABC", false);
    chrome.test.assertFalse(mockCallback.wasCalled);
    chrome.test.assertTrue(
        navigateInCurrentTabCallback.navigateInCurrentTabCalled);
    chrome.test.assertEq(0, viewport.position.x);
    chrome.test.assertEq(300, viewport.position.y);

    chrome.test.succeed();
  }
];

var scriptingAPI = new PDFScriptingAPI(window, window);
scriptingAPI.setLoadCallback(function() {
  chrome.test.runTests(tests);
});
