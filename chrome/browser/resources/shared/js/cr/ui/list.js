// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// require: listselectionmodel.js

/**
 * @fileoverview This implements a list control.
 */

cr.define('cr.ui', function() {
  const ListSelectionModel = cr.ui.ListSelectionModel;
  const ListSelectionController = cr.ui.ListSelectionController;
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
   * @param {ListItem=} opt_item The list item to use to do the measuring. If
   *     this is not provided an item will be created based on the first value
   *     in the model.
   * @return {number} The height of the item, taking margins into account.
   */
  function measureItem(list, opt_item) {
    var dataModel = list.dataModel;
    if (!dataModel || !dataModel.length)
      return 0;
    var item = opt_item || list.createItem(dataModel.item(0));
    if (!opt_item)
      list.appendChild(item);

    var rect = item.getBoundingClientRect();
    var cs = getComputedStyle(item);
    var mt = parseFloat(cs.marginTop);
    var mb = parseFloat(cs.marginBottom);
    var h = rect.height;

    // Handle margin collapsing.
    if (mt < 0 && mb < 0) {
      h += Math.min(mt, mb);
    } else if (mt >= 0 && mb >= 0) {
      h += Math.max(mt, mb);
    } else {
      h += mt + mb;
    }

    if (!opt_item)
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

    /**
     * Whether or not the list is autoexpanding. If true, the list resizes
     * its height to accomadate all children.
     * @type {boolean}
     * @private
     */
    autoExpands_: false,

    dataModel_: null,

    /**
     * The data model driving the list.
     * @type {ListDataModel}
     */
    set dataModel(dataModel) {
      if (this.dataModel_ != dataModel) {
        if (!this.boundHandleDataModelSplice_) {
          this.boundHandleDataModelSplice_ =
              this.handleDataModelSplice_.bind(this);
          this.boundHandleDataModelChange_ =
              this.handleDataModelChange_.bind(this);
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
        this.boundHandleOnChange_ = this.handleOnChange_.bind(this);
        this.boundHandleLeadChange_ = this.handleLeadChange_.bind(this);
      }

      if (oldSm) {
        oldSm.removeEventListener('change', this.boundHandleOnChange_);
        oldSm.removeEventListener('leadIndexChange',
                                  this.boundHandleLeadChange_);
      }

      this.selectionModel_ = sm;
      this.selectionController_ = this.createSelectionController(sm);

      if (sm) {
        sm.addEventListener('change', this.boundHandleOnChange_);
        sm.addEventListener('leadIndexChange', this.boundHandleLeadChange_);
      }
    },

    /**
     * Whether or not the list auto-expands.
     * @type {boolean}
     */
    get autoExpands() {
      return this.autoExpands_;
    },
    set autoExpands(autoExpands) {
      if (this.autoExpands_ == autoExpands)
        return;
      this.autoExpands_ = autoExpands;
      this.redraw();
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
     * The HTML elements representing the items. This is just all the list item
     * children but subclasses may override this to filter out certain elements.
     * @type {HTMLCollection}
     */
    get items() {
      return Array.prototype.filter.call(this.children, function(child) {
        return !child.classList.contains('spacer');
      });
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

      this.addEventListener('dblclick', this.handleDoubleClick_);
      this.addEventListener('mousedown', this.handleMouseDownUp_);
      this.addEventListener('mouseup', this.handleMouseDownUp_);
      this.addEventListener('keydown', this.handleKeyDown);
      this.addEventListener('focus', this.handleElementFocus_, true);
      this.addEventListener('blur', this.handleElementBlur_, true);
      this.addEventListener('scroll', this.redraw.bind(this));

      // Make list focusable
      if (!this.hasAttribute('tabindex'))
        this.tabIndex = 0;
    },

    /**
     * @return {number} The height of an item, measuring it if necessary.
     * @private
     */
    getItemHeight_: function() {
      if (!this.itemHeight_)
        this.itemHeight_ = measureItem(this);
      return this.itemHeight_;
    },

    /**
     * Callback for the double click event.
     * @param {Event} e The mouse event object.
     * @private
     */
    handleDoubleClick_: function(e) {
      if (this.disabled)
        return;

      var target = this.getListItemAncestor(e.target);
      var index = target ? this.getIndexOfListItem(target) : -1;
      this.activateItemAtIndex(index);
    },

    /**
     * Callback for mousedown and mouseup events.
     * @param {Event} e The mouse event object.
     * @private
     */
    handleMouseDownUp_: function(e) {
      if (this.disabled)
        return;

      var target = e.target;

      // If the target was this element we need to make sure that the user did
      // not click on a border or a scrollbar.
      if (target == this && !inViewport(target, e))
        return;

      target = this.getListItemAncestor(target);

      var index = target ? this.getIndexOfListItem(target) : -1;
      this.selectionController_.handleMouseDownUp(e, index);
    },

    /**
     * Called when an element in the list is focused. Marks the list as having
     * a focused element, and dispatches an event if it didn't have focus.
     * @param {Event} e The focus event.
     * @private
     */
    handleElementFocus_: function(e) {
      if (!this.hasElementFocus) {
        this.hasElementFocus = true;
        // Force styles based on hasElementFocus to take effect.
        this.forceRepaint_();
      }
    },

    /**
     * Called when an element in the list is blurred. If focus moves outside
     * the list, marks the list as no longer having focus and dispatches an
     * event.
     * @param {Event} e The blur event.
     * @private
     */
    handleElementBlur_: function(e) {
      // When the blur event happens we do not know who is getting focus so we
      // delay this a bit until we know if the new focus node is outside the
      // list.
      var list = this;
      var doc = e.target.ownerDocument;
      window.setTimeout(function() {
        var activeElement = doc.activeElement;
        if (!list.contains(activeElement)) {
          list.hasElementFocus = false;
          // Force styles based on hasElementFocus to take effect.
          list.forceRepaint_();
        }
      });
    },

    /**
     * Forces a repaint of the list. Changing custom attributes, even if there
     * are style rules depending on them, doesn't cause a repaint
     * (<https://bugs.webkit.org/show_bug.cgi?id=12519>), so this can be called
     * to force the list to repaint.
     * @private
     */
    forceRepaint_: function(e) {
      var dummyElement = document.createElement('div');
      this.appendChild(dummyElement);
      this.removeChild(dummyElement);
    },

    /**
     * Returns the list item element containing the given element, or null if
     * it doesn't belong to any list item element.
     * @param {HTMLElement} element The element.
     * @return {ListItem} The list item containing |element|, or null.
     */
    getListItemAncestor: function(element) {
      var container = element;
      while (container && container.parentNode != this) {
        container = container.parentNode;
      }
      return container;
    },

    /**
     * Handle a keydown event.
     * @param {Event} e The keydown event.
     * @return {boolean} Whether the key event was handled.
     */
    handleKeyDown: function(e) {
      if (this.disabled)
        return;

      return this.selectionController_.handleKeyDown(e);
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

      var itemHeight = this.getItemHeight_();
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
     * Find the index of the given list item element.
     * @param {ListItem} item The list item to get the index of.
     * @return {number} The index of the list item, or -1 if not found.
     */
    getIndexOfListItem: function(item) {
      var paddingTop = parseFloat(getComputedStyle(this).paddingTop);
      var cs = getComputedStyle(item);
      var top = item.offsetTop - parseFloat(cs.marginTop) - paddingTop;
      var itemHeight = this.getItemHeight_();
      var index = Math.floor((top + itemHeight / 2) / itemHeight);
      var childIndex = index - this.firstIndex_ + 1;
      if (childIndex >= 0 && childIndex < this.children.length &&
          this.children[childIndex] == item) {
        return index;
      }
      return -1;
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
     * Creates the selection controller to use internally.
     * @param {cr.ui.ListSelectionModel} sm The underlying selection model.
     * @return {!cr.ui.ListSelectionModel} The newly created selection
     *     controller.
     */
    createSelectionController: function(sm) {
      return new ListSelectionController(sm);
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

      var itemHeight = this.getItemHeight_();

      // We cache the list items since creating the DOM nodes is the most
      // expensive part of redrawing.
      var cachedItems = this.cachedItems_ || {};
      var newCachedItems = {};

      var desiredScrollHeight = dataModel.length * itemHeight;

      var autoExpands = this.autoExpands_
      var firstIndex = autoExpands ? 0 : Math.floor(scrollTop / itemHeight);
      var itemsInViewPort = autoExpands ? dataModel.length : Math.min(
          dataModel.length - firstIndex,
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
      var listItem;

      for (var y = firstIndex; y < lastIndex; y++) {
        var dataItem = dataModel.item(y);
        listItem = cachedItems[y] || this.createItem(dataItem);
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

      // Measure again in case the item height has change due to a page zoom.
      //
      // The measure above is only done the first time but this measure is done
      // after every redraw. It is done in a timeout so it will not trigger
      // a reflow (which made the redraw speed 3 times slower on my system).
      // By using a timeout the measuring will happen later when there is no
      // need for a reflow.
      if (listItem) {
        var list = this;
        window.setTimeout(function() {
          if (listItem.parentNode == list)
            list.itemHeight_ = measureItem(list, listItem);
        });
      }
    },

    /**
     * Redraws a single item.
     * @param {number} index The row index to redraw.
     */
    redrawItem: function(index) {
      if (index >= this.firstIndex_ && index < this.lastIndex_) {
        delete this.cachedItems_[index];
        this.redraw();
      }
    },

    /**
     * Called when a list item is activated, currently only by a double click
     * event.
     * @param {number} index The index of the activated item.
     */
    activateItemAtIndex: function(index) {
    },
  };

  cr.defineProperty(List, 'disabled', cr.PropertyKind.BOOL_ATTR);

  /**
   * Whether the list or one of its descendents has focus. This is necessary
   * because list items can contain controls that can be focused, and for some
   * purposes (e.g., styling), the list can still be conceptually focused at
   * that point even though it doesn't actually have the page focus.
   */
  cr.defineProperty(List, 'hasElementFocus', cr.PropertyKind.BOOL_ATTR);

  return {
    List: List
  }
});
