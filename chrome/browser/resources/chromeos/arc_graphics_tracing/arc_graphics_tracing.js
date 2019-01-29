// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview ARC Graphics Tracing UI root element.
 */

cr.define('cr.ArcGraphicsTracing', function() {
  return {
    /**
     * Initializes internal structures.
     */
    initialize: function() {
      var startDelay = $('arc-graphics-tracing-start-delay');
      var duration = $('arc-graphics-tracing-duration');
      $('arc-graphics-tracing-start')
          .addEventListener('click', function(event) {
            chrome.send(
                'startTracing',
                [Number(startDelay.value), Number(duration.value)]);
          }, false);
    },

    setModel: function(model) {
      setGraphicBuffersModel(model);
    }
  };
});

/**
 * Initializes UI.
 */
window.onload = function() {
  cr.ArcGraphicsTracing.initialize();
};
