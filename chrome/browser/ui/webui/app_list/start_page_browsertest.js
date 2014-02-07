// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * TestFixture for kiosk app settings WebUI testing.
 * @extends {testing.Test}
 * @constructor
 **/
function AppListStartPageWebUITest() {}

AppListStartPageWebUITest.prototype = {
  __proto__: testing.Test.prototype,

  /**
   * Browser to app launcher start page.
   */
  browsePreload: 'chrome://app-list/',

  /**
   * Recommend apps data.
   * @private
   */
  recommendedApps_: [
    {
      'appId': 'app_id_1',
      'textTitle': 'app 1',
      'iconUrl': 'icon_url_1'
    },
    {
      'appId': 'app_id_2',
      'textTitle': 'app 2',
      'iconUrl': 'icon_url_2'
    },
  ],

  /** @override */
  preLoad: function() {
    this.makeAndRegisterMockHandler(['initialize', 'launchApp']);
    this.mockHandler.stubs().initialize().will(callFunction(function() {
      appList.startPage.setRecommendedApps(this.recommendedApps_);
    }.bind(this)));
    this.mockHandler.stubs().launchApp(ANYTHING);
  }
};

TEST_F('AppListStartPageWebUITest', 'Basic', function() {
  assertEquals(this.browsePreload, document.location.href);

  var recommendedApp = $('start-page').querySelector('.recommended-apps');
  assertEquals(this.recommendedApps_.length, recommendedApp.childElementCount);
  for (var i = 0; i < recommendedApp.childElementCount; ++i) {
    assertEquals(this.recommendedApps_[i].appId,
                 recommendedApp.children[i].appId);
  }
});

TEST_F('AppListStartPageWebUITest', 'ClickToLaunch', function() {
  var recommendedApp = $('start-page').querySelector('.recommended-apps');
  for (var i = 0; i < recommendedApp.childElementCount; ++i) {
    this.mockHandler.expects(once()).launchApp(
        [this.recommendedApps_[i].appId]);
    cr.dispatchSimpleEvent(recommendedApp.children[i], 'click');
  }
});
