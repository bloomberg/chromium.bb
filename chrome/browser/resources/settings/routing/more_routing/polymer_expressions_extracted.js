// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function(scope) {
var PolymerExpressions = scope.PolymerExpressions;
var MoreRouting = scope.MoreRouting = scope.MoreRouting || {};

/**
 *
 */
PolymerExpressions.prototype.route = function route(pathOrName) {
  return MoreRouting.getRoute(pathOrName);
};

/**
 *
 */
PolymerExpressions.prototype.urlFor = function urlFor(pathOrName, params) {
  return MoreRouting.urlFor(pathOrName, params);
};

/**
 * Determines whether the current URL matches a route _and specific params_.
 *
 * @param {string} pathOrName
 * @param {!Object} params
 * @return {boolean} Whether the route found via `pathOrName` is active, and
 *     `params` matches its current params.
 */
PolymerExpressions.prototype.isCurrentUrl = {
  filter: function isCurrentUrl(pathOrName, params) {
    return MoreRouting.getRoute(pathOrName).isCurrentUrl(params);
  },
  extraDeps: function isCurrentUrlExtraDeps(pathOrName, params) {
    // TODO(nevir): Why does this only trigger once if we return a PathObserver?
    return [[MoreRouting.getRoute(pathOrName), 'parts']];
  },
};
})(window);
