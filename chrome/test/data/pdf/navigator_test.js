// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function NavigateInCurrentTabCallback() {
  this.navigateCalled = false;
  this.url = undefined;
  this.callback = function(url) {
    this.navigateCalled = true;
    this.url = url;
  }.bind(this);
  this.reset = function() {
    this.navigateCalled = false;
    this.url = undefined;
  };
}

function NavigateInNewTabCallback() {
  this.navigateCalled = false;
  this.url = undefined;
  this.callback = function(url) {
    this.navigateCalled = true;
    this.url = url;
  }.bind(this);
  this.reset = function() {
    this.navigateCalled = false;
    this.url = undefined;
  };
}

/**
 * Given a |navigator|, navigate to |url| in the current tab or new tab,
 * depending on the value of |openInNewTab|. Use |viewportChangedCallback|
 * and |navigateCallback| to check the callbacks, and that the navigation
 * to |expectedResultUrl| happened.
 */
function doNavigationUrlTest(
    navigator,
    url,
    openInNewTab,
    expectedResultUrl,
    viewportChangedCallback,
    navigateCallback) {
  viewportChangedCallback.reset();
  navigateCallback.reset();
  navigator.navigate(url, openInNewTab);
  chrome.test.assertFalse(viewportChangedCallback.wasCalled);
  chrome.test.assertTrue(navigateCallback.navigateCalled);
  chrome.test.assertEq(expectedResultUrl, navigateCallback.url);
}

/**
 * Helper function to run doNavigationUrlTest() for the current tab and a new
 * tab.
 */
function doNavigationUrlTestInCurrentTabAndNewTab(
    navigator,
    url,
    expectedResultUrl,
    viewportChangedCallback,
    navigateInCurrentTabCallback,
    navigateInNewTabCallback) {
  doNavigationUrlTest(navigator, url, false, expectedResultUrl,
      viewportChangedCallback, navigateInCurrentTabCallback);
  doNavigationUrlTest(navigator, url, true, expectedResultUrl,
      viewportChangedCallback, navigateInNewTabCallback);
}

var tests = [
  /**
   * Test navigation within the page, opening a url in the same tab and
   * opening a url in a new tab.
   */
  function testNavigate() {
    var mockWindow = new MockWindow(100, 100);
    var mockSizer = new MockSizer();
    var mockCallback = new MockViewportChangedCallback();
    var viewport = new Viewport(mockWindow, mockSizer, mockCallback.callback,
                                function() {}, function() {}, 0, 1, 0);

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
    chrome.test.assertTrue(navigateInNewTabCallback.navigateCalled);
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
    chrome.test.assertTrue(navigateInCurrentTabCallback.navigateCalled);
    chrome.test.assertEq(0, viewport.position.x);
    chrome.test.assertEq(300, viewport.position.y);

    chrome.test.succeed();
  },
  /**
   * Test opening a url in the same tab and opening a url in a new tab for
   * a http:// url as the current location. The destination url may not have
   * a valid scheme, so the navigator must determine the url by following
   * similar heuristics as Adobe Acrobat Reader.
   */
  function testNavigateForLinksWithoutScheme() {
    var mockWindow = new MockWindow(100, 100);
    var mockSizer = new MockSizer();
    var mockCallback = new MockViewportChangedCallback();
    var viewport = new Viewport(mockWindow, mockSizer, mockCallback.callback,
                                function() {}, function() {}, 0, 1, 0);

    var paramsParser = new OpenPDFParamsParser(function(name) {
      paramsParser.onNamedDestinationReceived(-1);
    });
    var url = "http://www.example.com/subdir/xyz.pdf";

    var navigateInCurrentTabCallback = new NavigateInCurrentTabCallback();
    var navigateInNewTabCallback = new NavigateInNewTabCallback();
    var navigator = new Navigator(url, viewport, paramsParser,
        navigateInCurrentTabCallback.callback,
        navigateInNewTabCallback.callback);

    // Sanity check.
    doNavigationUrlTestInCurrentTabAndNewTab(
        navigator,
        'https://www.foo.com/bar.pdf',
        'https://www.foo.com/bar.pdf',
        mockCallback,
        navigateInCurrentTabCallback,
        navigateInNewTabCallback);

    // Open relative links.
    doNavigationUrlTestInCurrentTabAndNewTab(
        navigator,
        'foo/bar.pdf',
        'http://www.example.com/subdir/foo/bar.pdf',
        mockCallback,
        navigateInCurrentTabCallback,
        navigateInNewTabCallback);
    doNavigationUrlTestInCurrentTabAndNewTab(
        navigator,
        'foo.com/bar.pdf',
        'http://www.example.com/subdir/foo.com/bar.pdf',
        mockCallback,
        navigateInCurrentTabCallback,
        navigateInNewTabCallback);
    // The expected result is not normalized here.
    doNavigationUrlTestInCurrentTabAndNewTab(
        navigator,
        '../www.foo.com/bar.pdf',
        'http://www.example.com/subdir/../www.foo.com/bar.pdf',
        mockCallback,
        navigateInCurrentTabCallback,
        navigateInNewTabCallback);

    // Open an absolute link.
    doNavigationUrlTestInCurrentTabAndNewTab(
        navigator,
        '/foodotcom/bar.pdf',
        'http://www.example.com/foodotcom/bar.pdf',
        mockCallback,
        navigateInCurrentTabCallback,
        navigateInNewTabCallback);

    // Open a http url without a scheme.
    doNavigationUrlTestInCurrentTabAndNewTab(
        navigator,
        'www.foo.com/bar.pdf',
        'http://www.foo.com/bar.pdf',
        mockCallback,
        navigateInCurrentTabCallback,
        navigateInNewTabCallback);

    // Test three dots.
    doNavigationUrlTestInCurrentTabAndNewTab(
        navigator,
        '.../bar.pdf',
        'http://www.example.com/subdir/.../bar.pdf',
        mockCallback,
        navigateInCurrentTabCallback,
        navigateInNewTabCallback);

    // Test forward slashes.
    doNavigationUrlTestInCurrentTabAndNewTab(
        navigator,
        '..\\bar.pdf',
        'http://www.example.com/subdir/..\\bar.pdf',
        mockCallback,
        navigateInCurrentTabCallback,
        navigateInNewTabCallback);
    doNavigationUrlTestInCurrentTabAndNewTab(
        navigator,
        '.\\bar.pdf',
        'http://www.example.com/subdir/.\\bar.pdf',
        mockCallback,
        navigateInCurrentTabCallback,
        navigateInNewTabCallback);
    doNavigationUrlTestInCurrentTabAndNewTab(
        navigator,
        '\\bar.pdf',
        'http://www.example.com/subdir/\\bar.pdf',
        mockCallback,
        navigateInCurrentTabCallback,
        navigateInNewTabCallback);

    // Regression test for https://crbug.com/569040
    doNavigationUrlTestInCurrentTabAndNewTab(
        navigator,
        'http://something.else/foo#page=5',
        'http://something.else/foo#page=5',
        mockCallback,
        navigateInCurrentTabCallback,
        navigateInNewTabCallback);

    chrome.test.succeed();
  },
  /**
   * Test opening a url in the same tab and opening a url in a new tab with a
   * file:/// url as the current location.
   */
  function testNavigateFromLocalFile() {
    var mockWindow = new MockWindow(100, 100);
    var mockSizer = new MockSizer();
    var mockCallback = new MockViewportChangedCallback();
    var viewport = new Viewport(mockWindow, mockSizer, mockCallback.callback,
                                function() {}, function() {}, 0, 1, 0);

    var paramsParser = new OpenPDFParamsParser(function(name) {
      paramsParser.onNamedDestinationReceived(-1);
    });
    var url = "file:///some/path/to/myfile.pdf";

    var navigateInCurrentTabCallback = new NavigateInCurrentTabCallback();
    var navigateInNewTabCallback = new NavigateInNewTabCallback();
    var navigator = new Navigator(url, viewport, paramsParser,
        navigateInCurrentTabCallback.callback,
        navigateInNewTabCallback.callback);

    // Open an absolute link.
    doNavigationUrlTestInCurrentTabAndNewTab(
        navigator,
        '/foodotcom/bar.pdf',
        'file:///foodotcom/bar.pdf',
        mockCallback,
        navigateInCurrentTabCallback,
        navigateInNewTabCallback);

    chrome.test.succeed();
  }
];

var scriptingAPI = new PDFScriptingAPI(window, window);
scriptingAPI.setLoadCallback(function() {
  chrome.test.runTests(tests);
});
