// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Javascript for DevicesPage, served from
 *     chrome://usb-internals/.
 */

cr.define('devices_page', function() {
  /**
   * Sets the device collection for the page's device table.
   * @param {!Array<!device.mojom.UsbDeviceInfo>} devices
   */
  function setDevices(devices) {
    const tableBody = $('device-list');
    tableBody.innerHTML = '';

    const rowTemplate = document.querySelector('#device-row');

    for (const device of devices) {
      const clone = document.importNode(rowTemplate.content, true);

      const td = clone.querySelectorAll('td');

      td[0].textContent = device.busNumber;
      td[1].textContent = device.portNumber;
      td[2].textContent = toHex(device.vendorId);
      td[3].textContent = toHex(device.productId);
      if (device.manufacturerName) {
        td[4].textContent = decodeString16(device.manufacturerName.data);
      }
      if (device.productName) {
        td[5].textContent = decodeString16(device.productName.data);
      }
      if (device.serialNumber) {
        td[6].textContent = decodeString16(device.serialNumber.data);
      }

      tableBody.appendChild(clone);
    }
  }

  /**
   * Parses utf16 coded string.
   * @param {!mojoBase.mojom.String16} arr
   * @return {string}
   */
  function decodeString16(arr) {
    return arr.map(ch => String.fromCodePoint(ch)).join('');
  }

  /**
   * Parses the decimal number to hex format.
   * @param {number} num
   * @return {string}
   */
  function toHex(num) {
    return '0x' + num.toString(16).padStart(4, '0').slice(-4).toUpperCase();
  }

  return {
    setDevices: setDevices,
  };
});
