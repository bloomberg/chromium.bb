// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options.internet', function() {
  /**
   * Creates a new network list div.
   * @param {Object=} opt_propertyBag Optional properties.
   * @constructor
   * @extends {HTMLDivElement}
   */
  var CellularPlanElement = cr.ui.define('div');

  CellularPlanElement.prototype = {
    __proto__: HTMLDivElement.prototype,

    /** @inheritDoc */
    decorate: function() {
    },

    /**
     * Loads given network list.
     * @param {Array} networks An array of network object.
     */
    load: function(plans) {
      this.textContent = '';
      if (!plans || !plans.length) {
        var noplansDiv = this.ownerDocument.createElement('div');
        noplansDiv.textContent = localStrings.getString('noPlansFound');
        this.appendChild(detailsTable);
      } else {
        for (var i = 0; i < plans.length; ++i) {
          this.appendChild(new CellularPlanItem(i, plans[i]));
        }
      }
    }
  };

  /**
   * Creates a new network item.
   * @param {Object} network The network this represents.
   * @constructor
   * @extends {HTMLDivElement}
   */
  function CellularPlanItem(idx, plan) {
    var el = cr.doc.createElement('div');
    el.data = {
      idx: idx,
      planType: plan.planType,
      name: plan.name,
      planSummary: plan.planSummary,
      dataRemaining: plan.dataRemaining,
      planExpires: plan.planExpires,
      warning: plan.warning
    };
    CellularPlanItem.decorate(el);
    return el;
  }


  /**
   * Decorates an element as a network item.
   * @param {!HTMLElement} el The element to decorate.
   */
  CellularPlanItem.decorate = function(el) {
    el.__proto__ = CellularPlanItem.prototype;
    el.decorate();
  };

  CellularPlanItem.prototype = {
    __proto__: HTMLDivElement.prototype,

    /** @inheritDoc */
    decorate: function() {
      this.className = 'cellular-plan';
      var detailsTable = this.createTable_('details-plan-table',
                                           'option-control-table');
      this.addRow_(detailsTable, 'plan-details-info',
          'option-name', 'planSummary', this.data.planSummary);
      this.addRow_(detailsTable, 'plan-details-info',
          'option-name', null, localStrings.getString('planName'),
          'option-value', 'planName', this.data.name);
      this.addRow_(detailsTable, 'plan-details-info',
        'option-name', null, localStrings.getString('dataRemaining'),
        'option-value', 'dataRemaining', this.data.dataRemaining);
      this.addRow_(detailsTable, 'plan-details-info',
        'option-name', null, localStrings.getString('planExpires'),
        'option-value', 'dataRemaining', this.data.planExpires);
      if (this.data.warning && this.data.warning != "") {
        this.addRow_(detailsTable, 'plan-details-info',
            'option-name', 'planWarning', this.data.warning);
      }
      this.appendChild(detailsTable);
      this.appendChild(this.ownerDocument.createElement('hr'));
    },

    createTable_: function(tableId, tableClass) {
      var table = this.ownerDocument.createElement('table');
      table.id = tableId;
      table.className = tableClass;
      return table;
    },

    addRow_: function(table, rowClass, col1Class, col1Id, col1Value,
            col2Class, col2Id, col2Value) {
      var row = this.ownerDocument.createElement('tr');
      if (rowClass)
        row.className = rowClass;
      var col1 = this.ownerDocument.createElement('td');
      col1.className = col1Class;
      if (col1Id)
        col1.id = col1Id;
      col1.textContent = col1Value;
      if (!col2Class)
        col1.setAttribute('colspan','2');
      row.appendChild(col1);
      if (col2Class) {
        var col2 = this.ownerDocument.createElement('td');
        col2.className = col2Class;
        if (col2Id)
          col2.id = col2Id;
        col2.textContent = col2Value;
        row.appendChild(col2);
      }
      table.appendChild(row);
    }
  };

  return {
    CellularPlanElement: CellularPlanElement
  };
});
