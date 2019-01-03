// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Javascript for usb_internals.html, served from chrome://usb-internals/.
 */

(function() {
// Connection to the UsbInternalsPageHandler instance running in the browser
// process.
let usbManagerTest = null;

function refreshDeviceList() {
  usbManagerTest.getTestDevices().then(function(response) {
    const tableBody = $('test-device-list');
    tableBody.innerHTML = '';
    for (const device of response.devices) {
      const row = document.createElement('tr');
      const name = document.createElement('td');
      const serialNumber = document.createElement('td');
      const landingPage = document.createElement('td');
      const remove = document.createElement('td');
      const removeButton = document.createElement('button');
      name.textContent = device.name;
      serialNumber.textContent = device.serialNumber;
      landingPage.textContent = device.landingPage.url;
      removeButton.addEventListener('click', async function() {
        await usbManagerTest.removeDeviceForTesting(device.guid);
        refreshDeviceList();
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
  usbManagerTest
      .addDeviceForTesting(
          $('test-device-name').value, $('test-device-serial').value,
          $('test-device-landing-page').value)
      .then(function(response) {
        if (response.success) {
          refreshDeviceList();
        }

        $('add-test-device-result').textContent = response.message;
        $('add-test-device-result').className =
            response.success ? 'action-success' : 'action-failure';
      });
  event.preventDefault();
}

document.addEventListener('DOMContentLoaded', function() {
  const pageHandler = new mojom.UsbInternalsPageHandlerPtr;
  Mojo.bindInterface(
      mojom.UsbInternalsPageHandler.name, mojo.makeRequest(pageHandler).handle);

  usbManagerTest = new device.mojom.UsbDeviceManagerTestPtr;
  pageHandler.bindTestInterface(mojo.makeRequest(usbManagerTest));

  $('add-test-device-form').addEventListener('submit', addTestDevice);
  refreshDeviceList();
});
})();
