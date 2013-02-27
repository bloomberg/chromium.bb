// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('network.config', function() {
  var NetworkConfig = cr.ui.define('div');

  NetworkConfig.prototype = {
    __proto__: HTMLDivElement.prototype,
    decorate: function() {
      var params = parseQueryParams(window.location);
      this.networkId_ = params.network;
      this.settingsArea_ = null;
      this.fetchProperties_();
    },

    fetchProperties_: function() {
      chrome.networkingPrivate.getProperties(this.networkId_,
                                             this.updateDom.bind(this));
    },

    updateDom: function(properties) {
      var div = document.createElement('div');
      var label = document.createElement('h4');
      label.textContent = 'User Settings';
      div.appendChild(label);
      var area = document.createElement('textarea');
      var str = JSON.stringify(properties, undefined, 2);
      area.value = str;
      div.appendChild(area);

      this.appendChild(div);
      this.settingsArea_ = area;
    },

    applyUserSettings: function() {
      chrome.networkingPrivate.setProperties(
          this.networkId_,
          JSON.parse(this.settingsArea_.value));
    },

    get networkId() {
      return this.networkId_;
    }
  };

  return {
    NetworkConfig: NetworkConfig
  };
});
