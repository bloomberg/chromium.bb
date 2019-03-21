// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Javascript for DescriptorPanel UI, served from
 *     chrome://usb-internals/.
 */

cr.define('descriptor_panel', function() {
  // Standard USB requests and descriptor types:
  const GET_DESCRIPTOR_REQUEST = 0x06;
  const DEVICE_DESCRIPTOR_TYPE = 0x01;
  const CONFIGURATION_DESCRIPTOR_TYPE = 0x02;
  const STRING_DESCRIPTOR_TYPE = 0x03;
  const INTERFACE_DESCRIPTOR_TYPE = 0x04;
  const ENDPOINT_DESCRIPTOR_TYPE = 0x05;

  const DEVICE_DESCRIPTOR_LENGTH = 18;
  const CONFIGURATION_DESCRIPTOR_LENGTH = 9;
  const INTERFACE_DESCRIPTOR_LENGTH = 9;
  const ENDPOINT_DESCRIPTOR_LENGTH = 7;

  const CONTROL_TRANSFER_TIMEOUT_MS = 2000;  // 2 seconds

  class DescriptorPanel {
    /**
     * @param {!device.mojom.UsbDeviceInterface} usbDeviceProxy
     * @param {HTMLElement} rootElement
     */
    constructor(usbDeviceProxy, rootElement) {
      /** @private {!device.mojom.UsbDeviceInterface} */
      this.usbDeviceProxy_ = usbDeviceProxy;

      /** @private {!HTMLElement} */
      this.rootElement_ = rootElement;

      const descriptorPanelTemplate =
          document.querySelector('#descriptor-panel-template');
      const descriptorPanelClone =
          document.importNode(descriptorPanelTemplate.content, true);

      /** @private {!HTMLElement} */
      this.rawDataTreeRoot_ =
          descriptorPanelClone.querySelector('#raw-data-tree-view');
      /** @private {!HTMLElement} */
      this.rawDataElement_ = descriptorPanelClone.querySelector('#raw-data');

      this.clearView();

      cr.ui.decorate(this.rawDataTreeRoot_, cr.ui.Tree);
      this.rawDataTreeRoot_.detail = {payload: {}, children: {}};

      this.rootElement_.appendChild(descriptorPanelClone);
    }

    /**
     * Clears the data first before populating it with the new content.
     */
    clearView() {
      this.rootElement_.querySelectorAll('error').forEach(el => el.remove());
      this.rawDataTreeRoot_.innerText = '';
      this.rawDataElement_.textContent = '';
    }

    /**
     * Adds function for mapping between two views.
     * @private
     */
    addMappingAction_() {
      // Highlights the byte(s) that hovered in the tree.
      this.rawDataTreeRoot_.querySelectorAll('.tree-row').forEach((el) => {
        const classList = el.classList;
        // classList[0] is 'tree-row'. classList[1] of tree item for fields
        // starts with 'field-offset-', and classList[1] of tree item for
        // descriptors (ie. endpoint descriptor) is descriptor type and index.
        const fieldOffsetOrDescriptorClass = classList[1];
        assert(
            fieldOffsetOrDescriptorClass.startsWith('field-offset-') ||
            fieldOffsetOrDescriptorClass.startsWith('descriptor-'));

        el.addEventListener('pointerenter', (event) => {
          this.rawDataElement_
              .querySelectorAll(`.${fieldOffsetOrDescriptorClass}`)
              .forEach((el) => el.classList.add('hovered-field'));
          event.stopPropagation();
        });

        el.addEventListener('pointerleave', () => {
          this.rawDataElement_
              .querySelectorAll(`.${fieldOffsetOrDescriptorClass}`)
              .forEach((el) => el.classList.remove('hovered-field'));
        });

        el.addEventListener('click', (event) => {
          if (event.target.className != 'expand-icon') {
            // Clears all the selected elements before select another.
            this.rawDataElement_.querySelectorAll('#raw-data span')
                .forEach((el) => el.classList.remove('selected-field'));

            this.rawDataElement_
                .querySelectorAll(`.${fieldOffsetOrDescriptorClass}`)
                .forEach((el) => el.classList.add('selected-field'));
          }
        });
      });

      // Selects the tree item that displays the byte hovered in the raw view.
      const rawDataByteElements = this.rawDataElement_.querySelectorAll('span');
      rawDataByteElements.forEach((el) => {
        const classList = el.classList;

        const fieldOffsetClass = classList[0];
        assert(fieldOffsetClass.startsWith('field-offset-'));

        el.addEventListener('pointerenter', () => {
          this.rawDataElement_.querySelectorAll(`.${fieldOffsetClass}`)
              .forEach((el) => el.classList.add('hovered-field'));
          const el =
              this.rawDataTreeRoot_.querySelector(`.${fieldOffsetClass}`);
          if (el) {
            el.classList.add('hover');
          }
        });

        el.addEventListener('pointerleave', () => {
          this.rawDataElement_.querySelectorAll(`.${fieldOffsetClass}`)
              .forEach((el) => el.classList.remove('hovered-field'));
          const el =
              this.rawDataTreeRoot_.querySelector(`.${fieldOffsetClass}`);
          if (el) {
            el.classList.remove('hover');
          }
        });

        el.addEventListener('click', () => {
          const el =
              this.rawDataTreeRoot_.querySelector(`.${fieldOffsetClass}`);
          if (el) {
            el.click();
          }
        });
      });
    }

    /**
     * Renders an element to display the raw data in hex, byte by byte, and
     * keeps every row no more than 16 bytes.
     * @param {!Uint8Array} rawData
     * @private
     */
    renderRawDataBytes_(rawData) {
      const rawDataByteTemplate = document.querySelector('#raw-data-byte');
      for (const [i, value] of rawData.entries()) {
        const rawDataByteClone =
            document.importNode(rawDataByteTemplate.content, true);
        const rawDataByteElement = rawDataByteClone.querySelector('span');

        rawDataByteElement.textContent =
            value.toString(16).padStart(2, '0').slice(-2).toUpperCase();
        this.rawDataElement_.appendChild(rawDataByteElement);
      }
    }

    /**
     * Renders a tree view to display the raw data in readable text.
     * @param {!cr.ui.Tree|!cr.ui.TreeItem} root
     * @param {!Array<Object>} fields
     * @param {!Uint8Array} rawData
     * @param {number} offset
     * @param {string=} opt_parentClassName
     * @return {number}
     * @private
     */
    renderRawDataTree_(root, fields, rawData, offset, opt_parentClassName) {
      const rawDataByteElements = this.rawDataElement_.querySelectorAll('span');

      for (const field of fields) {
        const className = `field-offset-${offset}`;

        const item = customTreeItem(
            `${field.label}: ${field.formatter(rawData, offset)}`, className);

        for (let i = 0; i < field.size; i++) {
          rawDataByteElements[offset + i].classList.add(className);
          if (opt_parentClassName) {
            rawDataByteElements[offset + i].classList.add(opt_parentClassName);
          }
        }

        root.add(item);

        offset += field.size;
      }

      return offset;
    }

    /**
     * Checks the if the status of a descriptor read indicates success.
     * @param {number} status
     * @param {string} defaultMessage
     * @return {boolean}
     * @private
     */
    checkDescriptorGetSuccess_(status, defaultMessage) {
      let failReason = '';
      switch (status) {
        case device.mojom.UsbTransferStatus.COMPLETED:
          return;
        case device.mojom.UsbTransferStatus.SHORT_PACKET:
          this.showError_('Descriptor is too short.');
          return;
        case device.mojom.UsbTransferStatus.BABBLE:
          this.showError_('Descriptor is too long.');
          return;
        case device.mojom.UsbTransferStatus.TRANSFER_ERROR:
          failReason = 'Transfer Error';
          break;
        case device.mojom.UsbTransferStatus.TIMEOUT:
          failReason = 'Timeout';
          break;
        case device.mojom.UsbTransferStatus.CANCELLED:
          failReason = 'Transfer was cancelled';
          break;
        case device.mojom.UsbTransferStatus.STALLED:
          failReason = 'Transfer Error';
          break;
        case device.mojom.UsbTransferStatus.DISCONNECT:
          failReason = 'Transfer stalled';
          break;
        case device.mojom.UsbTransferStatus.PERMISSION_DENIED:
          failReason = 'Permission denied';
          break;
      }
      this.showError_(`${defaultMessage} (Reason: ${failReason})`);
      throw new Error(`${defaultMessage} (${failReason})`);
    }

    /**
     * Shows an error message if error occurs in getting or rendering
     * descriptors.
     * @param {string} message
     * @private
     */
    showError_(message) {
      const errorTemplate = document.querySelector('#error');

      const clone = document.importNode(errorTemplate.content, true);

      const errorText = clone.querySelector('error');
      errorText.textContent = message;

      this.rootElement_.prepend(clone);
    }

    /**
     * Gets device descriptor of current device.
     * @return {!Uint8Array}
     * @private
     */
    async getDeviceDescriptor_() {
      /** @type {device.mojom.UsbControlTransferParams} */
      const usbControlTransferParams = {};
      usbControlTransferParams.type =
          device.mojom.UsbControlTransferType.STANDARD;
      usbControlTransferParams.recipient =
          device.mojom.UsbControlTransferRecipient.DEVICE;
      usbControlTransferParams.request = GET_DESCRIPTOR_REQUEST;
      usbControlTransferParams.value = (DEVICE_DESCRIPTOR_TYPE << 8);
      usbControlTransferParams.index = 0;
      const length = DEVICE_DESCRIPTOR_LENGTH;
      const timeout = CONTROL_TRANSFER_TIMEOUT_MS;

      await this.usbDeviceProxy_.open();

      const response = await this.usbDeviceProxy_.controlTransferIn(
          usbControlTransferParams, length, timeout);

      this.checkDescriptorGetSuccess_(
          response.status, 'Failed to read the device descriptor');

      return new Uint8Array(response.data);
    }

    /**
     * Renders a view to display device descriptor hex data in both tree view
     * and raw form.
     */
    async renderDeviceDescriptor() {
      const rawData = await this.getDeviceDescriptor_();

      this.renderRawDataBytes_(rawData);

      const fields = [
        {
          label: 'Length',
          size: 1,
          formatter: formatByte,
        },
        {
          label: 'Descriptor Type',
          size: 1,
          formatter: formatDescriptorType,
        },
        {
          label: 'USB Version',
          size: 2,
          formatter: formatUsbVersion,
        },
        {
          label: 'Class Code',
          size: 1,
          formatter: formatByte,
        },
        {
          label: 'Subclass Code',
          size: 1,
          formatter: formatByte,
        },
        {
          label: 'Protocol Code',
          size: 1,
          formatter: formatByte,
        },
        {
          label: 'Control Pipe Maximum Packet Size',
          size: 1,
          formatter: formatByte,
        },
        {
          label: 'Vendor ID',
          size: 2,
          formatter: formatTwoBytesToHex,
        },
        {
          label: 'Product ID',
          size: 2,
          formatter: formatTwoBytesToHex,
        },
        {
          label: 'Device Version',
          size: 2,
          formatter: formatUsbVersion,
        },
        {
          label: 'Manufacturer String Index',
          size: 1,
          formatter: formatByte,
        },
        {
          label: 'Product String Index',
          size: 1,
          formatter: formatByte,
        },
        {
          label: 'Serial Number Index',
          size: 1,
          formatter: formatByte,
        },
        {
          label: 'Number of Configurations',
          size: 1,
          formatter: formatByte,
        },
      ];

      let offset = 0;
      offset = this.renderRawDataTree_(
          this.rawDataTreeRoot_, fields, rawData, offset);

      assert(
          offset === DEVICE_DESCRIPTOR_LENGTH,
          'Device Descriptor Rendering Error');

      this.addMappingAction_();
    }

    /**
     * Gets configuration descriptor of current device.
     * @return {!Uint8Array}
     * @private
     */
    async getConfigurationDescriptor_() {
      /** @type {device.mojom.UsbControlTransferParams} */
      const usbControlTransferParams = {};
      usbControlTransferParams.type =
          device.mojom.UsbControlTransferType.STANDARD;
      usbControlTransferParams.recipient =
          device.mojom.UsbControlTransferRecipient.DEVICE;
      usbControlTransferParams.request = GET_DESCRIPTOR_REQUEST;
      usbControlTransferParams.value = (CONFIGURATION_DESCRIPTOR_TYPE << 8);
      usbControlTransferParams.index = 0;
      let length = CONFIGURATION_DESCRIPTOR_LENGTH;
      const timeout = CONTROL_TRANSFER_TIMEOUT_MS;

      await this.usbDeviceProxy_.open();

      let response = await this.usbDeviceProxy_.controlTransferIn(
          usbControlTransferParams, length, timeout);

      this.checkDescriptorGetSuccess_(
          response.status,
          'Failed to read the device configuration descriptor to determine ' +
              'the total descriptor length.');

      const data = new DataView(new Uint8Array(response.data).buffer);
      length = data.getUint16(2, true);
      // Re-gets the data use the full length.
      response = await this.usbDeviceProxy_.controlTransferIn(
          usbControlTransferParams, length, timeout);

      this.checkDescriptorGetSuccess_(
          response.status,
          'Failed to read the complete configuration descriptor');

      return new Uint8Array(response.data);
    }

    /**
     * Renders a view to display configuration descriptor hex data in both tree
     * view and raw form.
     */
    async renderConfigurationDescriptor() {
      const rawData = await this.getConfigurationDescriptor_();

      this.renderRawDataBytes_(rawData);

      const fields = [
        {
          label: 'Length',
          size: 1,
          formatter: formatByte,
        },
        {
          label: 'Descriptor Type',
          size: 1,
          formatter: formatDescriptorType,
        },
        {
          label: 'Total Length',
          size: 2,
          formatter: formatShort,
        },
        {
          label: 'Number of Interfaces',
          size: 1,
          formatter: formatByte,
        },
        {
          label: 'Configuration Value',
          size: 1,
          formatter: formatByte,
        },
        {
          label: 'Configuration String Index',
          size: 1,
          formatter: formatByte,
        },
        {
          label: 'Attribute Bitmap',
          size: 1,
          formatter: formatBitmap,
        },
        {
          label: 'Max Power (2mA increments)',
          size: 1,
          formatter: formatByte,
        },
      ];

      let offset = 0;

      const expectNumInterfaces = rawData[offset + 4];

      offset = this.renderRawDataTree_(
          this.rawDataTreeRoot_, fields, rawData, offset);

      if (offset != CONFIGURATION_DESCRIPTOR_LENGTH) {
        this.showError_(
            'Some error(s) occurs during rendering configuration descriptor');
      }

      let indexInterface = 0;
      let indexEndpoint = 0;
      let indexUnknown = 0;
      let expectNumEndpoints = 0;

      while ((offset + 1) < rawData.length) {
        // The descriptor length and type byte still exists.
        switch (rawData[offset + 1]) {
          case INTERFACE_DESCRIPTOR_TYPE:
            [offset, expectNumEndpoints] = this.renderInterfaceDescriptor_(
                rawData, offset, indexInterface, expectNumEndpoints);
            indexInterface++;
            break;
          case ENDPOINT_DESCRIPTOR_TYPE:
            offset =
                this.renderEndpointDescriptor_(rawData, offset, indexEndpoint);
            indexEndpoint++;
            break;
          default:
            offset =
                this.renderUnknownDescriptor_(rawData, offset, indexUnknown);
            indexUnknown++;
            break;
        }
      }

      if (expectNumInterfaces != indexInterface) {
        this.showError_(`Expected to find ${
            expectNumInterfaces} interface descriptors but only encountered ${
            indexInterface}.`);
      }

      if (expectNumEndpoints != indexEndpoint) {
        this.showError_(`Expected to find ${
            expectNumEndpoints} interface descriptors but only encountered ${
            indexEndpoint}.`);
      }

      assert(
          offset === rawData.length,
          'Complete Configuration Descriptor Rendering Error');

      this.addMappingAction_();
    }

    /**
     * Renders a tree item to display indexInterface-th interface descriptor.
     * @param {!Uint8Array} rawData
     * @param {number} originalOffset
     * @param {number} indexInterface
     * @param {number} expectNumEndpoints
     * @return {number}
     * @private
     */
    renderInterfaceDescriptor_(
        rawData, originalOffset, indexInterface, expectNumEndpoints) {
      if (originalOffset + INTERFACE_DESCRIPTOR_LENGTH > rawData.length) {
        this.showError_(
            `Failed to read the ${indexInterface}-th interface descriptor.`);
      }

      const interfaceItem = customTreeItem(
          `Interface ${indexInterface}`,
          `descriptor-interface-${indexInterface}`);
      this.rawDataTreeRoot_.add(interfaceItem);

      const fields = [
        {
          label: 'Length',
          size: 1,
          formatter: formatByte,
        },
        {
          label: 'Descriptor Type',
          size: 1,
          formatter: formatDescriptorType,
        },
        {
          label: 'Interface Number',
          size: 1,
          formatter: formatByte,
        },
        {
          label: 'Alternate String',
          size: 1,
          formatter: formatByte,
        },
        {
          label: 'Number of Endpoints',
          size: 1,
          formatter: formatByte,
        },
        {
          label: 'Interface Class Code',
          size: 1,
          formatter: formatByte,
        },
        {
          label: 'Interface Subclass Code',
          size: 1,
          formatter: formatByte,
        },
        {
          label: 'Interface Protocol Code',
          size: 1,
          formatter: formatByte,
        },
        {
          label: 'Interface String Index',
          size: 1,
          formatter: formatByte,
        },
      ];

      let offset = originalOffset;

      expectNumEndpoints += rawData[offset + 4];

      offset = this.renderRawDataTree_(
          interfaceItem, fields, rawData, offset,
          `descriptor-interface-${indexInterface}`);

      if (offset != originalOffset + INTERFACE_DESCRIPTOR_LENGTH) {
        this.showError_(`Some error(s) occurred while rendering ${
            indexInterface}-th interface descriptor.`);
      }

      return [offset, expectNumEndpoints];
    }

    /**
     * Renders a tree item to display indexEndpoint-th endpoint descriptor.
     * @param {!Uint8Array} rawData
     * @param {number} originalOffset
     * @param {number} indexInterface
     * @return {number}
     * @private
     */
    renderEndpointDescriptor_(rawData, originalOffset, indexEndpoint) {
      if (originalOffset + ENDPOINT_DESCRIPTOR_LENGTH > rawData.length) {
        this.showError_(
            `Failed to read the ${indexEndpoint}-th endpoint descriptor.`);
      }

      const endpointItem = customTreeItem(
          `Endpoint ${indexEndpoint}`, `descriptor-endpoint-${indexEndpoint}`);
      this.rawDataTreeRoot_.add(endpointItem);

      const fields = [
        {
          label: 'Length',
          size: 1,
          formatter: formatByte,
        },
        {
          label: 'Descriptor Type',
          size: 1,
          formatter: formatDescriptorType,
        },
        {
          label: 'EndPoint Address',
          size: 1,
          formatter: formatByte,
        },
        {
          label: 'Attribute Bitmap',
          size: 1,
          formatter: formatBitmap,
        },
        {
          label: 'Max Packet Size',
          size: 2,
          formatter: formatShort,
        },
        {
          label: 'Interval',
          size: 1,
          formatter: formatByte,
        },
      ];

      let offset = originalOffset;
      offset = this.renderRawDataTree_(
          endpointItem, fields, rawData, offset,
          `descriptor-endpoint-${indexEndpoint}`);

      if (offset != originalOffset + ENDPOINT_DESCRIPTOR_LENGTH) {
        this.showError_(`Some error(s) occurred while rendering ${
            indexEndpoint}-th endpoint descriptor.`);
      }

      return offset;
    }

    /**
     * Renders a tree item to display length and type of indexUnknown-th unknown
     * descriptor.
     * @param {!Uint8Array} rawData
     * @param {number} originalOffset
     * @param {number} indexInterface
     * @return {number}
     * @private
     */
    renderUnknownDescriptor_(rawData, originalOffset, indexUnknown) {
      const length = rawData[originalOffset];

      if (originalOffset + length > rawData.length) {
        this.showError_(
            `Failed to read the ${indexUnknown}-th unknown descriptor.`);
        return;
      }

      const unknownItem = customTreeItem(
          `Unknown Descriptor ${indexUnknown}`,
          `descriptor-unknown-${indexUnknown}`);
      this.rawDataTreeRoot_.add(unknownItem);

      const fields = [
        {
          label: 'Length',
          size: 1,
          formatter: formatByte,
        },
        {
          label: 'Descriptor Type',
          size: 1,
          formatter: formatDescriptorType,
        },
      ];

      let offset = originalOffset;
      offset = this.renderRawDataTree_(
          unknownItem, fields, rawData, offset,
          `descriptor-unknown-${indexUnknown}`);

      const rawDataByteElements = this.rawDataElement_.querySelectorAll('span');

      for (; offset < originalOffset + length; offset++) {
        rawDataByteElements[offset].classList.add(`field-offset-${offset}`);
        rawDataByteElements[offset].classList.add(
            `descriptor-unknown-${indexUnknown}`);
      }

      return offset;
    }
  }

  /**
   * Renders a customized TreeItem with the given content and class name.
   * @param {string} itemLabel
   * @param {string=} opt_className
   * @return {!cr.ui.TreeItem}
   */
  function customTreeItem(itemLabel, opt_className) {
    const item = new cr.ui.TreeItem({
      label: itemLabel,
      icon: '',
    });
    if (opt_className) {
      item.querySelector('.tree-row').classList.add(opt_className);
    }
    return item;
  }

  /**
   * Parses one byte to decimal number string.
   * @param {!Uint8Array} rawData
   * @param {number} offset
   * @return {string}
   */
  function formatByte(rawData, offset) {
    return rawData[offset].toString();
  }

  /**
   * Parses two bytes to decimal number.
   * @param {!Uint8Array} rawData
   * @param {number} offset
   * @return {number}
   */
  function parseShort(rawData, offset) {
    const data = new DataView(rawData.buffer);
    return data.getUint16(offset, true);
  }

  /**
   * Parses two bytes to decimal number string.
   * @param {!Uint8Array} rawData
   * @param {number} offset
   * @return {string}
   */
  function formatShort(rawData, offset) {
    return parseShort(rawData, offset).toString();
  }

  /**
   * Parses two bytes to a hex string.
   * @param {!Uint8Array} rawData
   * @param {number} offset
   * @return {string}
   */
  function formatTwoBytesToHex(rawData, offset) {
    const num = parseShort(rawData, offset);
    return `0x${num.toString(16).padStart(4, '0').slice(-4).toUpperCase()}`;
  }

  /**
   * Parses two bytes to USB version format.
   * @param {!Uint8Array} rawData
   * @param {number} offset
   * @return {string}
   */
  function formatUsbVersion(rawData, offset) {
    return `${rawData[offset + 1]}.${rawData[offset] >> 4}.${
        rawData[offset] & 0x0F}`;
  }

  /**
   * Parses one byte to a bitmap.
   * @param {!Uint8Array} rawData
   * @param {number} offset
   * @return {string}
   */
  function formatBitmap(rawData, offset) {
    return rawData[offset].toString(2).padStart(8, '0').slice(-8);
  }

  /**
   * Parses descriptor type to a hex string.
   * @param {!Uint8Array} rawData
   * @param {number} offset
   * @return {string}
   */
  function formatDescriptorType(rawData, offset) {
    return `0x${
        rawData[offset].toString(16).padStart(2, '0').slice(-2).toUpperCase()}`;
  }

  return {
    DescriptorPanel,
  };
});