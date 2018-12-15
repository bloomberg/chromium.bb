// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Namespace for the Camera app.
 */
var cca = cca || {};

/**
 * Namespace for views.
 */
cca.views = cca.views || {};

/**
 * Creates the gridsettings-view controller.
 * @extends {cca.views.View}
 * @constructor
 */
cca.views.GridSettings = function() {
  cca.views.View.call(this, '#gridsettings', true, true);

  // End of properties, seal the object.
  Object.seal(this);

  cca.views.settings.util.setupMenu(this, {});
};

cca.views.GridSettings.prototype = {
  __proto__: cca.views.View.prototype,
};
