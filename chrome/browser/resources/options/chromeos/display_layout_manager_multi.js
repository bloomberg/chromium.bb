// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.exportPath('options');

cr.define('options', function() {
  'use strict';

  var DisplayLayoutManager = options.DisplayLayoutManager;

  /**
   * @constructor
   * @extends {options.DisplayLayoutManager}
   */
  function DisplayLayoutManagerMulti() { DisplayLayoutManager.call(this); }

  // Helper class for display layout management. Implements logic for laying
  // out 3+ displays.
  DisplayLayoutManagerMulti.prototype = {
    __proto__: DisplayLayoutManager.prototype,

    /** @type {string} */
    dragId_: '',

    /** @type {string} */
    dragParentId_: '',

    /** @type {options.DisplayLayoutType} */
    dragLayoutType_: options.DisplayLayoutType.RIGHT,

    /** @override */
    setFocusedId: function(focusedId, opt_userAction) {
      DisplayLayoutManager.prototype.setFocusedId.call(this, focusedId);
      if (!opt_userAction || this.dragId_ != '')
        return;
      var layout = this.displayLayoutMap_[focusedId];
      this.highlightEdge_(layout.parentId, layout.layoutType);
    },

    /** @override */
    updatePosition: function(id, newPosition) {
      this.dragId_ = id;
      var layout = this.displayLayoutMap_[id];

      // Find the closest parent.
      var parentId = this.findClosest_(layout, newPosition);
      var parent = this.displayLayoutMap_[parentId];

      // Find the closest edge.
      var layoutType = layout.getLayoutTypeForPosition(parent.div, newPosition);

      // Calculate the new position and delta.
      var oldPos = {x: layout.div.offsetLeft, y: layout.div.offsetTop};
      var newPos = layout.getSnapPosition(newPosition, parent.div, layoutType);
      var deltaPos = {x: newPos.x - oldPos.x, y: newPos.y - oldPos.y};

      // Check for collisions.
      this.collideAndModifyDelta_(layout, oldPos, deltaPos);
      if (deltaPos.x == 0 && deltaPos.y == 0)
        return;

      // Update the div (but not the actual layout type or offset yet),
      newPos = {x: oldPos.x + deltaPos.x, y: oldPos.y + deltaPos.y};
      layout.setDivPosition(newPos, parent.div, layoutType);

      // If the edge changed, update and highlight it.
      if (layoutType != this.dragLayoutType_ ||
          parentId != this.dragParentId_) {
        this.dragLayoutType_ = layoutType;
        this.dragParentId_ = parentId;
        this.highlightEdge_(this.dragParentId_, this.dragLayoutType_);
      }
    },

    /** @override */
    finalizePosition: function(id) {
      if (id != this.dragId_ || !this.dragParentId_) {
        this.dragId_ = '';
        return false;
      }

      var layout = this.displayLayoutMap_[id];

      // All immediate children of |layout| will need to be re-parented.
      var orphanIds = this.findChildren_(id, false /* do not recurse */);

      if (layout.parentId == '') {
        // We can not re-parent the primary display, so instead re-parent the
        // "drag parent" to the primary display and make all other displays
        // orphans.
        orphanIds = this.findChildren_(id, true /* recurse */);

        // Remove the "drag parent" from the orphans list and reparent it first.
        var index = orphanIds.indexOf(this.dragParentId_);
        assert(index != -1);
        orphanIds.splice(index, 1);
        this.reparentOrphan_(this.dragParentId_, orphanIds);
      } else {
        var dragParent = this.displayLayoutMap_[this.dragParentId_];

        // Special case: re-parenting to a descendant. Parent the immediate
        // child (the 'top parent') to the dragged display's current parent.
        var topParent = dragParent;
        while (topParent) {
          if (topParent.parentId == '')
            break;
          if (topParent.parentId == id) {
            topParent.parentId = layout.parentId;
            break;
          }
          topParent = this.displayLayoutMap_[topParent.parentId];
        }

        // Re-parent the dragged display.
        layout.parentId = this.dragParentId_;
        layout.layoutType = this.dragLayoutType_;

        // Snap to corners and calculate the offset from the new parent.
        // This does not depend on any unresolved child layout.
        layout.adjustCorners(dragParent.div, this.dragLayoutType_);
        layout.calculateOffset(this.visualScale_, dragParent);
      }

      // Update any orphaned children. This may cause the dragged display to
      // be re-attached if it was attached to a child.
      this.updateOrphans_(orphanIds);

      this.highlightEdge_('', undefined);  // Remove any highlights.
      this.dragId_ = '';

      return layout.originalDivOffsets.x != layout.div.offsetLeft ||
          layout.originalDivOffsets.y != layout.div.offsetTop;
    },

    /**
     * Re-parent all entries in |orphanIds| and any children.
     * @param {Array<string>} orphanIds The list of ids affected by the move.
     * @private
     */
    updateOrphans_: function(orphanIds) {
      var orphans = orphanIds.slice();
      for (var orphan of orphanIds) {
        var newOrphans = this.findChildren_(orphan, true /* recurse */);
        // If the dragged display was re-parented to one of its children,
        // there may be duplicates so merge the lists.
        for (var o of newOrphans) {
          if (orphans.indexOf(o) == -1)
            orphans.push(o);
        }
      }

      // Remove each orphan from the list as it is re-parented so that
      // subsequent orphans can be parented to it.
      while (orphans.length) {
        var orphanId = orphans.shift();
        this.reparentOrphan_(orphanId, orphans);
      }
    },

    /**
     * Re-parent the orphan to a layout that is not a member of
     * |otherOrphanIds|.
     * @param {string} orphanId The id of the orphan to re-parent.
     * @param {Array<string>} otherOrphanIds The list of ids of other orphans
     *     to ignore when re-parenting.
     * @private
     */
    reparentOrphan_: function(orphanId, otherOrphanIds) {
      var orphan = this.displayLayoutMap_[orphanId];

      if (orphanId == this.dragId_ && orphan.parentId != '') {
        // Preserve the layout and offset of the dragged div.
        var parent = this.displayLayoutMap_[orphan.parentId];
        orphan.bounds = this.calcLayoutBounds_(orphan);
        orphan.layoutDivFromBounds(
            this.displayAreaOffset_, this.visualScale_, parent);
        return;
      }

      // Find the closest parent.
      var pos = {x: orphan.div.offsetLeft, y: orphan.div.offsetTop};
      var newParentId = this.findClosest_(orphan, pos, otherOrphanIds);
      orphan.parentId = newParentId;
      assert(newParentId != '');
      var parent = this.displayLayoutMap_[newParentId];

      // Find the closest edge.
      var layoutType = orphan.getLayoutTypeForPosition(parent.div, pos);

      // Snap from the nearest corner to the desired locaiton and get the delta.
      var cornerPos = orphan.getCornerPos(parent.div, pos);
      pos = orphan.getSnapPosition(pos, parent.div, layoutType);
      var deltaPos = {x: pos.x - cornerPos.x, y: pos.y - cornerPos.y};

      // Check for collisions.
      this.collideAndModifyDelta_(orphan, cornerPos, deltaPos);
      pos = {x: cornerPos.x + deltaPos.x, y: cornerPos.y + deltaPos.y};

      // Update the div and adjust the corners.
      orphan.layoutType = layoutType;
      orphan.setDivPosition(pos, parent.div, layoutType);
      orphan.adjustCorners(parent.div, layoutType);

      // Calculate the bounds from the new div position.
      orphan.calculateOffset(this.visualScale_, parent);

      // TODO(stevenjb): Set bounds and send orphan update.
    },

    /**
     * @param {string} parentId
     * @param {boolean} recurse Include descendants of children.
     * @return {!Array<string>}
     * @private
     */
    findChildren_: function(parentId, recurse) {
      var children = [];
      for (var childId in this.displayLayoutMap_) {
        if (childId == parentId)
          continue;
        if (this.displayLayoutMap_[childId].parentId == parentId) {
          // Insert immediate children at the front of the array.
          children.unshift(childId);
          if (recurse) {
            // Descendants get added to the end of the list.
            children = children.concat(this.findChildren_(childId, true));
          }
        }
      }
      return children;
    },

    /**
     * Finds the display closest to |position| ignoring |child|.
     * @param {!options.DisplayLayout} child
     * @param {!options.DisplayPosition} position
     * @param {Array<string>=} opt_ignoreIds Ids to ignore.
     * @return {string}
     * @private
     */
    findClosest_: function(child, position, opt_ignoreIds) {
      var x = position.x + child.div.offsetWidth / 2;
      var y = position.y + child.div.offsetHeight / 2;
      var closestId = '';
      var closestDelta2 = 0;
      for (var id in this.displayLayoutMap_) {
        if (id == child.id)
          continue;
        if (opt_ignoreIds && opt_ignoreIds.indexOf(id) != -1)
          continue;
        var div = this.displayLayoutMap_[id].div;
        var left = div.offsetLeft;
        var top = div.offsetTop;
        var width = div.offsetWidth;
        var height = div.offsetHeight;
        if (x >= left && x < left + width && y >= top && y < top + height)
          return id;  // point is inside rect
        var dx, dy;
        if (x < left)
          dx = left - x;
        else if (x > left + width)
          dx = x - (left + width);
        else
          dx = 0;
        if (y < top)
          dy = top - y;
        else if (y > top + height)
          dy = y - (top + height);
        else
          dy = 0;
        var delta2 = dx * dx + dy * dy;
        if (closestId == '' || delta2 < closestDelta2) {
          closestId = id;
          closestDelta2 = delta2;
        }
      }
      return closestId;
    },

    /**
     * Intersects |layout| at |pos| with each other layout and reduces
     * |deltaPos| to avoid any collisions (or sets it to [0,0] if the div can
     * not be moved in the direction of |deltaPos|).
     * @param {!options.DisplayLayout} layout
     * @param {!options.DisplayPosition} pos
     * @param {!options.DisplayPosition} deltaPos
     */
    collideAndModifyDelta_: function(layout, pos, deltaPos) {
      var keys = Object.keys(
          /** @type {!Object<!options.DisplayLayout>}*/ (
              this.displayLayoutMap_));
      var others = new Set(keys);
      others.delete(layout.id);
      var checkCollisions = true;
      while (checkCollisions) {
        checkCollisions = false;
        for (var tid of others) {
          var tlayout = this.displayLayoutMap_[tid];
          if (layout.collideWithDivAndModifyDelta(pos, tlayout.div, deltaPos)) {
            if (deltaPos.x == 0 && deltaPos.y == 0)
              return;
            others.delete(tid);
            checkCollisions = true;
            break;
          }
        }
      }
    },

    /**
     * Highlights the edge of the div associated with |parentId| based on
     * |layoutType| and removes any other highlights. If |layoutType| is
     * undefined, removes all highlights.
     * @param {string} id
     * @param {options.DisplayLayoutType|undefined} layoutType
     * @private
     */
    highlightEdge_: function(id, layoutType) {
      for (var tid in this.displayLayoutMap_) {
        var tlayout = this.displayLayoutMap_[tid];
        var highlight = '';
        if (tlayout.id == id) {
          switch (layoutType) {
            case options.DisplayLayoutType.RIGHT:
              highlight = 'displays-parent-right';
              break;
            case options.DisplayLayoutType.LEFT:
              highlight = 'displays-parent-left';
              break;
            case options.DisplayLayoutType.TOP:
              highlight = 'displays-parent-top';
              break;
            case options.DisplayLayoutType.BOTTOM:
              highlight = 'displays-parent-bottom';
              break;
          }
        }
        tlayout.div.classList.toggle(
            'displays-parent-right', highlight == 'displays-parent-right');
        tlayout.div.classList.toggle(
            'displays-parent-left', highlight == 'displays-parent-left');
        tlayout.div.classList.toggle(
            'displays-parent-top', highlight == 'displays-parent-top');
        tlayout.div.classList.toggle(
            'displays-parent-bottom', highlight == 'displays-parent-bottom');
      }
    },

    /**
     * Calculates but do not set the div bound.
     * @param {!options.DisplayLayout} layout
     * @return {!options.DisplayBounds}
     */
    calcLayoutBounds_: function(layout) {
      if (layout.parentId == '')
        return /** @type {!options.DisplayBounds} */ (layout.bounds);
      var parent = this.displayLayoutMap_[layout.parentId];
      // Parent layout bounds may not be calculated yet, so calculate (but
      // do not set) them.
      var parentBounds = this.calcLayoutBounds_(parent);
      return layout.calculateBounds(parentBounds);
    },
  };

  // Export
  return {DisplayLayoutManagerMulti: DisplayLayoutManagerMulti};
});
