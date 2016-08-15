// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.exportPath('settings');
assert(!settings.defaultResourceLoaded,
       'settings.js run twice. You probably have an invalid import.');
/** Global defined when the main Settings script runs. */
settings.defaultResourceLoaded = true;

/**
 * @fileoverview
 * 'cr-settings' is the main MD-Settings element, combining the UI and models.
 *
 * Example:
 *
 *    <cr-settings></cr-settings>
 */
Polymer({
  is: 'cr-settings',

  /** @override */
  created: function() {
    settings.initializeRouteFromUrl();
  },

  /** @override */
  ready: function() {
    this.$.ui.directionDelegate = new settings.DirectionDelegateImpl;
  },
});
