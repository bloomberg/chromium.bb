// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function(scope) {
var MoreRouting = scope.MoreRouting = scope.MoreRouting || {};
MoreRouting.HashDriver = HashDriver;

/**
 * TODO(nevir): Docs.
 */
function HashDriver() {
  MoreRouting.Driver.apply(this, arguments);
  this._bindEvents();
  this._read();
}
HashDriver.prototype = Object.create(MoreRouting.Driver.prototype);

// By default, we prefer hashbang; but you can prefix with any string you wish.
HashDriver.prototype.prefix = '!/';

HashDriver.prototype.urlForParts = function urlForParts(parts) {
  return '#' + MoreRouting.Driver.prototype.urlForParts.call(this, parts);
};

HashDriver.prototype.navigateToUrl = function navigateToUrl(url) {
  window.location.hash = url;
};

HashDriver.prototype._bindEvents = function _bindEvents() {
  window.addEventListener('hashchange', this._read.bind(this));
};

HashDriver.prototype._read = function _read() {
  this.setCurrentPath(window.location.hash.substr(1) || this.prefix);
};

})(window);
