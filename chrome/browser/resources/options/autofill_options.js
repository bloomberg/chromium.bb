// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {

  var OptionsPage = options.OptionsPage;

  /////////////////////////////////////////////////////////////////////////////
  // AutoFillOptions class:

  /**
   * Encapsulated handling of AutoFill options page.
   * @constructor
   */
  function AutoFillOptions() {
    this.activeNavTab = null;
    OptionsPage.call(this, 'autoFillOptions',
                     templateData.autoFillOptionsTitle,
                     'autoFillOptionsPage');
  }

  cr.addSingletonGetter(AutoFillOptions);

  AutoFillOptions.prototype = {
    __proto__: OptionsPage.prototype,

    initializePage: function() {
      OptionsPage.prototype.initializePage.call(this);

      // TODO(jhawkins): Add initialization here.
    }
  };

  // Export
  return {
    AutoFillOptions: AutoFillOptions
  };

});

