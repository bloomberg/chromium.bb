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
      td[2].textContent = toHex_(device.vendorId);
      td[3].textContent = toHex_(device.productId);
      if (device.manufacturerName) {
        td[4].textContent = decodeString16_(device.manufacturerName.data);
      }
      if (device.productName) {
        td[5].textContent = decodeString16_(device.productName.data);
      }
      if (device.serialNumber) {
        td[6].textContent = decodeString16_(device.serialNumber.data);
      }

      const inspectButton = clone.querySelector('button');
      inspectButton.addEventListener('click', () => {
        switchToTab_(device);
      });

      tableBody.appendChild(clone);
    }
  }

  /**
   * Parses utf16 coded string.
   * @param {!mojoBase.mojom.String16} arr
   * @return {string}
   * @private
   */
  function decodeString16_(arr) {
    return arr.map(ch => String.fromCodePoint(ch)).join('');
  }

  /**
   * Parses the decimal number to hex format.
   * @param {number} num
   * @return {string}
   * @private
   */
  function toHex_(num) {
    return '0x' + num.toString(16).padStart(4, '0').slice(-4).toUpperCase();
  }

  /**
   * Switches to the device's tab, creating one if necessary.
   * @param {!device.mojom.UsbDeviceInfo} device
   * @private
   */
  function switchToTab_(device) {
    const tabId = device.guid;

    if (null == $(tabId)) {
      addTab_(device);
    }
    $(tabId).selected = true;
  }

  /**
   * Adds a tab to display a tree view showing device's detail information.
   * @param {!device.mojom.UsbDeviceInfo} device
   * @private
   */
  function addTab_(device) {
    const tabs = document.querySelector('tabs');

    const tabTemplate = document.querySelector('#tab-template');
    const tabClone = document.importNode(tabTemplate.content, true);

    const tab = tabClone.querySelector('tab');
    if (device.productName) {
      tab.textContent = decodeString16_(device.productName.data);
    } else {
      tab.textContent = toHex_(device.vendorId).slice(2) + ':' +
          toHex_(device.productId).slice(2);
    }
    tab.id = device.guid;

    tabs.appendChild(tabClone);
    cr.ui.decorate('tab', cr.ui.Tab);

    const tabPanels = document.querySelector('tabpanels');

    const tabPanelTemplate = document.querySelector('#tabpanel-template');
    const tabPanelClone = document.importNode(tabPanelTemplate.content, true);

    /**
     * Root of the WebContents tree of current device.
     * @type {cr.ui.Tree|null}
     */
    const treeViewRoot = tabPanelClone.querySelector('#tree-view');
    cr.ui.decorate(treeViewRoot, cr.ui.Tree);
    treeViewRoot.detail = {payload: {}, children: {}};
    // Clear the tree first before populating it with the new content.
    treeViewRoot.innerText = '';
    renderDeviceTree_(device, treeViewRoot);

    tabPanels.appendChild(tabPanelClone);
    cr.ui.decorate('tabpanel', cr.ui.TabPanel);
  }

  /**
   * Renders a customized TreeItem with the given content and class name.
   * @param {string} itemLabel
   * @return {!cr.ui.TreeItem}
   * @private
   */
  function customTreeItem_(itemLabel) {
    const item = new cr.ui.TreeItem({
      label: itemLabel,
      icon: '',
    });
    return item;
  }


  /**
   * Renders a tree to display the device's detail information.
   * @param {!device.mojom.UsbDeviceInfo} device
   * @param {!cr.ui.Tree} root
   * @private
   */
  function renderDeviceTree_(device, root) {
    root.add(customTreeItem_(
        'USB Version: ' + device.usbVersionMajor + '.' +
        device.usbVersionMinor + '.' + device.usbVersionSubminor));

    root.add(customTreeItem_('Class Code: ' + device.classCode));

    root.add(customTreeItem_('Subclass Code: ' + device.subclassCode));

    root.add(customTreeItem_('Protocol Code: ' + device.protocolCode));

    root.add(customTreeItem_('Port Number: ' + device.portNumber));

    root.add(customTreeItem_('Vendor Id: ' + toHex_(device.vendorId)));

    root.add(customTreeItem_('Product Id: ' + toHex_(device.productId)));

    root.add(customTreeItem_(
        'Device Version: ' + device.deviceVersionMajor + ',' +
        device.deviceVersionMinor + ',' + device.deviceVersionSubminor));

    root.add(customTreeItem_(
        'Manufacturer Name: ' + decodeString16_(device.manufacturerName.data)));

    root.add(customTreeItem_(
        'Product Name: ' + decodeString16_(device.productName.data)));

    root.add(customTreeItem_(
        'Serial Number: ' + decodeString16_(device.serialNumber.data)));

    root.add(customTreeItem_(
        'WebUSB Landing Page: ' + device.webusbLandingPage.url));

    root.add(
        customTreeItem_('Active Configuration: ' + device.activeConfiguration));

    const configurationsArray = device.configurations;
    renderConfigurationTreeItem_(configurationsArray, root);
  }

  /**
   * Renders a tree item to display the device's configurations information.
   * @param {!Array<!device.mojom.UsbConfigurationInfo>} configurationsArray
   * @param {!cr.ui.TreeItem} root
   * @private
   */
  function renderConfigurationTreeItem_(configurationsArray, root) {
    for (const configuration of configurationsArray) {
      const configurationItem =
          customTreeItem_('Configuration ' + configuration.configurationValue);

      if (configuration.configurationName) {
        configurationItem.add(customTreeItem_(
            'Configuration Name: ' +
            decodeString16_(configuration.configurationName.data)));
      }

      const interfacesArray = configuration.interfaces;
      renderInterfacesTreeItem_(interfacesArray, configurationItem);

      root.add(configurationItem);
    }
  }

  /**
   * Renders a tree item to display the device's interfaces information.
   * @param {!Array<!device.mojom.UsbInterfaceInfo>} interfacesArray
   * @param {!cr.ui.TreeItem} root
   * @private
   */
  function renderInterfacesTreeItem_(interfacesArray, root) {
    for (const currentInterface of interfacesArray) {
      const interfaceItem =
          customTreeItem_('Interface ' + currentInterface.interfaceNumber);

      const alternatesArray = currentInterface.alternates;
      renderAlternatesTreeItem_(alternatesArray, interfaceItem);

      root.add(interfaceItem);
    }
  }

  /**
   * Renders a tree item to display the device's alternate interfaces
   * information.
   * @param {!Array<!device.mojom.UsbAlternateInterfaceInfo>} alternatesArray
   * @param {!cr.ui.TreeItem} root
   * @private
   */
  function renderAlternatesTreeItem_(alternatesArray, root) {
    for (const alternate of alternatesArray) {
      const alternateItem =
          customTreeItem_('Alternate ' + alternate.alternateSetting);

      alternateItem.add(customTreeItem_('Class Code: ' + alternate.classCode));

      alternateItem.add(
          customTreeItem_('Subclass Code: ' + alternate.subclassCode));

      alternateItem.add(
          customTreeItem_('Protocol Code: ' + alternate.protocolCode));

      if (alternate.interfaceName) {
        alternateItem.add(customTreeItem_(
            'Interface Name: ' +
            decodeString16_(alternate.interfaceName.data)));
      }

      const endpointsArray = alternate.endpoints;
      renderEndpointsTreeItem_(endpointsArray, alternateItem);

      root.add(alternateItem);
    }
  }

  /**
   * Renders a tree item to display the device's endpoints information.
   * @param {!Array<!device.mojom.UsbEndpointInfo>} endpointsArray
   * @param {!cr.ui.TreeItem} root
   * @private
   */
  function renderEndpointsTreeItem_(endpointsArray, root) {
    for (const endpoint of endpointsArray) {
      let itemLabel = 'Endpoint ';

      itemLabel += endpoint.endpointNumber;

      switch (endpoint.direction) {
        case device.mojom.UsbTransferDirection.INBOUND:
          itemLabel += ' (INBOUND)';
          break;
        case device.mojom.UsbTransferDirection.OUTBOUND:
          itemLabel += ' (OUTBOUND)';
          break;
      }

      const endpointItem = customTreeItem_(itemLabel);

      let usbTransferType = '';
      switch (endpoint.type) {
        case device.mojom.UsbTransferType.CONTROL:
          usbTransferType = 'CONTROL';
          break;
        case device.mojom.UsbTransferType.ISOCHRONOUS:
          usbTransferType = 'ISOCHRONOUS';
          break;
        case device.mojom.UsbTransferType.BULK:
          usbTransferType = 'BULK';
          break;
        case device.mojom.UsbTransferType.INTERRUPT:
          usbTransferType = 'INTERRUPT';
          break;
      }

      endpointItem.add(
          customTreeItem_('USB Transfer Type: ' + usbTransferType));

      endpointItem.add(customTreeItem_('Packet Size: ' + endpoint.packetSize));

      root.add(endpointItem);
    }
  }

  return {
    setDevices: setDevices,
  };
});