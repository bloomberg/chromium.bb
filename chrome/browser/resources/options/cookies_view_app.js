// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {

  /////////////////////////////////////////////////////////////////////////////
  // CookiesViewApp class:

  /**
   * Encapsulated handling of app cookies and other data page. It
   * derives from the regular CookiesView.
   * @constructor
   */
  function CookiesViewApp(model) {
    options.OptionsPage.call(this, 'app-cookies',
                             loadTimeData.getString('cookiesViewPageTabTitle'),
                             'app-cookies-view-page');
  }

  cr.addSingletonGetter(CookiesViewApp);

  CookiesViewApp.prototype = {
    __proto__: options.CookiesView.prototype,

    isAppContext: function() {
      return true;
    },
  };

  // CookiesViewHandler callbacks.
  CookiesViewApp.onTreeItemAdded = function(args) {
    $('app-cookies-list').addByParentId(args[0], args[1], args[2]);
  };

  CookiesViewApp.onTreeItemRemoved = function(args) {
    $('app-cookies-list').removeByParentId(args[0], args[1], args[2]);
  };

  CookiesViewApp.loadChildren = function(args) {
    $('app-cookies-list').loadChildren(args[0], args[1]);
  };

  // Export
  return {
    CookiesViewApp: CookiesViewApp
  };
});
