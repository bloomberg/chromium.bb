// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('downloadInternals', function() {
  'use strict';

  /** @type {!downloadInternals.DownloadInternalsBrowserProxy} */
  var browserProxy =
      downloadInternals.DownloadInternalsBrowserProxyImpl.getInstance();

  function onServiceStatusChanged(state) {
    $('service-state').textContent = state.serviceState;
    $('service-status-model').textContent = state.modelStatus;
    $('service-status-driver').textContent = state.driverStatus;
    $('service-status-file').textContent = state.fileMonitorStatus;
  }

  function initialize() {
    // Register all event listeners.
    cr.addWebUIListener('service-status-changed', onServiceStatusChanged);

    // Kick off requests for the current system state.
    browserProxy.getServiceStatus().then(onServiceStatusChanged);
  }

  return {
    initialize: initialize,
  };
});

document.addEventListener('DOMContentLoaded', downloadInternals.initialize);