// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(gbillock): refactor this together with CookiesList once we have
// a better sense from UX design what it'll look like and so what'll be shared.
cr.define('options', function() {
  const DeletableItemList = options.DeletableItemList;
  const DeletableItem = options.DeletableItem;
  const ArrayDataModel = cr.ui.ArrayDataModel;
  const ListSingleSelectionModel = cr.ui.ListSingleSelectionModel;
  const localStrings = new LocalStrings();

  /**
   * Returns the item's height, like offsetHeight but such that it works better
   * when the page is zoomed. See the similar calculation in @{code cr.ui.List}.
   * This version also accounts for the animation done in this file.
   * @param {Element} item The item to get the height of.
   * @return {number} The height of the item, calculated with zooming in mind.
   */
  function getItemHeight(item) {
    var height = item.style.height;
    // Use the fixed animation target height if set, in case the element is
    // currently being animated and we'd get an intermediate height below.
    if (height && height.substr(-2) == 'px')
      return parseInt(height.substr(0, height.length - 2));
    return item.getBoundingClientRect().height;
  }

  // Map of parent pathIDs to node objects.
  var parentLookup = {};

  // Pending requests for child information.
  var lookupRequests = {};

  /**
   * Creates a new list item for intent service data. Note that these are
   * created and destroyed lazily as they scroll into and out of view,
   * so they must be stateless. We cache the expanded item in
   * @{code IntentsList} though, so it can keep state.
   * (Mostly just which item is selected.)
   *
   * @param {Object} origin Data used to create an intents list item.
   * @param {IntentsList} list The list that will contain this item.
   * @constructor
   * @extends {DeletableItem}
   */
  function IntentsListItem(origin, list) {
    var listItem = new DeletableItem(null);
    listItem.__proto__ = IntentsListItem.prototype;

    listItem.origin = origin;
    listItem.list = list;
    listItem.decorate();

    // This hooks up updateOrigin() to the list item, makes the top-level
    // tree nodes (i.e., origins) register their IDs in parentLookup, and
    // causes them to request their children if they have none. Note that we
    // have special logic in the setter for the parent property to make sure
    // that we can still garbage collect list items when they scroll out of
    // view, even though it appears that we keep a direct reference.
    if (origin) {
      origin.parent = listItem;
      origin.updateOrigin();
    }

    return listItem;
  }

  IntentsListItem.prototype = {
    __proto__: DeletableItem.prototype,

    /** @inheritDoc */
    decorate: function() {
      this.siteChild = this.ownerDocument.createElement('div');
      this.siteChild.className = 'intents-site';
      this.dataChild = this.ownerDocument.createElement('div');
      this.dataChild.className = 'intents-data';
      this.itemsChild = this.ownerDocument.createElement('div');
      this.itemsChild.className = 'intents-items';
      this.infoChild = this.ownerDocument.createElement('div');
      this.infoChild.className = 'intents-details';
      this.infoChild.hidden = true;
      var remove = this.ownerDocument.createElement('button');
      remove.textContent = localStrings.getString('removeIntent');
      remove.onclick = this.removeIntent_.bind(this);
      this.infoChild.appendChild(remove);
      var content = this.contentElement;
      content.appendChild(this.siteChild);
      content.appendChild(this.dataChild);
      content.appendChild(this.itemsChild);
      this.itemsChild.appendChild(this.infoChild);
      if (this.origin && this.origin.data) {
        this.siteChild.textContent = this.origin.data.site;
        this.siteChild.setAttribute('title', this.origin.data.site);
      }
      this.itemList_ = [];
    },

    /** @type {boolean} */
    get expanded() {
      return this.expanded_;
    },
    set expanded(expanded) {
      if (this.expanded_ == expanded)
        return;
      this.expanded_ = expanded;
      if (expanded) {
        var oldExpanded = this.list.expandedItem;
        this.list.expandedItem = this;
        this.updateItems_();
        if (oldExpanded)
          oldExpanded.expanded = false;
        this.classList.add('show-items');
        this.dataChild.hidden = true;
      } else {
        if (this.list.expandedItem == this) {
          this.list.leadItemHeight = 0;
          this.list.expandedItem = null;
        }
        this.style.height = '';
        this.itemsChild.style.height = '';
        this.classList.remove('show-items');
        this.dataChild.hidden = false;
      }
    },

    /**
     * The callback for the "remove" button shown when an item is selected.
     * Requests that the currently selected intent service be removed.
     * @private
     */
    removeIntent_: function() {
      if (this.selectedIndex_ >= 0) {
        var item = this.itemList_[this.selectedIndex_];
        if (item && item.node)
          chrome.send('removeIntent', [item.node.pathId]);
      }
    },

    /**
     * Disable animation within this intents list item, in preparation for
     * making changes that will need to be animated. Makes it possible to
     * measure the contents without displaying them, to set animation targets.
     * @private
     */
    disableAnimation_: function() {
      this.itemsHeight_ = getItemHeight(this.itemsChild);
      this.classList.add('measure-items');
    },

    /**
     * Enable animation after changing the contents of this intents list item.
     * See @{code disableAnimation_}.
     * @private
     */
    enableAnimation_: function() {
      if (!this.classList.contains('measure-items'))
        this.disableAnimation_();
      this.itemsChild.style.height = '';
      // This will force relayout in order to calculate the new heights.
      var itemsHeight = getItemHeight(this.itemsChild);
      var fixedHeight = getItemHeight(this) + itemsHeight - this.itemsHeight_;
      this.itemsChild.style.height = this.itemsHeight_ + 'px';
      // Force relayout before enabling animation, so that if we have
      // changed things since the last layout, they will not be animated
      // during subsequent layouts.
      this.itemsChild.offsetHeight;
      this.classList.remove('measure-items');
      this.itemsChild.style.height = itemsHeight + 'px';
      this.style.height = fixedHeight + 'px';
      if (this.expanded)
        this.list.leadItemHeight = fixedHeight;
    },

    /**
     * Updates the origin summary to reflect changes in its items.
     * Both IntentsListItem and IntentsTreeNode implement this API.
     * This implementation scans the descendants to update the text.
     */
    updateOrigin: function() {
      console.log('IntentsListItem.updateOrigin');
      var text = '';
      for (var i = 0; i < this.origin.children.length; ++i) {
        if (text.length > 0)
          text += ', ' + this.origin.children[i].data.action;
        else
          text = this.origin.children[i].data.action;
      }
      this.dataChild.textContent = text;

      if (this.expanded)
        this.updateItems_();
    },

    /**
     * Updates the items section to reflect changes, animating to the new state.
     * Removes existing contents and calls @{code IntentsTreeNode.createItems}.
     * @private
     */
    updateItems_: function() {
      this.disableAnimation_();
      this.itemsChild.textContent = '';
      this.infoChild.hidden = true;
      this.selectedIndex_ = -1;
      this.itemList_ = [];
      if (this.origin)
        this.origin.createItems(this);
      this.itemsChild.appendChild(this.infoChild);
      this.enableAnimation_();
    },

    /**
     * Append a new intents node "bubble" to this list item.
     * @param {IntentsTreeNode} node The intents node to add a bubble for.
     * @param {Element} div The DOM element for the bubble itself.
     * @return {number} The index the bubble was added at.
     */
    appendItem: function(node, div) {
      this.itemList_.push({node: node, div: div});
      this.itemsChild.appendChild(div);
      return this.itemList_.length - 1;
    },

    /**
     * The currently selected intents node ("intents bubble") index.
     * @type {number}
     * @private
     */
    selectedIndex_: -1,

    /**
     * Get the currently selected intents node ("intents bubble") index.
     * @type {number}
     */
    get selectedIndex() {
      return this.selectedIndex_;
    },

    /**
     * Set the currently selected intents node ("intents bubble") index to
     * @{code itemIndex}, unselecting any previously selected node first.
     * @param {number} itemIndex The index to set as the selected index.
     * TODO: KILL THIS
     */
    set selectedIndex(itemIndex) {
      // Get the list index up front before we change anything.
      var index = this.list.getIndexOfListItem(this);
      // Unselect any previously selected item.
      if (this.selectedIndex_ >= 0) {
        var item = this.itemList_[this.selectedIndex_];
        if (item && item.div)
          item.div.removeAttribute('selected');
      }
      // Special case: decrementing -1 wraps around to the end of the list.
      if (itemIndex == -2)
        itemIndex = this.itemList_.length - 1;
      // Check if we're going out of bounds and hide the item details.
      if (itemIndex < 0 || itemIndex >= this.itemList_.length) {
        this.selectedIndex_ = -1;
        this.disableAnimation_();
        this.infoChild.hidden = true;
        this.enableAnimation_();
        return;
      }
      // Set the new selected item and show the item details for it.
      this.selectedIndex_ = itemIndex;
      this.itemList_[itemIndex].div.setAttribute('selected', '');
      this.disableAnimation_();
      this.infoChild.hidden = false;
      this.enableAnimation_();
      // If we're near the bottom of the list this may cause the list item to go
      // beyond the end of the visible area. Fix it after the animation is done.
      var list = this.list;
      window.setTimeout(function() { list.scrollIndexIntoView(index); }, 150);
    },
  };

  /**
   * {@code IntentsTreeNode}s mirror the structure of the intents tree lazily,
   * and contain all the actual data used to generate the
   * {@code IntentsListItem}s.
   * @param {Object} data The data object for this node.
   * @constructor
   */
  function IntentsTreeNode(data) {
    this.data = data;
    this.children = [];
  }

  IntentsTreeNode.prototype = {
    /**
     * Insert an intents tree node at the given index.
     * Both IntentsList and IntentsTreeNode implement this API.
     * @param {Object} data The data object for the node to add.
     * @param {number} index The index at which to insert the node.
     */
    insertAt: function(data, index) {
      console.log('IntentsTreeNode.insertAt adding ' +
                  JSON.stringify(data) + ' at ' + index);
      var child = new IntentsTreeNode(data);
      this.children.splice(index, 0, child);
      child.parent = this;
      this.updateOrigin();
    },

    /**
     * Remove an intents tree node from the given index.
     * Both IntentsList and IntentsTreeNode implement this API.
     * @param {number} index The index of the tree node to remove.
     */
    remove: function(index) {
      if (index < this.children.length) {
        this.children.splice(index, 1);
        this.updateOrigin();
      }
    },

    /**
     * Clears all children.
     * Both IntentsList and IntentsTreeNode implement this API.
     * It is used by IntentsList.loadChildren().
     */
    clear: function() {
      // We might leave some garbage in parentLookup for removed children.
      // But that should be OK because parentLookup is cleared when we
      // reload the tree.
      this.children = [];
      this.updateOrigin();
    },

    /**
     * The counter used by startBatchUpdates() and endBatchUpdates().
     * @type {number}
     */
    batchCount_: 0,

    /**
     * See cr.ui.List.startBatchUpdates().
     * Both IntentsList (via List) and IntentsTreeNode implement this API.
     */
    startBatchUpdates: function() {
      this.batchCount_++;
    },

    /**
     * See cr.ui.List.endBatchUpdates().
     * Both IntentsList (via List) and IntentsTreeNode implement this API.
     */
    endBatchUpdates: function() {
      if (!--this.batchCount_)
        this.updateOrigin();
    },

    /**
     * Requests updating the origin summary to reflect changes in this item.
     * Both IntentsListItem and IntentsTreeNode implement this API.
     */
    updateOrigin: function() {
      if (!this.batchCount_ && this.parent)
        this.parent.updateOrigin();
    },

    /**
     * Create the intents services rows for this node.
     * Append the rows to @{code item}.
     * @param {IntentsListItem} item The intents list item to create items in.
     */
    createItems: function(item) {
      if (this.children.length > 0) {
        for (var i = 0; i < this.children.length; ++i)
          this.children[i].createItems(item);
      } else if (this.data && !this.data.hasChildren) {
        var div = item.ownerDocument.createElement('div');
        div.className = 'intents-item';
        // Help out screen readers and such: this is a clickable thing.
        div.setAttribute('role', 'button');

        var divAction = item.ownerDocument.createElement('div');
        divAction.className = 'intents-item-action';
        divAction.textContent = this.data.action;
        div.appendChild(divAction);

        var divTypes = item.ownerDocument.createElement('div');
        divTypes.className = 'intents-item-types';
        var text = "";
        for (var i = 0; i < this.data.types.length; ++i) {
          if (text != "")
            text += ", ";
          text += this.data.types[i];
        }
        divTypes.textContent = text;
        div.appendChild(divTypes);

        var divUrl = item.ownerDocument.createElement('div');
        divUrl.className = 'intents-item-url';
        divUrl.textContent = this.data.url;
        div.appendChild(divUrl);

        var index = item.appendItem(this, div);
        div.onclick = function() {
          if (item.selectedIndex == index)
            item.selectedIndex = -1;
          else
            item.selectedIndex = index;
        };
      }
    },

    /**
     * The parent of this intents tree node.
     * @type {?IntentsTreeNode|IntentsListItem}
     */
    get parent(parent) {
      // See below for an explanation of this special case.
      if (typeof this.parent_ == 'number')
        return this.list_.getListItemByIndex(this.parent_);
      return this.parent_;
    },
    set parent(parent) {
      if (parent == this.parent)
        return;

      if (parent instanceof IntentsListItem) {
        // If the parent is to be a IntentsListItem, then we keep the reference
        // to it by its containing list and list index, rather than directly.
        // This allows the list items to be garbage collected when they scroll
        // out of view (except the expanded item, which we cache). This is
        // transparent except in the setter and getter, where we handle it.
        this.parent_ = parent.listIndex;
        this.list_ = parent.list;
        parent.addEventListener('listIndexChange',
                                this.parentIndexChanged_.bind(this));
      } else {
        this.parent_ = parent;
      }


      if (parent)
        parentLookup[this.pathId] = this;
      else
        delete parentLookup[this.pathId];

      if (this.data && this.data.hasChildren &&
          !this.children.length && !lookupRequests[this.pathId]) {
        console.log('SENDING loadIntents');
        lookupRequests[this.pathId] = true;
        chrome.send('loadIntents', [this.pathId]);
      }
    },

    /**
     * Called when the parent is a IntentsListItem whose index has changed.
     * See the code above that avoids keeping a direct reference to
     * IntentsListItem parents, to allow them to be garbage collected.
     * @private
     */
    parentIndexChanged_: function(event) {
      if (typeof this.parent_ == 'number') {
        this.parent_ = event.newValue;
        // We set a timeout to update the origin, rather than doing it right
        // away, because this callback may occur while the list items are
        // being repopulated following a scroll event. Calling updateOrigin()
        // immediately could trigger relayout that would reset the scroll
        // position within the list, among other things.
        window.setTimeout(this.updateOrigin.bind(this), 0);
      }
    },

    /**
     * The intents tree path id.
     * @type {string}
     */
    get pathId() {
      var parent = this.parent;
      if (parent && parent instanceof IntentsTreeNode)
        return parent.pathId + ',' + this.data.action;
      return this.data.site;
    },
  };

  /**
   * Creates a new intents list.
   * @param {Object=} opt_propertyBag Optional properties.
   * @constructor
   * @extends {DeletableItemList}
   */
  var IntentsList = cr.ui.define('list');

  IntentsList.prototype = {
    __proto__: DeletableItemList.prototype,

    /** @inheritDoc */
    decorate: function() {
      DeletableItemList.prototype.decorate.call(this);
      this.classList.add('intents-list');
      this.data_ = [];
      this.dataModel = new ArrayDataModel(this.data_);
      this.addEventListener('keydown', this.handleKeyLeftRight_.bind(this));
      var sm = new ListSingleSelectionModel();
      sm.addEventListener('change', this.cookieSelectionChange_.bind(this));
      sm.addEventListener('leadIndexChange', this.cookieLeadChange_.bind(this));
      this.selectionModel = sm;
    },

    /**
     * Handles key down events and looks for left and right arrows, then
     * dispatches to the currently expanded item, if any.
     * @param {Event} e The keydown event.
     * @private
     */
    handleKeyLeftRight_: function(e) {
      var id = e.keyIdentifier;
      if ((id == 'Left' || id == 'Right') && this.expandedItem) {
        var cs = this.ownerDocument.defaultView.getComputedStyle(this);
        var rtl = cs.direction == 'rtl';
        if ((!rtl && id == 'Left') || (rtl && id == 'Right'))
          this.expandedItem.selectedIndex--;
        else
          this.expandedItem.selectedIndex++;
        this.scrollIndexIntoView(this.expandedItem.listIndex);
        // Prevent the page itself from scrolling.
        e.preventDefault();
      }
    },

    /**
     * Called on selection model selection changes.
     * @param {Event} ce The selection change event.
     * @private
     */
    cookieSelectionChange_: function(ce) {
      ce.changes.forEach(function(change) {
          var listItem = this.getListItemByIndex(change.index);
          if (listItem) {
            if (!change.selected) {
              // We set a timeout here, rather than setting the item unexpanded
              // immediately, so that if another item gets set expanded right
              // away, it will be expanded before this item is unexpanded. It
              // will notice that, and unexpand this item in sync with its own
              // expansion. Later, this callback will end up having no effect.
              window.setTimeout(function() {
                if (!listItem.selected || !listItem.lead)
                  listItem.expanded = false;
              }, 0);
            } else if (listItem.lead) {
              listItem.expanded = true;
            }
          }
        }, this);
    },

    /**
     * Called on selection model lead changes.
     * @param {Event} pe The lead change event.
     * @private
     */
    cookieLeadChange_: function(pe) {
      if (pe.oldValue != -1) {
        var listItem = this.getListItemByIndex(pe.oldValue);
        if (listItem) {
          // See cookieSelectionChange_ above for why we use a timeout here.
          window.setTimeout(function() {
            if (!listItem.lead || !listItem.selected)
              listItem.expanded = false;
          }, 0);
        }
      }
      if (pe.newValue != -1) {
        var listItem = this.getListItemByIndex(pe.newValue);
        if (listItem && listItem.selected)
          listItem.expanded = true;
      }
    },

    /**
     * The currently expanded item. Used by IntentsListItem above.
     * @type {?IntentsListItem}
     */
    expandedItem: null,

    // from cr.ui.List
    /** @inheritDoc */
    createItem: function(data) {
      // We use the cached expanded item in order to allow it to maintain some
      // state (like its fixed height, and which bubble is selected).
      if (this.expandedItem && this.expandedItem.origin == data)
        return this.expandedItem;
      return new IntentsListItem(data, this);
    },

    // from options.DeletableItemList
    /** @inheritDoc */
    deleteItemAtIndex: function(index) {
      var item = this.data_[index];
      if (item) {
        var pathId = item.pathId;
        if (pathId)
          chrome.send('removeIntent', [pathId]);
      }
    },

    /**
     * Insert an intents tree node at the given index.
     * Both IntentsList and IntentsTreeNode implement this API.
     * @param {Object} data The data object for the node to add.
     * @param {number} index The index at which to insert the node.
     */
    insertAt: function(data, index) {
      this.dataModel.splice(index, 0, new IntentsTreeNode(data));
    },

    /**
     * Remove an intents tree node from the given index.
     * Both IntentsList and IntentsTreeNode implement this API.
     * @param {number} index The index of the tree node to remove.
     */
    remove: function(index) {
      if (index < this.data_.length)
        this.dataModel.splice(index, 1);
    },

    /**
     * Clears the list.
     * Both IntentsList and IntentsTreeNode implement this API.
     * It is used by IntentsList.loadChildren().
     */
    clear: function() {
      parentLookup = {};
      this.data_ = [];
      this.dataModel = new ArrayDataModel(this.data_);
      this.redraw();
    },

    /**
     * Add tree nodes by given parent.
     * Note: this method will be O(n^2) in the general case. Use it only to
     * populate an empty parent or to insert single nodes to avoid this.
     * @param {Object} parent The parent node.
     * @param {number} start Start index of where to insert nodes.
     * @param {Array} nodesData Nodes data array.
     * @private
     */
    addByParent_: function(parent, start, nodesData) {
      if (!parent)
        return;

      parent.startBatchUpdates();
      for (var i = 0; i < nodesData.length; ++i)
        parent.insertAt(nodesData[i], start + i);
      parent.endBatchUpdates();

      cr.dispatchSimpleEvent(this, 'change');
    },

    /**
     * Add tree nodes by parent id.
     * This is used by intents_view.js.
     * Note: this method will be O(n^2) in the general case. Use it only to
     * populate an empty parent or to insert single nodes to avoid this.
     * @param {string} parentId Id of the parent node.
     * @param {number} start Start index of where to insert nodes.
     * @param {Array} nodesData Nodes data array.
     */
    addByParentId: function(parentId, start, nodesData) {
      var parent = parentId ? parentLookup[parentId] : this;
      this.addByParent_(parent, start, nodesData);
    },

    /**
     * Removes tree nodes by parent id.
     * This is used by intents_view.js.
     * @param {string} parentId Id of the parent node.
     * @param {number} start Start index of nodes to remove.
     * @param {number} count Number of nodes to remove.
     */
    removeByParentId: function(parentId, start, count) {
      var parent = parentId ? parentLookup[parentId] : this;
      if (!parent)
        return;

      parent.startBatchUpdates();
      while (count-- > 0)
        parent.remove(start);
      parent.endBatchUpdates();

      cr.dispatchSimpleEvent(this, 'change');
    },

    /**
     * Loads the immediate children of given parent node.
     * This is used by intents_view.js.
     * @param {string} parentId Id of the parent node.
     * @param {Array} children The immediate children of parent node.
     */
    loadChildren: function(parentId, children) {
      console.log('Loading intents view: ' +
                  parentId + ' ' + JSON.stringify(children));
      if (parentId)
        delete lookupRequests[parentId];
      var parent = parentId ? parentLookup[parentId] : this;
      if (!parent)
        return;

      parent.startBatchUpdates();
      parent.clear();
      this.addByParent_(parent, 0, children);
      parent.endBatchUpdates();
    },
  };

  return {
    IntentsList: IntentsList
  };
});
