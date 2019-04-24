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
  const BOS_DESCRIPTOR_TYPE = 0x0F;

  const DEVICE_CAPABILITY_DESCRIPTOR_TYPE_PLATFORM = 0x05;

  const DEVICE_DESCRIPTOR_LENGTH = 18;
  const CONFIGURATION_DESCRIPTOR_LENGTH = 9;
  const INTERFACE_DESCRIPTOR_LENGTH = 9;
  const ENDPOINT_DESCRIPTOR_LENGTH = 7;
  const BOS_DESCRIPTOR_LENGTH = 5;
  const MAX_STRING_DESCRIPTOR_LENGTH = 0xFF;
  const MAX_URL_DESCRIPTOR_LENGTH = 0xFF;

  const CONTROL_TRANSFER_TIMEOUT_MS = 2000;  // 2 seconds

  // Language codes are defined in:
  // https://docs.microsoft.com/en-us/windows/desktop/intl/language-identifier-constants-and-strings
  const LANGUAGE_CODE_EN_US = 0x0409;

  // These constants are defined by the WebUSB specification:
  // http://wicg.github.io/webusb/
  const WEB_USB_DESCRIPTOR_LENGTH = 24;

  const GET_URL_REQUEST = 0x02;

  const WEB_USB_CAPABILITY_UUID = [
    // Little-endian encoding of {3408b638-09a9-47a0-8bfd-a0768815b665}.
    0x38, 0xB6, 0x08, 0x34, 0xA9, 0x09, 0xA0, 0x47, 0x8B, 0xFD, 0xA0, 0x76,
    0x88, 0x15, 0xB6, 0x65
  ];

  class DescriptorPanel {
    /**
     * @param {!device.mojom.UsbDeviceInterface} usbDeviceProxy
     * @param {!HTMLElement} rootElement
     * @param {DescriptorPanel=} stringDescriptorPanel
     */
    constructor(
        usbDeviceProxy, rootElement, stringDescriptorPanel = undefined) {
      /** @private {!device.mojom.UsbDeviceInterface} */
      this.usbDeviceProxy_ = usbDeviceProxy;

      /** @private {!HTMLElement} */
      this.rootElement_ = rootElement;

      this.clearView();

      if (stringDescriptorPanel) {
        /** @private {!DescriptorPanel} */
        this.stringDescriptorPanel_ = stringDescriptorPanel;
      }
    }

    /**
     * Adds a display area which contains a tree view and a byte view.
     * @return {{rawDataTreeRoot:!cr.ui.Tree,rawDataByteElement:!HTMLElement}}
     * @private
     */
    addNewDescriptorDisplayElement_() {
      const descriptorPanelTemplate =
          document.querySelector('#descriptor-panel-template');
      const descriptorPanelClone =
          document.importNode(descriptorPanelTemplate.content, true);

      /** @private {!HTMLElement} */
      const rawDataTreeRoot =
          descriptorPanelClone.querySelector('#raw-data-tree-view');
      /** @private {!HTMLElement} */
      const rawDataByteElement =
          descriptorPanelClone.querySelector('#raw-data-byte-view');

      cr.ui.decorate(rawDataTreeRoot, cr.ui.Tree);
      rawDataTreeRoot.detail = {payload: {}, children: {}};

      this.rootElement_.appendChild(descriptorPanelClone);
      return {rawDataTreeRoot, rawDataByteElement};
    }

    /**
     * Clears the data first before populating it with the new content.
     */
    clearView() {
      this.rootElement_.querySelectorAll('.descriptor-panel')
          .forEach(el => el.remove());
      this.rootElement_.querySelectorAll('error').forEach(el => el.remove());
    }

    /**
     * Renders a tree view to display the raw data in readable text.
     * @param {!cr.ui.Tree|!cr.ui.TreeItem} root
     * @param {!HTMLElement} rawDataByteElement
     * @param {!Array<Object>} fields
     * @param {!Uint8Array} rawData
     * @param {number} offset
     * @param {string=} parentClassName
     * @return {number}
     * @private
     */
    renderRawDataTree_(
        root, rawDataByteElement, fields, rawData, offset,
        parentClassName = undefined) {
      const rawDataByteElements = rawDataByteElement.querySelectorAll('span');

      for (const field of fields) {
        const className = `field-offset-${offset}`;

        const item = customTreeItem(
            `${field.label}${field.formatter(rawData, offset)}`, className);

        if (field.extraTreeItemFormatter) {
          field.extraTreeItemFormatter(rawData, offset, item, field.label);
        }

        for (let i = 0; i < field.size; i++) {
          rawDataByteElements[offset + i].classList.add(className);
          if (parentClassName) {
            rawDataByteElements[offset + i].classList.add(parentClassName);
          }
        }

        root.add(item);

        offset += field.size;
      }

      return offset;
    }

    /**
     * Renders a get string descriptor button for the String Descriptor Index
     * field, and adds an autocomplete value to the index input area in string
     * descriptor panel.
     * @param {!Uint8Array} rawData
     * @param {number} offset
     * @param {!cr.ui.TreeItem} item
     * @param {string} fieldLabel
     */
    renderIndexItem_(rawData, offset, item, fieldLabel) {
      const index = rawData[offset];
      if (index > 0) {
        if (!this.stringDescriptorPanel_.stringDescriptorIndexes.has(index)) {
          const optionElement = cr.doc.createElement('option');
          optionElement.label = index;
          optionElement.value = index;
          this.stringDescriptorPanel_.indexesListElement.appendChild(
              optionElement);

          this.stringDescriptorPanel_.stringDescriptorIndexes.add(index);
        }

        const buttonTemplate = document.querySelector('#raw-data-tree-button');
        const button = document.importNode(buttonTemplate.content, true)
                           .querySelector('button');
        item.querySelector('.tree-row').appendChild(button);
        button.addEventListener('click', (event) => {
          event.stopPropagation();
          // Clear the previous string descriptors.
          item.querySelector('.tree-children').textContent = '';
          this.stringDescriptorPanel_.clearView();
          this.stringDescriptorPanel_.renderStringDescriptorForAllLanguages(
              index, item);
        });
      } else if (index < 0) {
        // Delete the ': ' in fieldLabel.
        const fieldName = fieldLabel.slice(0, -2);
        this.showError_(`Invalid String Descriptor occurs in \
            field ${fieldName} of this descriptor.`);
      }
    }

    /**
     * Renders a URL descriptor item for the URL Descriptor Index and
     * adds it to the String Descriptor Index item.
     * @param {!Uint8Array} rawData
     * @param {number} offset
     * @param {!cr.ui.TreeItem} item
     * @param {string} fieldLabel Not used in this function, but used to match
     *     other extraTreeItemFormatter.
     */
    async renderLandingPageItem_(rawData, offset, item, fieldLabel) {
      // The second to last byte Byte is the vendor code used to query URL
      // descriptor. Last byte is index of url descriptor. These are defined by
      // the WebUSB specification: http://wicg.github.io/webusb/
      const vendorCode = rawData[offset - 1];
      const urlIndex = rawData[offset];
      const url = await this.getUrlDescriptor_(vendorCode, urlIndex);

      const landingPageItem = customTreeItem(url);
      item.add(landingPageItem);

      landingPageItem.querySelector('.tree-label')
          .addEventListener('click', () => window.open(url, '_blank'));
    }

    /**
     * Checks if the status of a descriptor read indicates success.
     * @param {number} status
     * @param {string} defaultMessage
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
      // Throws an error to stop rendering descriptor.
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

      const response = await this.usbDeviceProxy_.controlTransferIn(
          usbControlTransferParams, DEVICE_DESCRIPTOR_LENGTH,
          CONTROL_TRANSFER_TIMEOUT_MS);

      this.checkDescriptorGetSuccess_(
          response.status, 'Failed to read the device descriptor.');

      return new Uint8Array(response.data);
    }

    /**
     * Renders a view to display device descriptor hex data in both tree view
     * and raw form.
     */
    async renderDeviceDescriptor() {
      let rawData;
      try {
        await this.usbDeviceProxy_.open();
        rawData = await this.getDeviceDescriptor_();
      } catch (e) {
        // Stop rendering if failed to read the device descriptor.
        return;
      } finally {
        await this.usbDeviceProxy_.close();
      }

      const fields = [
        {
          label: 'Length: ',
          size: 1,
          formatter: formatByte,
        },
        {
          label: 'Descriptor Type: ',
          size: 1,
          formatter: formatDescriptorType,
        },
        {
          label: 'USB Version: ',
          size: 2,
          formatter: formatUsbVersion,
        },
        {
          label: 'Class Code: ',
          size: 1,
          formatter: formatByte,
        },
        {
          label: 'Subclass Code: ',
          size: 1,
          formatter: formatByte,
        },
        {
          label: 'Protocol Code: ',
          size: 1,
          formatter: formatByte,
        },
        {
          label: 'Control Pipe Maximum Packet Size: ',
          size: 1,
          formatter: formatByte,
        },
        {
          label: 'Vendor ID: ',
          size: 2,
          formatter: formatTwoBytesToHex,
        },
        {
          label: 'Product ID: ',
          size: 2,
          formatter: formatTwoBytesToHex,
        },
        {
          label: 'Device Version: ',
          size: 2,
          formatter: formatUsbVersion,
        },
        {
          label: 'Manufacturer String Index: ',
          size: 1,
          formatter: formatByte,
          extraTreeItemFormatter: this.renderIndexItem_.bind(this),
        },
        {
          label: 'Product String Index: ',
          size: 1,
          formatter: formatByte,
          extraTreeItemFormatter: this.renderIndexItem_.bind(this),
        },
        {
          label: 'Serial Number Index: ',
          size: 1,
          formatter: formatByte,
          extraTreeItemFormatter: this.renderIndexItem_.bind(this),
        },
        {
          label: 'Number of Configurations: ',
          size: 1,
          formatter: formatByte,
        },
      ];

      const displayElement = this.addNewDescriptorDisplayElement_();
      /** @type {!cr.ui.Tree} */
      const rawDataTreeRoot = displayElement.rawDataTreeRoot;
      /** @type {!HTMLElement} */
      const rawDataByteElement = displayElement.rawDataByteElement;

      renderRawDataBytes(rawDataByteElement, rawData);

      let offset = 0;
      offset = this.renderRawDataTree_(
          rawDataTreeRoot, rawDataByteElement, fields, rawData, offset);

      assert(
          offset === DEVICE_DESCRIPTOR_LENGTH,
          'Device Descriptor Rendering Error');

      addMappingAction(rawDataTreeRoot, rawDataByteElement);
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

      let response = await this.usbDeviceProxy_.controlTransferIn(
          usbControlTransferParams, CONFIGURATION_DESCRIPTOR_LENGTH,
          CONTROL_TRANSFER_TIMEOUT_MS);

      this.checkDescriptorGetSuccess_(
          response.status,
          'Failed to read the device configuration descriptor to determine ' +
              'the total descriptor length.');

      const data = new DataView(new Uint8Array(response.data).buffer);
      const length = data.getUint16(2, true);
      // Re-gets the data use the full length.
      response = await this.usbDeviceProxy_.controlTransferIn(
          usbControlTransferParams, length, CONTROL_TRANSFER_TIMEOUT_MS);

      this.checkDescriptorGetSuccess_(
          response.status,
          'Failed to read the complete configuration descriptor.');

      return new Uint8Array(response.data);
    }

    /**
     * Renders a view to display configuration descriptor hex data in both tree
     * view and raw form.
     */
    async renderConfigurationDescriptor() {
      let rawData;
      try {
        await this.usbDeviceProxy_.open();
        rawData = await this.getConfigurationDescriptor_();
      } catch (e) {
        // Stop rendering if failed to read the configuration descriptor.
        return;
      } finally {
        await this.usbDeviceProxy_.close();
      }

      const fields = [
        {
          label: 'Length: ',
          size: 1,
          formatter: formatByte,
        },
        {
          label: 'Descriptor Type: ',
          size: 1,
          formatter: formatDescriptorType,
        },
        {
          label: 'Total Length: ',
          size: 2,
          formatter: formatShort,
        },
        {
          label: 'Number of Interfaces: ',
          size: 1,
          formatter: formatByte,
        },
        {
          label: 'Configuration Value: ',
          size: 1,
          formatter: formatByte,
        },
        {
          label: 'Configuration String Index: ',
          size: 1,
          formatter: formatByte,
          extraTreeItemFormatter: this.renderIndexItem_.bind(this),
        },
        {
          label: 'Attribute Bitmap: ',
          size: 1,
          formatter: formatBitmap,
        },
        {
          label: 'Max Power (2mA increments): ',
          size: 1,
          formatter: formatByte,
        },
      ];

      const displayElement = this.addNewDescriptorDisplayElement_();
      /** @type {!cr.ui.Tree} */
      const rawDataTreeRoot = displayElement.rawDataTreeRoot;
      /** @type {!HTMLElement} */
      const rawDataByteElement = displayElement.rawDataByteElement;

      renderRawDataBytes(rawDataByteElement, rawData);

      let offset = 0;
      const expectNumInterfaces = rawData[4];

      offset = this.renderRawDataTree_(
          rawDataTreeRoot, rawDataByteElement, fields, rawData, offset);

      if (offset != CONFIGURATION_DESCRIPTOR_LENGTH) {
        this.showError_(
            'An error occurred while rendering configuration descriptor.');
      }

      let indexInterface = 0;
      let indexEndpoint = 0;
      let indexUnknown = 0;
      let expectNumEndpoints = 0;

      // Continue parsing while there are still unparsed interface, endpoint,
      // or endpoint companion descriptors. Stop if accessing the descriptor
      // type (rawData[offset + 1]) would cause us to read past the end of the
      // buffer.
      while ((offset + 1) < rawData.length) {
        switch (rawData[offset + 1]) {
          case INTERFACE_DESCRIPTOR_TYPE:
            [offset, expectNumEndpoints] = this.renderInterfaceDescriptor_(
                rawDataTreeRoot, rawDataByteElement, rawData, offset,
                indexInterface, expectNumEndpoints);
            indexInterface++;
            break;
          case ENDPOINT_DESCRIPTOR_TYPE:
            offset = this.renderEndpointDescriptor_(
                rawDataTreeRoot, rawDataByteElement, rawData, offset,
                indexEndpoint);
            indexEndpoint++;
            break;
          default:
            offset = this.renderUnknownDescriptor_(
                rawDataTreeRoot, rawDataByteElement, rawData, offset,
                indexUnknown);
            indexUnknown++;
            break;
        }
      }

      if (expectNumInterfaces != indexInterface) {
        this.showError_(`Expected to find \
            ${expectNumInterfaces} interface descriptors \
            but only encountered ${indexInterface}.`);
      }

      if (expectNumEndpoints != indexEndpoint) {
        this.showError_(`Expected to find \
            ${expectNumEndpoints} interface descriptors \
            but only encountered ${indexEndpoint}.`);
      }

      assert(
          offset === rawData.length,
          'Complete Configuration Descriptor Rendering Error');

      addMappingAction(rawDataTreeRoot, rawDataByteElement);
    }

    /**
     * Renders a tree item to display interface descriptor at index
     * indexInterface.
     * @param {!cr.ui.Tree} rawDataTreeRoot
     * @param {!HTMLElement} rawDataByteElement
     * @param {!Uint8Array} rawData
     * @param {number} originalOffset
     * @param {number} indexInterface
     * @param {number} expectNumEndpoints
     * @return {!Array<number>}
     * @private
     */
    renderInterfaceDescriptor_(
        rawDataTreeRoot, rawDataByteElement, rawData, originalOffset,
        indexInterface, expectNumEndpoints) {
      if (originalOffset + INTERFACE_DESCRIPTOR_LENGTH > rawData.length) {
        this.showError_(`Failed to read the interface descriptor\
          at index ${indexInterface}.`);
      }

      const interfaceItem = customTreeItem(
          `Interface ${indexInterface}`,
          `descriptor-interface-${indexInterface}`);
      rawDataTreeRoot.add(interfaceItem);

      const fields = [
        {
          label: 'Length: ',
          size: 1,
          formatter: formatByte,
        },
        {
          label: 'Descriptor Type: ',
          size: 1,
          formatter: formatDescriptorType,
        },
        {
          label: 'Interface Number: ',
          size: 1,
          formatter: formatByte,
        },
        {
          label: 'Alternate String: ',
          size: 1,
          formatter: formatByte,
        },
        {
          label: 'Number of Endpoint: ',
          size: 1,
          formatter: formatByte,
        },
        {
          label: 'Interface Class Code: ',
          size: 1,
          formatter: formatByte,
        },
        {
          label: 'Interface Subclass Code: ',
          size: 1,
          formatter: formatByte,
        },
        {
          label: 'Interface Protocol Code: ',
          size: 1,
          formatter: formatByte,
        },
        {
          label: 'Interface String Index: ',
          size: 1,
          formatter: formatByte,
          extraTreeItemFormatter: this.renderIndexItem_.bind(this),
        },
      ];

      let offset = originalOffset;

      expectNumEndpoints += rawData[offset + 4];

      offset = this.renderRawDataTree_(
          interfaceItem, rawDataByteElement, fields, rawData, offset,
          `descriptor-interface-${indexInterface}`);

      if (offset != originalOffset + INTERFACE_DESCRIPTOR_LENGTH) {
        this.showError_(
            `An error occurred while rendering interface descriptor at \
            index ${indexInterface}.`);
      }

      return [offset, expectNumEndpoints];
    }

    /**
     * Renders a tree item to display endpoint descriptor at index
     * indexEndpoint.
     * @param {!cr.ui.Tree} rawDataTreeRoot
     * @param {!HTMLElement} rawDataByteElement
     * @param {!Uint8Array} rawData
     * @param {number} originalOffset
     * @param {number} indexEndpoint
     * @return {number}
     * @private
     */
    renderEndpointDescriptor_(
        rawDataTreeRoot, rawDataByteElement, rawData, originalOffset,
        indexEndpoint) {
      if (originalOffset + ENDPOINT_DESCRIPTOR_LENGTH > rawData.length) {
        this.showError_(`Failed to read the endpoint descriptor at \
        index ${indexEndpoint}.`);
      }

      const endpointItem = customTreeItem(
          `Endpoint ${indexEndpoint}`, `descriptor-endpoint-${indexEndpoint}`);
      rawDataTreeRoot.add(endpointItem);

      const fields = [
        {
          label: 'Length: ',
          size: 1,
          formatter: formatByte,
        },
        {
          label: 'Descriptor Type: ',
          size: 1,
          formatter: formatDescriptorType,
        },
        {
          label: 'EndPoint Address: ',
          size: 1,
          formatter: formatByte,
        },
        {
          label: 'Attribute Bitmap: ',
          size: 1,
          formatter: formatBitmap,
        },
        {
          label: 'Max Packet Size: ',
          size: 2,
          formatter: formatShort,
        },
        {
          label: 'Interval: ',
          size: 1,
          formatter: formatByte,
        },
      ];

      let offset = originalOffset;
      offset = this.renderRawDataTree_(
          endpointItem, rawDataByteElement, fields, rawData, offset,
          `descriptor-endpoint-${indexEndpoint}`);

      if (offset != originalOffset + ENDPOINT_DESCRIPTOR_LENGTH) {
        this.showError_(
            `An error occurred while rendering endpoint descriptor at \
             index ${indexEndpoint}.`);
      }

      return offset;
    }

    /**
     * Renders a tree item to display length and type of unknown descriptor at
     * index indexUnknown.
     * @param {!cr.ui.Tree} rawDataTreeRoot
     * @param {!HTMLElement} rawDataByteElement
     * @param {!Uint8Array} rawData
     * @param {number} originalOffset
     * @param {number} indexUnknown
     * @return {number}
     * @private
     */
    renderUnknownDescriptor_(
        rawDataTreeRoot, rawDataByteElement, rawData, originalOffset,
        indexUnknown) {
      const length = rawData[originalOffset];

      if (originalOffset + length > rawData.length) {
        this.showError_(
            `Failed to read the unknown descriptor at index ${indexUnknown}.`);
        return;
      }

      const unknownItem = customTreeItem(
          `Unknown Descriptor ${indexUnknown}`,
          `descriptor-unknown-${indexUnknown}`);
      rawDataTreeRoot.add(unknownItem);

      const fields = [
        {
          label: 'Length: ',
          size: 1,
          formatter: formatByte,
        },
        {
          label: 'Descriptor Type: ',
          size: 1,
          formatter: formatDescriptorType,
        },
      ];

      let offset = originalOffset;
      offset = this.renderRawDataTree_(
          unknownItem, rawDataByteElement, fields, rawData, offset,
          `descriptor-unknown-${indexUnknown}`);

      const rawDataByteElements = rawDataByteElement.querySelectorAll('span');

      for (; offset < originalOffset + length; offset++) {
        rawDataByteElements[offset].classList.add(`field-offset-${offset}`);
        rawDataByteElements[offset].classList.add(
            `descriptor-unknown-${indexUnknown}`);
      }

      return offset;
    }

    /**
     * Gets all the Supported Language Code of this device, and adds them to
     * autocomplete value of the language code input area in string descriptor
     * panel.
     * @return {!Array<number>}
     */
    async getAllLanguageCodes() {
      /** @type {device.mojom.UsbControlTransferParams} */
      const usbControlTransferParams = {};
      usbControlTransferParams.type =
          device.mojom.UsbControlTransferType.STANDARD;
      usbControlTransferParams.recipient =
          device.mojom.UsbControlTransferRecipient.DEVICE;
      usbControlTransferParams.request = GET_DESCRIPTOR_REQUEST;
      usbControlTransferParams.value = (STRING_DESCRIPTOR_TYPE << 8);
      usbControlTransferParams.index = 0;

      await this.usbDeviceProxy_.open();

      const response = await this.usbDeviceProxy_.controlTransferIn(
          usbControlTransferParams, MAX_STRING_DESCRIPTOR_LENGTH,
          CONTROL_TRANSFER_TIMEOUT_MS);

      await this.usbDeviceProxy_.close();

      try {
        this.checkDescriptorGetSuccess_(
            response.status,
            'Failed to read the device string descriptor to determine ' +
                'all supported languages.');
      } catch (e) {
        // Stop rendering autocomplete datalist if failed to read the string
        // descriptor.
        return;
      }

      const responseData = new Uint8Array(response.data);
      this.languageCodesListElement_.innerText = '';

      const optionAllElement = cr.doc.createElement('option');
      optionAllElement.value = 'All';
      this.languageCodesListElement_.appendChild(optionAllElement);

      const languageCodesList = [];
      // First two bytes are length and descriptor type(0x03);
      for (let i = 2; i < responseData.length; i += 2) {
        const languageCode = parseShort(responseData, i);

        const optionElement = cr.doc.createElement('option');
        optionElement.label = parseLanguageCode(languageCode);
        optionElement.value = `0x${toHex(languageCode, 4)}`;

        this.languageCodesListElement_.appendChild(optionElement);

        languageCodesList.push(languageCode);
      }

      return languageCodesList;
    }

    /**
     * Gets string descriptor of current device with index and language code.
     * @param {number} index
     * @param {number} languageCode
     * @return {{languageCode:string,rawData:!Uint8Array}}
     * @private
     */
    async getStringDescriptorForLanguageCode_(index, languageCode) {
      /** @type {device.mojom.UsbControlTransferParams} */
      const usbControlTransferParams = {};
      usbControlTransferParams.type =
          device.mojom.UsbControlTransferType.STANDARD;
      usbControlTransferParams.recipient =
          device.mojom.UsbControlTransferRecipient.DEVICE;
      usbControlTransferParams.request = GET_DESCRIPTOR_REQUEST;

      usbControlTransferParams.index = languageCode;

      usbControlTransferParams.value = (STRING_DESCRIPTOR_TYPE << 8) | index;

      const response = await this.usbDeviceProxy_.controlTransferIn(
          usbControlTransferParams, MAX_DESCRIPTOR_LENGTH,
          CONTROL_TRANSFER_TIMEOUT_MS);

      const languageCodeStr = parseLanguageCode(languageCode);
      this.checkDescriptorGetSuccess_(
          response.status, `Failed to read the device string descriptor of\
              index: ${index}, language: ${languageCodeStr}.`);

      const rawData = new Uint8Array(response.data);
      return {languageCodeStr, rawData};
    }

    /**
     * Gets string descriptor of current device with given index and language
     * code.
     * @param {number} index
     * @param {number} languageCode
     * @param {cr.ui.TreeItem=} treeItem
     */
    async renderStringDescriptorForLanguageCode(
        index, languageCode, treeItem = undefined) {
      this.rootElement_.hidden = false;

      this.indexInput_.value = index;

      let rawDataMap;
      try {
        await this.usbDeviceProxy_.open();
        rawDataMap =
            await this.getStringDescriptorForLanguageCode_(index, languageCode);
      } catch (e) {
        // Stop rendering if failed to read the string descriptor.
        return;
      } finally {
        await this.usbDeviceProxy_.close();
      }

      const languageStr = rawDataMap.languageCodeStr;
      const rawData = rawDataMap.rawData;

      const length = rawData[0];
      if (length > rawData.length) {
        this.showError_(`Failed to read the string descriptor at \
            index ${index} in ${languageStr}.`);
        return;
      }

      const fields = [
        {
          'label': 'Length: ',
          'size': 1,
          'formatter': formatByte,
        },
        {
          'label': 'Descriptor Type: ',
          'size': 1,
          'formatter': formatDescriptorType,
        },
      ];

      // The first two elements are length and descriptor type.
      for (let i = 2; i < rawData.length; i += 2) {
        const field = {
          'label': '',
          'size': 2,
          'formatter': formatLetter,
        };
        fields.push(field);
      }

      const displayElement = this.addNewDescriptorDisplayElement_();
      /** @type {!cr.ui.Tree} */
      const rawDataTreeRoot = displayElement.rawDataTreeRoot;
      /** @type {!HTMLElement} */
      const rawDataByteElement = displayElement.rawDataByteElement;

      // The first two elements of rawData are length and descriptor type.
      const stringDescriptor = decodeUtf16Array(rawData.slice(2));
      const stringDescriptorItem = customTreeItem(
          `${languageStr}: ${stringDescriptor}`,
          `descriptor-string-${index}-language-code-${languageStr}`);
      rawDataTreeRoot.add(stringDescriptorItem);
      if (treeItem) {
        treeItem.add(customTreeItem(`${languageStr}: ${stringDescriptor}`));
        treeItem.expanded = true;
      }

      renderRawDataBytes(rawDataByteElement, rawData);

      let offset = 0;
      offset = this.renderRawDataTree_(
          stringDescriptorItem, rawDataByteElement, fields, rawData, offset,
          `descriptor-string-${index}-language-code-${languageStr}`);

      addMappingAction(rawDataTreeRoot, rawDataByteElement);
    }

    /**
     * Gets string descriptor in all supported languages of current device with
     * given index.
     * @param {number} index
     * @param {cr.ui.TreeItem=} treeItem
     */
    async renderStringDescriptorForAllLanguages(index, treeItem = undefined) {
      this.rootElement_.hidden = false;

      this.indexInput_.value = index;

      /** @type {!Array<number>|undefined} */
      const languageCodesList = await this.getAllLanguageCodes();

      for (const languageCode of languageCodesList) {
        await this.renderStringDescriptorForLanguageCode(
            index, languageCode, treeItem);
      }
    }

    /**
     * Initializes the string descriptor panel for autocomplete functionality.
     * @param {number} tabId
     */
    initialStringDescriptorPanel(tabId) {
      // Binds the input area and datalist use each tab's unique id.
      this.rootElement_.querySelectorAll('input').forEach(
          el => el.setAttribute('list', `${el.getAttribute('list')}-${tabId}`));
      this.rootElement_.querySelectorAll('datalist')
          .forEach(el => el.id = `${el.id}-${tabId}`);

      /** @type {!HTMLElement} */
      const button = this.rootElement_.querySelector('button');
      /** @type {!HTMLElement} */
      this.indexInput_ = this.rootElement_.querySelector('#index-input');
      /** @type {!HTMLElement} */
      const languageCodeInput =
          this.rootElement_.querySelector('#language-code-input');

      button.addEventListener('click', () => {
        this.clearView();
        const index = Number.parseInt(this.indexInput_.value);
        if (this.checkIndexValueValid_(index)) {
          if (languageCodeInput.value === 'All') {
            this.renderStringDescriptorForAllLanguages(index);
          } else {
            const languageCode = Number.parseInt(languageCodeInput.value);
            if (this.checkLanguageCodeValueValid_(languageCode)) {
              this.renderStringDescriptorForLanguageCode(index, languageCode);
            }
          }
        }
      });

      /** @type {!Set<number>} */
      this.stringDescriptorIndexes = new Set();
      /** @type {!HTMLElement} */
      this.indexesListElement =
          this.rootElement_.querySelector(`#indexes-${tabId}`);
      /** @type {!HTMLElement} */
      this.languageCodesListElement_ =
          this.rootElement_.querySelector(`#languages-${tabId}`);
    }

    /**
     * Checks if the user input index is a valid uint8 number.
     * @param {number} index
     * @return {boolean}
     */
    checkIndexValueValid_(index) {
      // index is 8 bit. 0 is reserved to query all supported language codes.
      if (Number.isNaN(index) || index < 1 || index > 255) {
        this.showError_('Invalid Index.');
        return false;
      }
      return true;
    }

    /**
     * Checks if the user input language code is a valid uint16 number.
     * @param {number} languageCode
     * @return {boolean}
     */
    checkLanguageCodeValueValid_(languageCode) {
      if (Number.isNaN(languageCode) || languageCode < 0 ||
          languageCode > 65535) {
        this.showError_('Invalid Language Code.');
        return false;
      }
      return true;
    }

    /**
     * Gets the Binary device Object Store (BOS) descriptor of the current
     * device, which contains the WebUSB descriptor and Microsoft OS 2.0
     * descriptor.
     * @return {!Uint8Array}
     * @private
     */
    async getBosDescriptor_() {
      /** @type {device.mojom.UsbControlTransferParams} */
      const usbControlTransferParams = {};
      usbControlTransferParams.type =
          device.mojom.UsbControlTransferType.STANDARD;
      usbControlTransferParams.recipient =
          device.mojom.UsbControlTransferRecipient.DEVICE;
      usbControlTransferParams.request = GET_DESCRIPTOR_REQUEST;
      usbControlTransferParams.value = (BOS_DESCRIPTOR_TYPE << 8);
      usbControlTransferParams.index = 0;

      let response = await this.usbDeviceProxy_.controlTransferIn(
          usbControlTransferParams, BOS_DESCRIPTOR_LENGTH,
          CONTROL_TRANSFER_TIMEOUT_MS);

      this.checkDescriptorGetSuccess_(
          response.status,
          'Failed to read the device BOS descriptor to determine ' +
              'the total descriptor length.');

      const data = new DataView(new Uint8Array(response.data).buffer);
      const length = data.getUint16(2, true);

      // Re-gets the data use the full length.
      response = await this.usbDeviceProxy_.controlTransferIn(
          usbControlTransferParams, length, CONTROL_TRANSFER_TIMEOUT_MS);

      this.checkDescriptorGetSuccess_(
          response.status, 'Failed to read the complete BOS descriptor.');

      return new Uint8Array(response.data);
    }

    /**
     * Renders a view to display BOS descriptor hex data in both tree view
     * and raw form.
     */
    async renderBosDescriptor() {
      let rawData;
      try {
        await this.usbDeviceProxy_.open();
        rawData = await this.getBosDescriptor_();
      } catch (e) {
        // Stop rendering if failed to read the BOS descriptor.
        return;
      } finally {
        await this.usbDeviceProxy_.close();
      }

      const fields = [
        {
          'label': 'Length: ',
          'size': 1,
          'formatter': formatByte,
        },
        {
          'label': 'Descriptor Type: ',
          'size': 1,
          'formatter': formatDescriptorType,
        },
        {
          'label': 'Total Length: ',
          'size': 2,
          'formatter': formatShort,
        },
        {
          'label': 'Number of Device Capability Descriptors: ',
          'size': 1,
          'formatter': formatByte,
        },
      ];

      const displayElement = this.addNewDescriptorDisplayElement_();
      /** @type {!cr.ui.Tree} */
      const rawDataTreeRoot = displayElement.rawDataTreeRoot;
      /** @type {!HTMLElement} */
      const rawDataByteElement = displayElement.rawDataByteElement;

      renderRawDataBytes(rawDataByteElement, rawData);

      let offset = 0;
      offset = this.renderRawDataTree_(
          rawDataTreeRoot, rawDataByteElement, fields, rawData, offset);

      if (offset != BOS_DESCRIPTOR_LENGTH) {
        this.showError_(
            'An error occurred while rendering BOS descriptor header.');
      }

      let indexWebUsb = 0;
      let indexUnknownBos = 0;
      // Continue parsing while there are still unparsed device capability
      // descriptors. Stop if accessing the device capability type
      // (rawData[offset + 2]) would cause us to read past the end of the
      // buffer.
      while ((offset + 2) < rawData.length) {
        switch (rawData[offset + 2]) {
          case DEVICE_CAPABILITY_DESCRIPTOR_TYPE_PLATFORM:
            if (isSameUUID(rawData, offset, WEB_USB_CAPABILITY_UUID)) {
              offset = this.renderWebUsbDescriptor_(
                  rawDataTreeRoot, rawDataByteElement, rawData, offset,
                  indexWebUsb);
              indexWebUsb++;
              break;
            }
          default:
            offset = this.renderUnknownBosDescriptor_(
                rawDataTreeRoot, rawDataByteElement, rawData, offset,
                indexUnknownBos);
            indexUnknownBos++;
        }
      }

      const expectNumBosDescriptors = rawData[5];
      const encounteredNumBosDescriptors = indexWebUsb + indexUnknownBos;
      if (encounteredNumBosDescriptors === expectNumBosDescriptors) {
        this.showError_(`Expected to find \
            ${expectNumBosDescriptors} interface descriptors \
            but only encountered ${encounteredNumBosDescriptors}.`);
      }

      assert(
          offset === rawData.length, 'Complete BOS Descriptor Rendering Error');

      addMappingAction(rawDataTreeRoot, rawDataByteElement);
    }

    /**
     * Renders a tree item to display WebUSB descriptor at index
     * indexWebUsb.
     * @param {!cr.ui.Tree} rawDataTreeRoot
     * @param {!HTMLElement} rawDataByteElement
     * @param {!Uint8Array} rawData
     * @param {number} originalOffset
     * @param {number} indexWebUsb
     * @return {!Array<number>}
     * @private
     */
    renderWebUsbDescriptor_(
        rawDataTreeRoot, rawDataByteElement, rawData, originalOffset,
        indexWebUsb) {
      if (originalOffset + WEB_USB_DESCRIPTOR_LENGTH > rawData.length) {
        this.showError_(`Failed to read the WebUSB descriptor\
          at index ${indexWebUsb}.`);
      }

      const webUsbItem = customTreeItem(
          `WebUSB Descriptor`, `descriptor-webusb-${indexWebUsb}`);
      rawDataTreeRoot.add(webUsbItem);

      const fields = [
        {
          label: 'Length: ',
          size: 1,
          formatter: formatByte,
        },
        {
          label: 'Descriptor Type: ',
          size: 1,
          formatter: formatDescriptorType,
        },
        {
          label: 'Device Capability Descriptor Type: ',
          size: 1,
          formatter: formatByte,
        },
        {
          label: 'Reserved (Should be Zero): ',
          size: 1,
          formatter: formatByte,
        },
        {
          label: 'UUID: ',
          size: 16,
          formatter: formatUUID,
        },
        {
          label: 'Protocol version supported (should be 1.0.0): ',
          size: 2,
          formatter: formatUsbVersion,
        },
        {
          label: 'Vendor Code: ',
          size: 1,
          formatter: formatByte,
        },
        {
          label: 'Landing Page: ',
          size: 1,
          formatter: formatByte,
          extraTreeItemFormatter: this.renderLandingPageItem_.bind(this),
        },
      ];

      let offset = originalOffset;

      offset = this.renderRawDataTree_(
          webUsbItem, rawDataByteElement, fields, rawData, offset,
          `descriptor-webusb-${indexWebUsb}`);

      if (offset != originalOffset + WEB_USB_DESCRIPTOR_LENGTH) {
        this.showError_(
            `An error occurred while rendering WebUSB descriptor at \
            index ${indexWebUsb}.`);
      }

      return offset;
    }

    /**
     * Renders a tree item to display unknown BOS descriptor at indexUnknownBos
     * @param {!cr.ui.Tree} rawDataTreeRoot
     * @param {!HTMLElement} rawDataByteElement
     * @param {!Uint8Array} rawData
     * @param {number} originalOffset
     * @param {number} indexUnknownBos
     * @return {!Array<number>}
     * @private
     */
    renderUnknownBosDescriptor_(
        rawDataTreeRoot, rawDataByteElement, rawData, originalOffset,
        indexUnknownBos) {
      const length = rawData[originalOffset];

      const unknownBosItem = customTreeItem(
          `Unknown BOS Descriptor`, `descriptor-unknownbos-${indexUnknownBos}`);
      rawDataTreeRoot.add(unknownBosItem);

      const fields = [
        {
          label: 'Length: ',
          size: 1,
          formatter: formatByte,
        },
        {
          label: 'Descriptor Type: ',
          size: 1,
          formatter: formatDescriptorType,
        },
        {
          label: 'Device Capability Descriptor Type: ',
          size: 1,
          formatter: formatByte,
        },
      ];

      let offset = originalOffset;
      offset = this.renderRawDataTree_(
          unknownBosItem, rawDataByteElement, fields, rawData, offset,
          `descriptor-unknownbos-${indexUnknownBos}`);

      const rawDataByteElements = rawDataByteElement.querySelectorAll('span');

      for (; offset < originalOffset + length; offset++) {
        rawDataByteElements[offset].classList.add(`field-offset-${offset}`);
        rawDataByteElements[offset].classList.add(
            `descriptor-unknownbos-${indexUnknownBos}`);
      }

      return originalOffset + length;
    }

    /**
     * Gets the URL Descriptor, and returns the parsed URL.
     * @param {number} vendorCode
     * @param {number} urlIndex
     * @return {string}
     * @private
     */
    async getUrlDescriptor_(vendorCode, urlIndex) {
      /** @type {device.mojom.UsbControlTransferParams} */
      const usbControlTransferParams = {};
      usbControlTransferParams.recipient =
          device.mojom.UsbControlTransferRecipient.DEVICE;
      // These constants are defined by the WebUSB specification:
      // http://wicg.github.io/webusb/
      usbControlTransferParams.type =
          device.mojom.UsbControlTransferType.VENDOR;
      usbControlTransferParams.request = vendorCode;
      usbControlTransferParams.value = urlIndex;
      usbControlTransferParams.index = GET_URL_REQUEST;

      await this.usbDeviceProxy_.open();

      // Gets the URL descriptor.
      const urlResponse = await this.usbDeviceProxy_.controlTransferIn(
          usbControlTransferParams, MAX_URL_DESCRIPTOR_LENGTH,
          CONTROL_TRANSFER_TIMEOUT_MS);

      await this.usbDeviceProxy_.close();

      try {
        this.checkDescriptorGetSuccess_(
            urlResponse.status, 'Failed to read the device URL descriptor.');
      } catch (e) {
        // Stops parsing to string format URL.
        return;
      }

      let urlDescriptor;
      // URL Prefixes are defined by Chapter 4.3.1 of the WebUSB specification:
      // http://wicg.github.io/webusb/
      switch (urlResponse.data[2]) {
        case 0:
          urlDescriptor = 'http://';
          break;
        case 1:
          urlDescriptor = 'https://';
          break;
        case 255:
        default:
          urlDescriptor = '';
      }
      // The first three elements of urlResponse.data are length, descriptor
      // type and URL scheme prefix.
      urlDescriptor +=
          decodeUtf8Array(new Uint8Array(urlResponse.data.slice(3)));
      return urlDescriptor;
    }
  }

  /**
   * Renders a customized TreeItem with the given content and class name.
   * @param {string} itemLabel
   * @param {string=} className
   * @return {!cr.ui.TreeItem}
   */
  function customTreeItem(itemLabel, className = undefined) {
    const item = new cr.ui.TreeItem({
      label: itemLabel,
      icon: '',
    });
    if (className) {
      item.querySelector('.tree-row').classList.add(className);
    }
    return item;
  }

  /**
   * Adds function for mapping between two views.
   * @param {!cr.ui.Tree} rawDataTreeRoot
   * @param {!HTMLElement} rawDataByteElement
   */
  function addMappingAction(rawDataTreeRoot, rawDataByteElement) {
    // Highlights the byte(s) that hovered in the tree.
    rawDataTreeRoot.querySelectorAll('.tree-row').forEach((el) => {
      const classList = el.classList;
      // classList[0] is 'tree-row'. classList[1] of tree item for fields
      // starts with 'field-offset-', and classList[1] of tree item for
      // descriptors (ie. endpoint descriptor) is descriptor type and index.
      const fieldOffsetOrDescriptorClass = classList[1];
      assert(
          fieldOffsetOrDescriptorClass.startsWith('field-offset-') ||
          fieldOffsetOrDescriptorClass.startsWith('descriptor-'));

      el.addEventListener('pointerenter', (event) => {
        rawDataByteElement.querySelectorAll(`.${fieldOffsetOrDescriptorClass}`)
            .forEach((el) => el.classList.add('hovered-field'));
        event.stopPropagation();
      });

      el.addEventListener('pointerleave', () => {
        rawDataByteElement.querySelectorAll(`.${fieldOffsetOrDescriptorClass}`)
            .forEach((el) => el.classList.remove('hovered-field'));
      });

      el.addEventListener('click', (event) => {
        if (event.target.className != 'expand-icon') {
          // Clears all the selected elements before select another.
          rawDataByteElement.querySelectorAll('#raw-data-byte-view span')
              .forEach((el) => el.classList.remove('selected-field'));

          rawDataByteElement
              .querySelectorAll(`.${fieldOffsetOrDescriptorClass}`)
              .forEach((el) => el.classList.add('selected-field'));
        }
      });
    });

    // Selects the tree item that displays the byte hovered in the raw view.
    const rawDataByteElements = rawDataByteElement.querySelectorAll('span');
    rawDataByteElements.forEach((el) => {
      const classList = el.classList;

      const fieldOffsetClass = classList[0];
      assert(fieldOffsetClass.startsWith('field-offset-'));

      el.addEventListener('pointerenter', () => {
        rawDataByteElement.querySelectorAll(`.${fieldOffsetClass}`)
            .forEach((el) => el.classList.add('hovered-field'));
        const el = rawDataTreeRoot.querySelector(`.${fieldOffsetClass}`);
        if (el) {
          el.classList.add('hover');
        }
      });

      el.addEventListener('pointerleave', () => {
        rawDataByteElement.querySelectorAll(`.${fieldOffsetClass}`)
            .forEach((el) => el.classList.remove('hovered-field'));
        const el = rawDataTreeRoot.querySelector(`.${fieldOffsetClass}`);
        if (el) {
          el.classList.remove('hover');
        }
      });

      el.addEventListener('click', () => {
        const el = rawDataTreeRoot.querySelector(`.${fieldOffsetClass}`);
        if (el) {
          el.click();
        }
      });
    });
  }

  /**
   * Renders an element to display the raw data in hex, byte by byte, and
   * keeps every row no more than 16 bytes.
   * @param {!HTMLElement} rawDataByteElement
   * @param {!Uint8Array} rawData
   */
  function renderRawDataBytes(rawDataByteElement, rawData) {
    const rawDataByteContainerTemplate =
        document.querySelector('#raw-data-byte-container-template');
    const rawDataByteContainerClone =
        document.importNode(rawDataByteContainerTemplate.content, true);
    const rawDataByteContainerElement =
        rawDataByteContainerClone.querySelector('div');

    const rawDataByteTemplate =
        document.querySelector('#raw-data-byte-template');
    for (const value of rawData) {
      const rawDataByteClone =
          document.importNode(rawDataByteTemplate.content, true);
      const rawDataByteElement = rawDataByteClone.querySelector('span');

      rawDataByteElement.textContent = toHex(value, 2);
      rawDataByteContainerElement.appendChild(rawDataByteElement);
    }
    rawDataByteElement.appendChild(rawDataByteContainerElement);
  }

  /**
   * Converts a number to a hexadecimal string padded with zeros to the given
   * number of digits.
   * @param {number} number
   * @param {number} numOfDigits
   * @return {string}
   */
  function toHex(number, numOfDigits) {
    return number.toString(16)
        .padStart(numOfDigits, '0')
        .slice(0 - numOfDigits)
        .toUpperCase();
  }

  /**
   * Parses unicode array to string.
   * @param {!Uint8Array} arr
   * @return {string}
   */
  function decodeUtf16Array(arr) {
    let str = '';
    for (let i = 0; i < arr.length; i += 2) {
      str += formatLetter(arr, i);
    }
    return str;
  }

  /**
   * Parses UTF-8 array to string.
   * @param {!Uint8Array} arr
   * @return {string}
   */
  function decodeUtf8Array(arr) {
    let str = '';
    for (let i = 0; i < arr.length; i++) {
      str += String.fromCodePoint(arr[i]);
    }
    return str;
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
   * Parses two bytes to decimal number string.
   * @param {!Uint8Array} rawData
   * @param {number} offset
   * @return {string}
   */
  function formatLetter(rawData, offset) {
    const num = parseShort(rawData, offset);
    return String.fromCodePoint(num);
  }

  /**
   * Parses two bytes to a hex string.
   * @param {!Uint8Array} rawData
   * @param {number} offset
   * @return {string}
   */
  function formatTwoBytesToHex(rawData, offset) {
    const num = parseShort(rawData, offset);
    return `0x${toHex(num, 4)}`;
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
    return `0x${toHex(rawData[offset], 2)}`;
  }

  /**
   * Parses UUID field.
   * @param {!Uint8Array} rawData
   * @param {number} offset
   * @return {string}
   */
  function formatUUID(rawData, offset) {
    // UUID is 16 bytes (Section 9.6.2.4 of Universal Serial Bus 3.1
    // Specification).
    // Additional reference: IETF RFC 4122. https://tools.ietf.org/html/rfc4122
    let uuidStr = '';
    const data = new DataView(rawData.buffer);

    uuidStr += toHex(data.getUint32(offset, true), 8);
    uuidStr += '-';
    uuidStr += toHex(data.getUint16(offset + 4, true), 4);
    uuidStr += '-';
    uuidStr += toHex(data.getUint16(offset + 6, true), 4);
    uuidStr += '-';
    uuidStr += toHex(data.getUint8(offset + 8), 2);
    uuidStr += toHex(data.getUint8(offset + 9), 2);
    uuidStr += '-';
    uuidStr += toHex(data.getUint8(offset + 10), 2);
    uuidStr += toHex(data.getUint8(offset + 11), 2);
    uuidStr += toHex(data.getUint8(offset + 12), 2);
    uuidStr += toHex(data.getUint8(offset + 13), 2);
    uuidStr += toHex(data.getUint8(offset + 14), 2);
    uuidStr += toHex(data.getUint8(offset + 15), 2);

    return uuidStr;
  }

  /**
   * Parses language code to readable language name.
   * @param {number} languageCode
   * @return {string}
   */
  function parseLanguageCode(languageCode) {
    switch (languageCode) {
      case LANGUAGE_CODE_EN_US:
        return 'en-US';
      default:
        return `0x${toHex(languageCode, 4)}`;
    }
  }

  /**
   * Checks if two UUIDs are same.
   * @param {!Uint8Array} rawData
   * @param {number} offset
   * @param {!Array<number>} uuidArr
   */
  function isSameUUID(rawData, offset, uuidArr) {
    // Validate the Platform Capability Descriptor
    if (offset + 20 > rawData.length) {
      return false;
    }
    // UUID is from index 4 to index 19 (Section 9.6.2.4 of Universal Serial
    // Bus 3.1 Specification).
    for (const [i, num] of rawData.slice(offset + 4, offset + 20).entries()) {
      if (num !== uuidArr[i]) {
        return false;
      }
    }
    return true;
  }

  return {
    DescriptorPanel,
  };
});
