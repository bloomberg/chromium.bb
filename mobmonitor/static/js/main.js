// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';


$(document).ready(function() {
  // Setup the health status widget.
  $('#healthStatusDisplay').healthDisplay();
  $('#healthStatusDisplay').healthDisplay('refreshHealthDisplay');

  setInterval(function() {
    $('#healthStatusDisplay').healthDisplay('refreshHealthDisplay');
  }, 3000);

  // Setup the action repair dialog popup.
  // TODO(msartori): Implement crbug.com/520749.
  $(document).on('click', '.header-service-status-button', function() {
    // Get the service that this button corresponds to.
    var service = $(this).closest('.health-container').attr('id');
    if (service.indexOf(SERVICE_CONTAINER_PREFIX) === 0) {
      service = service.replace(SERVICE_CONTAINER_PREFIX, '');
    }

    // Do not launch dialog if this service does not need repair.
    if (!$('#healthStatusDisplay').healthDisplay('needsRepair', service)) {
      return;
    }

    // Get the actions for this service.
    var actions = $('#healthStatusDisplay').healthDisplay(
        'getServiceActions', service);

    // Create and launch the action repair dialog.
    var dialog = new ActionRepairDialog(service, actions);
    dialog.submitHandler = function(service, action, args, kwargs) {
      rpcRepairService(service, action, args, kwargs, function(response) {
        $('#healthStatusDisplay').healthDisplay('markStale', response.service);
      });
    };
    dialog.open();
  });
});
