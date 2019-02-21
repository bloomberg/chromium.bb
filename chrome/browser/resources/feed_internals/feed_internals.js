// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

// Reference to the backend.
let pageHandler = null;

(function() {
// Get and display general properties.
function updatePageWithProperties() {
  pageHandler.getGeneralProperties().then(response => {
    const properties = response.properties;
    $('isFeedEnabled').textContent = properties.isFeedEnabled;
  });
}

// Get and display user classifier properties.
function updatePageWithUserClass() {
  pageHandler.getUserClassifierProperties().then(response => {
    const properties = response.properties;
    $('userClassDescription').textContent = properties.userClassDescription;
    $('avgHoursBetweenViews').textContent = properties.avgHoursBetweenViews;
    $('avgHoursBetweenUses').textContent = properties.avgHoursBetweenUses;
  });
}

// Hook up buttons to event listeners.
function setupEventListeners() {
  $('btnClearUserClassification').addEventListener('click', function() {
    pageHandler.clearUserClassifierProperties();
    updatePageWithUserClass();
  });
}

document.addEventListener('DOMContentLoaded', function() {
  // Setup backend mojo.
  pageHandler = feedInternals.mojom.PageHandler.getProxy();

  updatePageWithProperties();
  updatePageWithUserClass();
  setupEventListeners();
});
})();
