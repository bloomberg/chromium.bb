// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Javascript for suggestions_internals.html, served from
 * chrome://suggestions-internals/. This is used to debug suggestions ranking.
 * When loaded, the page will show the current set of suggestions, along with a
 * large set of information (e.g. all the signals that were taken into
 * consideration for deciding which pages were selected to be shown to the user)
 * that will aid in debugging and optimizing the algorithms.
 */
cr.define('suggestionsInternals', function() {
  'use strict';

  /**
   * Register our event handlers.
   */
  function initialize() {
    // TODO(macourteau): implement this.
  }

  return {
    initialize: initialize,
  };
});

document.addEventListener('DOMContentLoaded', suggestionsInternals.initialize);
