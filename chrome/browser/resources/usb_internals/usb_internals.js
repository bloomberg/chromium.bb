// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Javascript for usb_internals.html, served from chrome://usb-internals/.
 */
cr.define('usb_internals', function() {
  class UsbInternals {
    constructor() {
      // Connection to the UsbInternalsPageHandler instance running in the
      // browser process.
      this.usbManagerTest = null;

      const pageHandler = new mojom.UsbInternalsPageHandlerPtr;
      Mojo.bindInterface(
          mojom.UsbInternalsPageHandler.name,
          mojo.makeRequest(pageHandler).handle);

      this.usbManagerTest = new device.mojom.UsbDeviceManagerTestPtr;
      pageHandler.bindTestInterface(mojo.makeRequest(this.usbManagerTest));

      cr.ui.decorate('tabbox', cr.ui.TabBox);
      $('add-test-device-form').addEventListener('submit', (event) => {
        this.addTestDevice(event);
      });
      this.refreshDeviceList();
    }

    async refreshDeviceList() {
      const response = await this.usbManagerTest.getTestDevices();

      const tableBody = $('test-device-list');
      tableBody.innerHTML = '';

      const rowTemplate = document.querySelector('#test-device-row');
      const td = rowTemplate.content.querySelectorAll('td');

      for (const device of response.devices) {
        td[0].textContent = device.name;
        td[1].textContent = device.serialNumber;
        td[2].textContent = device.landingPage.url;

        const clone = document.importNode(rowTemplate.content, true);

        const removeButton = clone.querySelector('button');
        removeButton.addEventListener('click', async () => {
          await this.usbManagerTest.removeDeviceForTesting(device.guid);
          this.refreshDeviceList();
        });

        tableBody.appendChild(clone);
      }
    }

    async addTestDevice(event) {
      event.preventDefault();

      const response = await this.usbManagerTest.addDeviceForTesting(
          $('test-device-name').value, $('test-device-serial').value,
          $('test-device-landing-page').value);
      if (response.success) {
        this.refreshDeviceList();
      }

      $('add-test-device-result').textContent = response.message;
      $('add-test-device-result').className =
          response.success ? 'action-success' : 'action-failure';
    }
  }

  return {
    UsbInternals,
  };
});

document.addEventListener('DOMContentLoaded', () => {
  new usb_internals.UsbInternals();
});