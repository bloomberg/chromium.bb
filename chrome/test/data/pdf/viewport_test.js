// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function MockWindow(width, height) {
  this.innerWidth = width;
  this.innerHeight = height;
  this.addEventListener = function(e, f) {
    if (e == 'scroll')
      this.scrollCallback = f;
    if (e == 'resize')
      this.resizeCallback = f;
  },
  this.scrollTo = function(x, y) {
    this.pageXOffset = Math.max(0, x);
    this.pageYOffset = Math.max(0, y);
    this.scrollCallback();
  };
  this.pageXOffset = 0;
  this.pageYOffset = 0;
  this.scrollCallback = null;
  this.resizeCallback = null;
}

function MockSizer() {
  this.style = {
    width: '0px',
    height: '0px'
  };
}

function MockViewportChangedCallback() {
  this.wasCalled = false;
  this.callback = function() {
    this.wasCalled = true;
  }.bind(this);
  this.reset = function() {
    this.wasCalled = false;
  };
}

function MockDocumentDimensions(width, height) {
  this.width = width || 0;
  this.height = height ? height : 0;
  this.pageDimensions = [];
  this.addPage = function(w, h) {
    var y = 0;
    if (this.pageDimensions.length != 0) {
      y = this.pageDimensions[this.pageDimensions.length - 1].y +
          this.pageDimensions[this.pageDimensions.length - 1].height;
    }
    this.width = Math.max(this.width, w);
    this.height += h;
    this.pageDimensions.push({
      x: 0,
      y: y,
      width: w,
      height: h
    });
  };
  this.reset = function() {
    this.width = 0;
    this.height = 0;
    this.pageDimensions = [];
  };
}

var tests = [
  function testDocumentNeedsScrollbars() {
    var viewport = new Viewport(new MockWindow(100, 100), new MockSizer(),
                                function() {}, function() {}, function() {}, 0);
    var scrollbars;

    viewport.setDocumentDimensions(new MockDocumentDimensions(90, 90));
    scrollbars = viewport.documentNeedsScrollbars_(1);
    chrome.test.assertFalse(scrollbars.vertical);
    chrome.test.assertFalse(scrollbars.horizontal);

    viewport.setDocumentDimensions(new MockDocumentDimensions(100, 100));
    scrollbars = viewport.documentNeedsScrollbars_(1);
    chrome.test.assertFalse(scrollbars.vertical);
    chrome.test.assertFalse(scrollbars.horizontal);

    viewport.setDocumentDimensions(new MockDocumentDimensions(110, 110));
    scrollbars = viewport.documentNeedsScrollbars_(1);
    chrome.test.assertTrue(scrollbars.vertical);
    chrome.test.assertTrue(scrollbars.horizontal);

    viewport.setDocumentDimensions(new MockDocumentDimensions(100, 101));
    scrollbars = viewport.documentNeedsScrollbars_(1);
    chrome.test.assertTrue(scrollbars.vertical);
    chrome.test.assertFalse(scrollbars.horizontal);

    viewport.setDocumentDimensions(new MockDocumentDimensions(40, 51));
    scrollbars = viewport.documentNeedsScrollbars_(2);
    chrome.test.assertTrue(scrollbars.vertical);
    chrome.test.assertFalse(scrollbars.horizontal);

    viewport.setDocumentDimensions(new MockDocumentDimensions(101, 202));
    scrollbars = viewport.documentNeedsScrollbars_(0.5);
    chrome.test.assertTrue(scrollbars.vertical);
    chrome.test.assertFalse(scrollbars.horizontal);
    chrome.test.succeed();
  },

  function testSetZoom() {
    var mockWindow = new MockWindow(100, 100);
    var mockSizer = new MockSizer();
    var mockCallback = new MockViewportChangedCallback();
    var viewport = new Viewport(mockWindow, mockSizer, mockCallback.callback,
                                function() {}, function() {}, 0);

    // Test setting the zoom without the document dimensions set. The sizer
    // shouldn't change size.
    mockCallback.reset();
    viewport.setZoom(0.5);
    chrome.test.assertEq(0.5, viewport.zoom);
    chrome.test.assertTrue(mockCallback.wasCalled);
    chrome.test.assertEq('0px', mockSizer.style.width);
    chrome.test.assertEq('0px', mockSizer.style.height);
    chrome.test.assertEq(0, mockWindow.pageXOffset);
    chrome.test.assertEq(0, mockWindow.pageYOffset);

    viewport.setZoom(1);
    viewport.setDocumentDimensions(new MockDocumentDimensions(200, 200));

    // Test zooming out.
    mockCallback.reset();
    viewport.setZoom(0.5);
    chrome.test.assertEq(0.5, viewport.zoom);
    chrome.test.assertTrue(mockCallback.wasCalled);
    chrome.test.assertEq('100px', mockSizer.style.width);
    chrome.test.assertEq('100px', mockSizer.style.height);

    // Test zooming in.
    mockCallback.reset();
    viewport.setZoom(2);
    chrome.test.assertEq(2, viewport.zoom);
    chrome.test.assertTrue(mockCallback.wasCalled);
    chrome.test.assertEq('400px', mockSizer.style.width);
    chrome.test.assertEq('400px', mockSizer.style.height);

    // Test that the scroll position scales correctly. It scales relative to the
    // center of the page.
    viewport.setZoom(1);
    mockWindow.pageXOffset = 50;
    mockWindow.pageYOffset = 50;
    viewport.setZoom(2);
    chrome.test.assertEq('400px', mockSizer.style.width);
    chrome.test.assertEq('400px', mockSizer.style.height);
    chrome.test.assertEq(150, mockWindow.pageXOffset);
    chrome.test.assertEq(150, mockWindow.pageYOffset);
    chrome.test.succeed();
  },

  function testGetMostVisiblePage() {
    var mockWindow = new MockWindow(100, 100);
    var viewport = new Viewport(mockWindow, new MockSizer(), function() {},
                                function() {}, function() {}, 0);

    var documentDimensions = new MockDocumentDimensions(100, 100);
    documentDimensions.addPage(100, 100);
    documentDimensions.addPage(150, 100);
    documentDimensions.addPage(100, 200);
    viewport.setDocumentDimensions(documentDimensions);
    viewport.setZoom(1);

    // Scrolled to the start of the first page.
    mockWindow.scrollTo(0, 0);
    chrome.test.assertEq(0, viewport.getMostVisiblePage());

    // Scrolled to the start of the second page.
    mockWindow.scrollTo(0, 100);
    chrome.test.assertEq(1, viewport.getMostVisiblePage());

    // Scrolled half way through the first page.
    mockWindow.scrollTo(0, 50);
    chrome.test.assertEq(0, viewport.getMostVisiblePage());

    // Scrolled just over half way through the first page.
    mockWindow.scrollTo(0, 51);
    chrome.test.assertEq(1, viewport.getMostVisiblePage());

    // Scrolled most of the way through the second page.
    mockWindow.scrollTo(0, 180);
    chrome.test.assertEq(2, viewport.getMostVisiblePage());

    // Scrolled just over half way through the first page with 2x zoom.
    viewport.setZoom(2);
    mockWindow.scrollTo(0, 151);
    chrome.test.assertEq(1, viewport.getMostVisiblePage());
    chrome.test.succeed();
  },

  function testFitToWidth() {
    var mockWindow = new MockWindow(100, 100);
    var mockSizer = new MockSizer();
    var mockCallback = new MockViewportChangedCallback();
    var viewport = new Viewport(mockWindow, mockSizer, mockCallback.callback,
                                function() {}, function() {}, 0);
    var documentDimensions = new MockDocumentDimensions();

    // Test with a document width which matches the window width.
    documentDimensions.addPage(100, 100);
    viewport.setDocumentDimensions(documentDimensions);
    viewport.setZoom(0.1);
    mockCallback.reset();
    viewport.fitToWidth();
    chrome.test.assertTrue(mockCallback.wasCalled);
    chrome.test.assertEq('100px', mockSizer.style.width);
    chrome.test.assertEq(1, viewport.zoom);

    // Test with a document width which is twice the size of the window width.
    documentDimensions.reset();
    documentDimensions.addPage(200, 100);
    viewport.setDocumentDimensions(documentDimensions);
    mockCallback.reset();
    viewport.fitToWidth();
    chrome.test.assertTrue(mockCallback.wasCalled);
    chrome.test.assertEq('100px', mockSizer.style.width);
    chrome.test.assertEq(0.5, viewport.zoom);

    // Test with a document width which is half the size of the window width.
    documentDimensions.reset();
    documentDimensions.addPage(50, 100);
    viewport.setDocumentDimensions(documentDimensions);
    mockCallback.reset();
    viewport.fitToWidth();
    chrome.test.assertTrue(mockCallback.wasCalled);
    chrome.test.assertEq('100px', mockSizer.style.width);
    chrome.test.assertEq(2, viewport.zoom);

    // Test that the scroll position stays the same relative to the page after
    // fit to page is called.
    documentDimensions.reset();
    documentDimensions.addPage(50, 400);
    viewport.setDocumentDimensions(documentDimensions);
    viewport.setZoom(1);
    mockWindow.scrollTo(0, 100);
    mockCallback.reset();
    viewport.fitToWidth();
    chrome.test.assertTrue(mockCallback.wasCalled);
    chrome.test.assertEq(2, viewport.zoom);
    chrome.test.assertEq(0, viewport.position.x);
    chrome.test.assertEq(200, viewport.position.y);

    // Test fitting works with scrollbars. The page will need to be zoomed to
    // fit to width, which will cause the page height to span outside of the
    // viewport, triggering 15px scrollbars to be shown.
    viewport = new Viewport(mockWindow, mockSizer, mockCallback.callback,
                            function() {}, function() {}, 15);
    documentDimensions.reset();
    documentDimensions.addPage(50, 100);
    viewport.setDocumentDimensions(documentDimensions);
    mockCallback.reset();
    viewport.fitToWidth();
    chrome.test.assertTrue(mockCallback.wasCalled);
    chrome.test.assertEq('85px', mockSizer.style.width);
    chrome.test.assertEq(1.7, viewport.zoom);
    chrome.test.succeed();
  },

  function testFitToPage() {
    var mockWindow = new MockWindow(100, 100);
    var mockSizer = new MockSizer();
    var mockCallback = new MockViewportChangedCallback();
    var viewport = new Viewport(mockWindow, mockSizer, mockCallback.callback,
                                function() {}, function() {}, 0);
    var documentDimensions = new MockDocumentDimensions();

    // Test with a page size which matches the window size.
    documentDimensions.addPage(100, 100);
    viewport.setDocumentDimensions(documentDimensions);
    viewport.setZoom(0.1);
    mockCallback.reset();
    viewport.fitToPage();
    chrome.test.assertTrue(mockCallback.wasCalled);
    chrome.test.assertEq('100px', mockSizer.style.width);
    chrome.test.assertEq('100px', mockSizer.style.height);
    chrome.test.assertEq(1, viewport.zoom);

    // Test with a page size whose width is larger than its height.
    documentDimensions.reset();
    documentDimensions.addPage(200, 100);
    viewport.setDocumentDimensions(documentDimensions);
    mockCallback.reset();
    viewport.fitToPage();
    chrome.test.assertTrue(mockCallback.wasCalled);
    chrome.test.assertEq('100px', mockSizer.style.width);
    chrome.test.assertEq('50px', mockSizer.style.height);
    chrome.test.assertEq(0.5, viewport.zoom);

    // Test with a page size whose height is larger than its width.
    documentDimensions.reset();
    documentDimensions.addPage(100, 200);
    viewport.setDocumentDimensions(documentDimensions);
    mockCallback.reset();
    viewport.fitToPage();
    chrome.test.assertTrue(mockCallback.wasCalled);
    chrome.test.assertEq('50px', mockSizer.style.width);
    chrome.test.assertEq('100px', mockSizer.style.height);
    chrome.test.assertEq(0.5, viewport.zoom);

    // Test that when there are multiple pages the most visible page is sized
    // to.
    documentDimensions.reset();
    documentDimensions.addPage(200, 100);
    documentDimensions.addPage(100, 400);
    viewport.setDocumentDimensions(documentDimensions);
    viewport.setZoom(1);
    mockWindow.scrollTo(0, 0);
    mockCallback.reset();
    viewport.fitToPage();
    chrome.test.assertTrue(mockCallback.wasCalled);
    chrome.test.assertEq('100px', mockSizer.style.width);
    chrome.test.assertEq('250px', mockSizer.style.height);
    chrome.test.assertEq(0.5, viewport.zoom);
    viewport.setZoom(1);
    mockWindow.scrollTo(0, 100);
    mockCallback.reset();
    viewport.fitToPage();
    chrome.test.assertTrue(mockCallback.wasCalled);
    chrome.test.assertEq('50px', mockSizer.style.width);
    chrome.test.assertEq('125px', mockSizer.style.height);
    chrome.test.assertEq(0.25, viewport.zoom);

    // Test that the top of the most visible page is scrolled to.
    documentDimensions.reset();
    documentDimensions.addPage(200, 200);
    documentDimensions.addPage(100, 400);
    viewport.setDocumentDimensions(documentDimensions);
    viewport.setZoom(1);
    mockWindow.scrollTo(0, 0);
    viewport.fitToPage();
    chrome.test.assertEq(0.5, viewport.zoom);
    chrome.test.assertEq(0, viewport.position.x);
    chrome.test.assertEq(0, viewport.position.y);
    viewport.setZoom(1);
    mockWindow.scrollTo(0, 175);
    viewport.fitToPage();
    chrome.test.assertEq(0.25, viewport.zoom);
    // The page will be centred because it is less than the document width.
    chrome.test.assertEq(12.5, viewport.position.x);
    chrome.test.assertEq(50, viewport.position.y);
    chrome.test.succeed();
  },

  function testGoToPage() {
    var mockWindow = new MockWindow(100, 100);
    var mockSizer = new MockSizer();
    var mockCallback = new MockViewportChangedCallback();
    var viewport = new Viewport(mockWindow, mockSizer, mockCallback.callback,
                                function() {}, function() {}, 0);
    var documentDimensions = new MockDocumentDimensions();

    documentDimensions.addPage(100, 100);
    documentDimensions.addPage(200, 200);
    documentDimensions.addPage(100, 400);
    viewport.setDocumentDimensions(documentDimensions);
    viewport.setZoom(1);

    mockCallback.reset();
    viewport.goToPage(0);
    chrome.test.assertTrue(mockCallback.wasCalled);
    chrome.test.assertEq(0, viewport.position.x);
    chrome.test.assertEq(0, viewport.position.y);

    mockCallback.reset();
    viewport.goToPage(1);
    chrome.test.assertTrue(mockCallback.wasCalled);
    chrome.test.assertEq(0, viewport.position.x);
    chrome.test.assertEq(100, viewport.position.y);

    mockCallback.reset();
    viewport.goToPage(2);
    chrome.test.assertTrue(mockCallback.wasCalled);
    chrome.test.assertEq(0, viewport.position.x);
    chrome.test.assertEq(300, viewport.position.y);

    viewport.setZoom(0.5);
    mockCallback.reset();
    viewport.goToPage(2);
    chrome.test.assertTrue(mockCallback.wasCalled);
    chrome.test.assertEq(0, viewport.position.x);
    chrome.test.assertEq(150, viewport.position.y);
    chrome.test.succeed();
  },

  function testGetPageScreenRect() {
    var mockWindow = new MockWindow(100, 100);
    var mockSizer = new MockSizer();
    var mockCallback = new MockViewportChangedCallback();
    var viewport = new Viewport(mockWindow, mockSizer, mockCallback.callback,
                                function() {}, function() {}, 0);
    var documentDimensions = new MockDocumentDimensions();
    documentDimensions.addPage(100, 100);
    documentDimensions.addPage(200, 200);
    viewport.setDocumentDimensions(documentDimensions);
    viewport.setZoom(1);

    // Test that the rect of the first page is positioned/sized correctly.
    mockWindow.scrollTo(0, 0);
    var rect1 = viewport.getPageScreenRect(0);
    chrome.test.assertEq(Viewport.PAGE_SHADOW.left + 100 / 2, rect1.x);
    chrome.test.assertEq(Viewport.PAGE_SHADOW.top, rect1.y);
    chrome.test.assertEq(100 - Viewport.PAGE_SHADOW.right -
        Viewport.PAGE_SHADOW.left, rect1.width);
    chrome.test.assertEq(100 - Viewport.PAGE_SHADOW.bottom -
        Viewport.PAGE_SHADOW.top, rect1.height);

    // Check that when we scroll, the rect of the first page is updated
    // correctly.
    mockWindow.scrollTo(100, 10);
    var rect2 = viewport.getPageScreenRect(0);
    chrome.test.assertEq(rect1.x - 100, rect2.x);
    chrome.test.assertEq(rect1.y - 10, rect2.y);
    chrome.test.assertEq(rect1.width, rect2.width);
    chrome.test.assertEq(rect1.height, rect2.height);

    // Check the rect of the second page is positioned/sized correctly.
    mockWindow.scrollTo(0, 100);
    rect1 = viewport.getPageScreenRect(1);
    chrome.test.assertEq(Viewport.PAGE_SHADOW.left, rect1.x);
    chrome.test.assertEq(Viewport.PAGE_SHADOW.top, rect1.y);
    chrome.test.assertEq(200 - Viewport.PAGE_SHADOW.right -
        Viewport.PAGE_SHADOW.left, rect1.width);
    chrome.test.assertEq(200 - Viewport.PAGE_SHADOW.bottom -
        Viewport.PAGE_SHADOW.top, rect1.height);
    chrome.test.succeed();
  },

  function testBeforeZoomAfterZoom() {
    var mockWindow = new MockWindow(100, 100);
    var mockSizer = new MockSizer();
    var viewport;
    var afterZoomCalled = false;
    var beforeZoomCalled = false;
    var afterZoom = function() {
        afterZoomCalled = true;
        chrome.test.assertTrue(beforeZoomCalled);
        chrome.test.assertEq(0.5, viewport.zoom);
    };
    var beforeZoom = function() {
        beforeZoomCalled = true;
        chrome.test.assertFalse(afterZoomCalled);
        chrome.test.assertEq(1, viewport.zoom);
    };
    viewport = new Viewport(mockWindow, mockSizer, function() {},
                            beforeZoom, afterZoom, 0);
    viewport.setZoom(0.5);
    chrome.test.succeed();
  }
];

chrome.test.runTests(tests);
