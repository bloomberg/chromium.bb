// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// require: listselectionmodel.js

/**
 * @fileoverview This implements a list control.
 */

cr.define('cr.ui', function() {
  const ListSelectionModel = cr.ui.ListSelectionModel;
  const ArrayDataModel = cr.ui.ArrayDataModel;

  /**
   * Whether a mouse event is inside the element viewport. This will return
   * false if the mouseevent was generated over a border or a scrollbar.
   * @param {!HTMLElement} el The element to test the event with.
   * @param {!Event} e The mouse event.
   * @param {boolean} Whether the mouse event was inside the viewport.
   */
  function inViewport(el, e) {
    var rect = el.getBoundingClientRect();
    var x = e.clientX;
    var y = e.clientY;
    return x >= rect.left + el.clientLeft &&
           x < rect.left + el.clientLeft + el.clientWidth &&
           y >= rect.top + el.clientTop &&
           y < rect.top + el.clientTop + el.clientHeight;
  }

  /**
   * Creates an item (dataModel.item(0)) and measures its height.
   * @param {!List} list The list to create the item for.
   * @return {number} The height of the item, taking margins into account.
   */
  function measureItem(list) {
    var dataModel = list.dataModel;
    if (!dataModel || !dataModel.length)
      return 0;
    var item = list.createItem(dataModel.item(0));
    list.appendChild(item);
    var cs = getComputedStyle(item);
    var mt = parseFloat(cs.marginTop);
    var mb = parseFloat(cs.marginBottom);
    var h = item.offsetHeight;

    // Handle margin collapsing.
    if (mt < 0 && mb < 0) {
      h += Math.min(mt, mb);
    } else if (mt >= 0 && mb >= 0) {
      h += Math.max(mt, mb);
    } else {
      h += mt + mb;
    }

    list.removeChild(item);
    return Math.max(0, h);
  }

  function getComputedStyle(el) {
    return el.ownerDocument.defaultView.getComputedStyle(el);
  }

  /**
   * Creates a new list element.
   * @param {Object=} opt_propertyBag Optional properties.
   * @constructor
   * @extends {HTMLUListElement}
   */
  var List = cr.ui.define('list');

  List.prototype = {
    __proto__: HTMLUListElement.prototype,

    /**
     * The height of list items. This is lazily calculated the first time it is
     * needed.
     * @type {number}
     * @private
     */
    itemHeight_: 0,

    dataModel_: null,

    /**
     * The data model driving the list.
     * @type {ListDataModel}
     */
    set dataModel(dataModel) {
      if (this.dataModel_ != dataModel) {
        if (!this.boundHandleDataModelSplice_) {
          this.boundHandleDataModelSplice_ =
              cr.bind(this.handleDataModelSplice_, this);
          this.boundHandleDataModelChange_ =
              cr.bind(this.handleDataModelChange_, this);
        }

        if (this.dataModel_) {
          this.dataModel_.removeEventListener('splice',
                                              this.boundHandleDataModelSplice_);
          this.dataModel_.removeEventListener('change',
                                              this.boundHandleDataModelChange_);
        }

        this.dataModel_ = dataModel;

        this.cachedItems_ = {};
        this.selectionModel.clear();
        if (dataModel)
          this.selectionModel.adjust(0, 0, dataModel.length);

        if (this.dataModel_) {
          this.dataModel_.addEventListener('splice',
                                           this.boundHandleDataModelSplice_);
          this.dataModel_.addEventListener('change',
                                           this.boundHandleDataModelChange_);
        }

        this.redraw();
      }
    },

    get dataModel() {
      return this.dataModel_;
    },

    /**
     * The selection model to use.
     * @type {cr.ui.ListSelectionModel}
     */
    get selectionModel() {
      return this.selectionModel_;
    },
    set selectionModel(sm) {
      var oldSm = this.selectionModel_;
      if (oldSm == sm)
        return;

      if (!this.boundHandleOnChange_) {
        this.boundHandleOnChange_ = cr.bind(this.handleOnChange_, this);
        this.boundHandleLeadChange_ = cr.bind(this.handleLeadChange_, this);
      }

      if (oldSm) {
        oldSm.removeEventListener('change', this.boundHandleOnChange_);
        oldSm.removeEventListener('leadIndexChange',
                                  this.boundHandleLeadChange_);
      }

      this.selectionModel_ = sm;

      if (sm) {
        sm.addEventListener('change', this.boundHandleOnChange_);
        sm.addEventListener('leadIndexChange', this.boundHandleLeadChange_);
      }
    },

    /**
     * Convenience alias for selectionModel.selectedItem
     * @type {cr.ui.ListItem}
     */
    get selectedItem() {
      var dataModel = this.dataModel;
      if (dataModel) {
        var index = this.selectionModel.selectedIndex;
        if (index != -1)
          return dataModel.item(index);
      }
      return null;
    },
    set selectedItem(selectedItem) {
      var dataModel = this.dataModel;
      if (dataModel) {
        var index = this.dataModel.indexOf(selectedItem);
        this.selectionModel.selectedIndex = index;
      }
    },

    /**
     * Convenience alias for selectionModel.selectedItems
     * @type {!Array<cr.ui.ListItem>}
     */
    get selectedItems() {
      var indexes = this.selectionModel.selectedIndexes;
      var dataModel = this.dataModel;
      if (dataModel) {
        return indexes.map(function(i) {
          return dataModel.item(i);
        });
      }
      return [];
    },

    /**
     * The HTML elements representing the items. This is just all the element
     * children but subclasses may override this to filter out certain elements.
     * @type {HTMLCollection}
     */
    get items() {
      return this.children;
    },

    batchCount_: 0,

    /**
     * When making a lot of updates to the list, the code could be wrapped in
     * the startBatchUpdates and finishBatchUpdates to increase performance. Be
     * sure that the code will not return without calling endBatchUpdates or the
     * list will not be correctly updated.
     */
    startBatchUpdates: function() {
      this.batchCount_++;
    },

    /**
     * See startBatchUpdates.
     */
    endBatchUpdates: function() {
      this.batchCount_--;
      if (this.batchCount_ == 0)
        this.redraw();
    },

    /**
     * Initializes the element.
     */
    decorate: function() {
      // Add fillers.
      this.beforeFiller_ = this.ownerDocument.createElement('div');
      this.afterFiller_ = this.ownerDocument.createElement('div');
      this.beforeFiller_.className = 'spacer';
      this.afterFiller_.className = 'spacer';
      this.appendChild(this.beforeFiller_);
      this.appendChild(this.afterFiller_);

      var length = this.dataModel ? this.dataModel.length : 0;
      this.selectionModel = new ListSelectionModel(length);

      this.addEventListener('mousedown', this.handleMouseDownUp_);
      this.addEventListener('mouseup', this.handleMouseDownUp_);
      this.addEventListener('keydown', this.handleKeyDown);
      this.addEventListener('scroll', cr.bind(this.redraw, this));

      // Make list focusable
      if (!this.hasAttribute('tabindex'))
        this.tabIndex = 0;
    },

    /**
     * Callback for mousedown and mouseup events.
     * @param {Event} e The mouse event object.
     * @private
     */
    handleMouseDownUp_: function(e) {
      var target = e.target;

      // If the target was this element we need to make sure that the user did
      // not click on a border or a scrollbar.
      if (target == this && !inViewport(target, e))
        return;

      while (target && target.parentNode != this) {
        target = target.parentNode;
      }

      if (!target) {
        this.selectionModel.handleMouseDownUp(e, -1);
      } else {
        var cs = getComputedStyle(target);
        var top = target.offsetTop -
                  parseFloat(cs.marginTop);
        var index = Math.floor(top / this.itemHeight_);
        this.selectionModel.handleMouseDownUp(e, index);
      }
    },

    /**
     * Handle a keydown event.
     * @param {Event} e The keydown event.
     * @return {boolean} Whether the key event was handled.
     */
    handleKeyDown: function(e) {
      return this.selectionModel.handleKeyDown(e);
    },

    /**
     * Callback from the selection model. We dispatch {@code change} events
     * when the selection changes.
     * @param {!cr.Event} e Event with change info.
     * @private
     */
    handleOnChange_: function(ce) {
      ce.changes.forEach(function(change) {
        var listItem = this.getListItemByIndex(change.index);
        if (listItem)
          listItem.selected = change.selected;
      }, this);

      cr.dispatchSimpleEvent(this, 'change');
    },

    /**
     * Handles a change of the lead item from the selection model.
     * @property {Event} pe The property change event.
     * @private
     */
    handleLeadChange_: function(pe) {
      var element;
      if (pe.oldValue != -1) {
        if ((element = this.getListItemByIndex(pe.oldValue)))
          element.lead = false;
      }

      if (pe.newValue != -1) {
        this.scrollIndexIntoView(pe.newValue);
        if ((element = this.getListItemByIndex(pe.newValue)))
          element.lead = true;
      }
    },

    handleDataModelSplice_: function(e) {
      this.selectionModel.adjust(e.index, e.removed.length, e.added.length);
      // Remove the cache of everything above index.
      for (var index in this.cachedItems_) {
        if (index >= e.index)
          delete this.cachedItems_[index];
      }
      this.redraw();
    },

    handleDataModelChange_: function(e) {
      if (e.index >= this.firstIndex_ && e.index < this.lastIndex_) {
        delete this.cachedItems_;
        this.redraw();
      }
    },

    /**
     * Ensures that a given index is inside the viewport.
     * @param {number} index The index of the item to scroll into view.
     * @return {boolean} Whether any scrolling was needed.
     */
    scrollIndexIntoView: function(index) {
      var dataModel = this.dataModel;
      if (!dataModel || index < 0 || index >= dataModel.length)
        return false;

      var itemHeight = this.itemHeight_;
      var scrollTop = this.scrollTop;
      var top = index * itemHeight;

      if (top < scrollTop) {
        this.scrollTop = top;
        return true;
      } else {
        var clientHeight = this.clientHeight;
        var cs = getComputedStyle(this);
        var paddingY = parseInt(cs.paddingTop, 10) +
                       parseInt(cs.paddingBottom, 10);

        if (top + itemHeight > scrollTop + clientHeight - paddingY) {
          this.scrollTop = top + itemHeight - clientHeight + paddingY;
          return true;
        }
      }

      return false;
    },

    /**
     * @return {!ClientRect} The rect to use for the context menu.
     */
    getRectForContextMenu: function() {
      // TODO(arv): Add trait support so we can share more code between trees
      // and lists.
      var index = this.selectionModel.selectedIndex;
      var el = this.getListItemByIndex(index);
      if (el)
        return el.getBoundingClientRect();
      return this.getBoundingClientRect();
    },

    /**
     * Takes a value from the data model and finds the associated list item.
     * @param {*} value The value in the data model that we want to get the list
     *     item for.
     * @return {ListItem} The first found list item or null if not found.
     */
    getListItem: function(value) {
      var dataModel = this.dataModel;
      if (dataModel) {
        var index = dataModel.indexOf(value);
        return this.getListItemByIndex(index);
      }
      return null;
    },

    /**
     * Find the list item element at the given index.
     * @param {number} index The index of the list item to get.
     * @return {ListItem} The found list item or null if not found.
     */
    getListItemByIndex: function(index) {
      if (index < this.firstIndex_ || index >= this.lastIndex_)
        return null;

      return this.children[index - this.firstIndex_ + 1];
    },

    /**
     * Creates a new list item.
     * @param {*} value The value to use for the item.
     * @return {!ListItem} The newly created list item.
     */
    createItem: function(value) {
      return new cr.ui.ListItem({label: value});
    },

    /**
     * Redraws the viewport.
     */
    redraw: function() {
      if (this.batchCount_ != 0)
        return;

      var dataModel = this.dataModel;
      if (!dataModel) {
        this.textContent = '';
        return;
      }

      console.time('redraw');
      var scrollTop = this.scrollTop;
      var clientHeight = this.clientHeight;

      if (!this.itemHeight_) {
        this.itemHeight_ = measureItem(this);
      }

      var itemHeight = this.itemHeight_;

      // We cache the list items since creating the DOM nodes is the most
      // expensive part of redrawing.
      var cachedItems = this.cachedItems_ || {};
      var newCachedItems = {};

      var desiredScrollHeight = dataModel.length * itemHeight;

      var firstIndex = Math.floor(scrollTop / itemHeight);
      var itemsInViewPort = Math.min(dataModel.length - firstIndex,
          Math.ceil((scrollTop + clientHeight - firstIndex * itemHeight) /
                    itemHeight));
      var lastIndex = firstIndex + itemsInViewPort;

      this.textContent = '';

      var oldFirstIndex = this.firstIndex_ || 0;
      var oldLastIndex = this.lastIndex_ || 0;

      this.beforeFiller_.style.height = firstIndex * itemHeight + 'px';
      this.appendChild(this.beforeFiller_);

      var sm = this.selectionModel;
      var leadIndex = sm.leadIndex;

      for (var y = firstIndex; y < lastIndex; y++) {
        var dataItem = dataModel.item(y);
        var listItem = cachedItems[y] || this.createItem(dataItem);
        if (y == leadIndex) {
          listItem.lead = true;
        }
        if (sm.getIndexSelected(y)) {
          listItem.selected = true;
        }
        this.appendChild(listItem);
        newCachedItems[y] = listItem;
      }

      this.afterFiller_.style.height =
          (dataModel.length - firstIndex - itemsInViewPort) * itemHeight + 'px';
      this.appendChild(this.afterFiller_);

      this.firstIndex_ = firstIndex;
      this.lastIndex_ = lastIndex;

      this.cachedItems_ = newCachedItems;

      console.timeEnd('redraw');
    },

    /**
     * Redraws a single item
     * @param {number} index The row index to redraw.
     */
    redrawItem: function(index) {
      if (index >= this.firstIndex_ && index < this.lastIndex_) {
        delete this.cachedItems_[index];
        this.redraw();
      }
    }
  };

  return {
    List: List
  }
});
