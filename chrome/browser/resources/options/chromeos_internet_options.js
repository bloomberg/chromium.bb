// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {

  var OptionsPage = options.OptionsPage;

  /////////////////////////////////////////////////////////////////////////////
  // InternetOptions class:

  /**
   * Encapsulated handling of ChromeOS internet options page.
   * @constructor
   */
  function InternetOptions() {
    OptionsPage.call(this, 'internet', localStrings.getString('internetPage'),
        'internetPage');
  }

  cr.addSingletonGetter(InternetOptions);

  // Inherit InternetOptions from OptionsPage.
  InternetOptions.prototype = {
    __proto__: OptionsPage.prototype,

    /**
     * Initializes InternetOptions page.
     * Calls base class implementation to starts preference initialization.
     */
    initializePage: function() {
      // Call base class implementation to starts preference initialization.
      OptionsPage.prototype.initializePage.call(this);

      options.internet.NetworkElement.decorate($('wiredList'));
      $('wiredList').load(templateData.wiredList);
      options.internet.NetworkElement.decorate($('wirelessList'));
      $('wirelessList').load(templateData.wirelessList);
      options.internet.NetworkElement.decorate($('rememberedList'));
      $('rememberedList').load(templateData.rememberedList);
    }
  };

  //
  //Chrome callbacks
  //
  InternetOptions.refreshNetworkData = function (data) {
    $('wiredList').load(data.wiredList);
    $('wirelessList').load(data.wirelessList);
    $('rememberedList').load(data.rememberedList);
  };

  // Export
  return {
    InternetOptions: InternetOptions
  };

});

