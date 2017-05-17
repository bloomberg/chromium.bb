// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Javascript for usb_internals.html, served from chrome://usb-internals/.
 */

(function() {
  // Connection to the UsbInternalsPageHandler instance running in the browser
  // process.
  let pageHandler = null;

  function refreshDeviceList() {
    pageHandler.getTestDevices().then(function(response) {
      let tableBody = $('test-device-list');
      tableBody.innerHTML = '';
      for (let device of response.devices) {
        let row = document.createElement('tr');
        let name = document.createElement('td');
        let serialNumber = document.createElement('td');
        let landingPage = document.createElement('td');
        let remove = document.createElement('td');
        let removeButton = document.createElement('button');
        name.textContent = device.name;
        serialNumber.textContent = device.serial_number;
        landingPage.textContent = device.landing_page.url;
        removeButton.addEventListener('click', function() {
          pageHandler.removeDeviceForTesting(device.guid)
              .then(refreshDeviceList);
        });
        removeButton.textContent = 'Remove';
        row.appendChild(name);
        row.appendChild(serialNumber);
        row.appendChild(landingPage);
        remove.appendChild(removeButton);
        row.appendChild(remove);
        tableBody.appendChild(row);
      }
    });
  }

  function addTestDevice(event) {
    pageHandler.addDeviceForTesting(
        $('test-device-name').value,
        $('test-device-serial').value,
        $('test-device-landing-page').value).then(function(response) {
      if (response.success)
        refreshDeviceList();
      $('add-test-device-result').textContent = response.message;
      $('add-test-device-result').className =
          response.success ? 'action-success' : 'action-failure';
    });
    event.preventDefault();
  }

  function initializeProxies() {
    return importModules([
      'chrome/browser/ui/webui/usb_internals/usb_internals.mojom',
      'content/public/renderer/frame_interfaces',
    ]).then(function(modules) {
      let mojom = modules[0];
      let frameInterfaces = modules[1];

      pageHandler = new mojom.UsbInternalsPageHandlerPtr(
          frameInterfaces.getInterface(mojom.UsbInternalsPageHandler.name));
    });
  }

  document.addEventListener('DOMContentLoaded', function() {
    initializeProxies().then(function() {
      $('add-test-device-form').addEventListener('submit', addTestDevice);
      refreshDeviceList();
    });
  });
})();
