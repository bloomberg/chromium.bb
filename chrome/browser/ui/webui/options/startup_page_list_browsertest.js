// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Fixture for startup pages WebUI tests.
 * @extends {testing.Test}
 * @constructor
 */
function StartupPageListWebUITest() {}

StartupPageListWebUITest.prototype = {
  __proto__: testing.Test.prototype,

  /**
   * Browse to the options page & call our preLoad().
   * @override
   */
  browsePreload: 'chrome://settings-frame/startup',

  /** @override */
  setUp: function() {
    StartupOverlay.updateStartupPages(this.fakeStartupList);
    // 1 item for entering data, 1+ from |this.fakeStartupList|.
    assertGE(this.getList().items.length, 2);
  },

  /**
   * Returns the list to be tested.
   * @return {Element} The start-up pages list.
   * @protected
   */
  getList: function() {
    return $('startupPagesList');
  },

  /**
   * Register a mock handler to ensure expectations are met and options pages
   * behave correctly.
   * @override
   */
  preLoad: function() {
    this.makeAndRegisterMockHandler(['addStartupPage',
                                     'dragDropStartupPage']);
  },

  /**
   * A fake list of startup pages to send to the overlay.
   * @protected
   */
  fakeStartupList: [
    {
      title: 'Yahoo!',
      url: 'http://yahoo.com',
      tooltip: 'Yahoo! homepage',
      modelIndex: 0
    },
    {
      title: 'Facebook',
      url: 'http://facebook.com',
      tooltip: 'Facebook :: Sign In',
      modelIndex: 1
    }
  ],
};

(function() {

/**
 * A mock data transfer object for drag/drop events.
 * @constructor
 */
function MockDataTransfer() {
  /**
   * The data this dataTransfer object knows about.
   * @type {!Object.<string, string>}
   * @private
   */
  this.data_ = {};
}

/**
 * Installs a lazily created MockDataTransfer on event#dataTransfer.
 * @param {!Event} event An event to modify.
 */
MockDataTransfer.install = function(event) {
  event.__defineGetter__('dataTransfer', function() {
    event.dataTransfer_ = event.dataTransfer_ || new MockDataTransfer;
    return event.dataTransfer_;
  });
};

MockDataTransfer.prototype = {
  /**
   * The URL data in this mock drop event.
   * @param {string} type The text of data being set.
   * @param {*} val The data to set. Will be stringified.
   */
  setData: function(type, val) {
    this.data_[type] = String(val);
  },

  /**
   * Gets data associated with this fake data transfer.
   * @param {string} type The type of data to get.
   * @return {string} The requested type of data or '' if not set.
   */
  getData: function(type) {
    return this.data_[type] || '';
  },
};

/**
 * Creates a fake bubbling, cancelable mouse event with a mock data transfer
 * installed.
 * @param {string} type A type of mouse event (e.g. 'drop').
 * @return {!Event} A fake mouse event.
 */
function createMouseEvent(type) {
  var event = new MouseEvent(type, {bubbles: true, cancelable: true});
  MockDataTransfer.install(event);
  return event;
}

TEST_F('StartupPageListWebUITest', 'testDropFromOutsideSource', function() {
  /** @const */ var NEW_PAGE = 'http://google.com';

  var mockDropEvent = createMouseEvent('drop');
  mockDropEvent.dataTransfer.setData('url', NEW_PAGE);

  this.mockHandler.expects(once()).addStartupPage([NEW_PAGE, 0]);

  this.getList().items[0].dispatchEvent(mockDropEvent);

  expectTrue(mockDropEvent.defaultPrevented);
});

TEST_F('StartupPageListWebUITest', 'testDropToReorder', function() {
  // TODO(dbeam): mock4js doesn't handle complex arguments well. Fix this.
  this.mockHandler.expects(once()).dragDropStartupPage([0, [1].join()]);

  this.getList().selectionModel.selectedIndex = 1;
  expectEquals(1, this.getList().selectionModel.selectedIndexes.length);

  this.getList().items[0].dispatchEvent(createMouseEvent('drop'));
});

}());
