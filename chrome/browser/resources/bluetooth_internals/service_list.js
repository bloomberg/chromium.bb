// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Javascript for ServiceList and ServiceListItem, served from
 *     chrome://bluetooth-internals/.
 */

cr.define('service_list', function() {
  /** @const */ var ExpandableList = expandable_list.ExpandableList;
  /** @const */ var ExpandableListItem = expandable_list.ExpandableListItem;

  /**
   * Property names that will be displayed in the ObjectFieldSet which contains
   * the ServiceInfo object.
   */
  var PROPERTY_NAMES = {
    id: 'ID',
    'uuid.uuid': 'UUID',
    is_primary: 'Type',
  };

  /**
   * A list item that displays the data in a ServiceInfo object. The brief
   * section contains the UUID of the given |serviceInfo|. The expanded section
   * contains an ObjectFieldSet that displays all of the properties in the
   * given |serviceInfo|.
   * @param {!interfaces.BluetoothDevice.ServiceInfo} serviceInfo
   * @constructor
   */
  function ServiceListItem(serviceInfo) {
    var listItem = new ExpandableListItem();
    listItem.__proto__ = ServiceListItem.prototype;

    listItem.info = serviceInfo;
    listItem.decorate();

    return listItem;
  }

  ServiceListItem.prototype = {
    __proto__: ExpandableListItem.prototype,

    /**
     * Decorates the element as a service list item. Creates layout and caches
     * references to the created header and fieldset.
     * @override
     */
    decorate: function() {
      this.classList.add('service-list-item');

      /** @private {!HTMLElement} */
      this.infoDiv_ = document.createElement('div');
      this.infoDiv_.classList.add('info-container');

      /** @private {!object_fieldset.ObjectFieldSet} */
      this.serviceFieldSet_ = object_fieldset.ObjectFieldSet();
      this.serviceFieldSet_.setPropertyDisplayNames(PROPERTY_NAMES);
      this.serviceFieldSet_.setObject({
        id: this.info.id,
        'uuid.uuid': this.info.uuid.uuid,
        is_primary: this.info.is_primary ? 'Primary' : 'Secondary',
      });

      // Create content for display in brief content container.
      var serviceHeaderText = document.createElement('div');
      serviceHeaderText.textContent = 'Service:';

      var serviceHeaderValue = document.createElement('div');
      serviceHeaderValue.textContent = this.info.uuid.uuid;

      var serviceHeader = document.createElement('div');
      serviceHeader.appendChild(serviceHeaderText);
      serviceHeader.appendChild(serviceHeaderValue);
      this.briefContent_.appendChild(serviceHeader);

      // Create content for display in expanded content container.
      var serviceInfoHeader = document.createElement('h4');
      serviceInfoHeader.textContent = 'Service Info';

      var serviceDiv = document.createElement('div');
      serviceDiv.classList.add('flex');
      serviceDiv.appendChild(this.serviceFieldSet_);

      this.infoDiv_.appendChild(serviceInfoHeader);
      this.infoDiv_.appendChild(serviceDiv);
      this.expandedContent_.appendChild(this.infoDiv_);
    },
  };

  /**
   * A list that displays ServiceListItems.
   * @constructor
   */
  var ServiceList = cr.ui.define('list');

  ServiceList.prototype = {
    __proto__: ExpandableList.prototype,

    /** @override */
    decorate: function() {
      ExpandableList.prototype.decorate.call(this);
      this.classList.add('service-list');
      this.setEmptyMessage('No Services Found');
    },

    /** @override */
    createItem: function(data) {
      return new ServiceListItem(data);
    },
  };

  return {
    ServiceList: ServiceList,
    ServiceListItem: ServiceListItem,
  };
});