// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

///////////////////////////////////////////////////////////////////////////////
// SearchEngineManager class:

/**
 * Encapsulated handling of search engine management page.
 * @constructor
 */
function SearchEngineManager() {
  this.activeNavTab = null;
  OptionsPage.call(this, 'searchEngines',
                   templateData.searchEngineManagerPage,
                   'searchEngineManagerPage');
}

cr.addSingletonGetter(SearchEngineManager);

SearchEngineManager.prototype = {
  __proto__: OptionsPage.prototype,

  initializePage: function() {
    OptionsPage.prototype.initializePage.call(this);

    // TODO(stuartmorgan): Add initialization here.
  },
};
