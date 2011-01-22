// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Dependencies that we should remove/formalize:
// util.js
//
// afterTransition
// chrome.send
// hideNotification
// isRtl
// localStrings
// logEvent
// showNotification


var MostVisited = (function() {

  function addPinnedUrl(item, index) {
    chrome.send('addPinnedURL', [item.url, item.title, item.faviconUrl || '',
                                 item.thumbnailUrl || '', String(index)]);
  }

  function getItem(el) {
    return findAncestorByClass(el, 'thumbnail-container');
  }

  function updatePinnedDom(el, pinned) {
    el.querySelector('.pin').title = localStrings.getString(pinned ?
        'unpinthumbnailtooltip' : 'pinthumbnailtooltip');
    if (pinned) {
      el.classList.add('pinned');
    } else {
      el.classList.remove('pinned');
    }
  }

  function getThumbnailIndex(el) {
    var nodes = el.parentNode.querySelectorAll('.thumbnail-container');
    return Array.prototype.indexOf.call(nodes, el);
  }

  function MostVisited(el, miniview, menu, useSmallGrid, visible) {
    this.element = el;
    this.miniview = miniview;
    this.menu = menu;
    this.useSmallGrid_ = useSmallGrid;
    this.visible_ = visible;

    this.createThumbnails_();
    this.applyMostVisitedRects_();

    el.addEventListener('click', this.handleClick_.bind(this));
    el.addEventListener('keydown', this.handleKeyDown_.bind(this));

    document.addEventListener('DOMContentLoaded',
                              this.ensureSmallGridCorrect.bind(this));

    // Commands
    document.addEventListener('command', this.handleCommand_.bind(this));
    document.addEventListener('canExecute', this.handleCanExecute_.bind(this));

    // DND
    el.addEventListener('dragstart', this.handleDragStart_.bind(this));
    el.addEventListener('dragenter', this.handleDragEnter_.bind(this));
    el.addEventListener('dragover', this.handleDragOver_.bind(this));
    el.addEventListener('dragleave', this.handleDragLeave_.bind(this));
    el.addEventListener('drop', this.handleDrop_.bind(this));
    el.addEventListener('dragend', this.handleDragEnd_.bind(this));
    el.addEventListener('drag', this.handleDrag_.bind(this));
    el.addEventListener('mousedown', this.handleMouseDown_.bind(this));
  }

  MostVisited.prototype = {
    togglePinned_: function(el) {
      var index = getThumbnailIndex(el);
      var item = this.data[index];
      item.pinned = !item.pinned;
      if (item.pinned) {
        addPinnedUrl(item, index);
      } else {
        chrome.send('removePinnedURL', [item.url]);
      }
      updatePinnedDom(el, item.pinned);
    },

    swapPosition_: function(source, destination) {
      var nodes = source.parentNode.querySelectorAll('.thumbnail-container');
      var sourceIndex = getThumbnailIndex(source);
      var destinationIndex = getThumbnailIndex(destination);
      swapDomNodes(source, destination);

      var sourceData = this.data[sourceIndex];
      addPinnedUrl(sourceData, destinationIndex);
      sourceData.pinned = true;
      updatePinnedDom(source, true);

      var destinationData = this.data[destinationIndex];
      // Only update the destination if it was pinned before.
      if (destinationData.pinned) {
        addPinnedUrl(destinationData, sourceIndex);
      }
      this.data[destinationIndex] = sourceData;
      this.data[sourceIndex] = destinationData;
    },

    updateSettingsLink: function(hasBlacklistedUrls) {
      if (hasBlacklistedUrls)
        $('most-visited-settings').classList.add('has-blacklist');
      else
        $('most-visited-settings').classList.remove('has-blacklist');
    },

    blacklist: function(el) {
      var self = this;
      var url = el.href;
      chrome.send('blacklistURLFromMostVisited', [url]);

      el.classList.add('hide');

      // Find the old item.
      var oldUrls = {};
      var oldIndex = -1;
      var oldItem;
      var data = this.data;
      for (var i = 0; i < data.length; i++) {
        if (data[i].url == url) {
          oldItem = data[i];
          oldIndex = i;
        }
        oldUrls[data[i].url] = true;
      }

      // Send 'getMostVisitedPages' with a callback since we want to find the
      // new page and add that in the place of the removed page.
      chromeSend('getMostVisited', [], 'mostVisitedPages',
                 function(data, firstRun, hasBlacklistedUrls) {
        // Update settings link.
        self.updateSettingsLink(hasBlacklistedUrls);

        // Find new item.
        var newItem;
        for (var i = 0; i < data.length; i++) {
          if (!(data[i].url in oldUrls)) {
            newItem = data[i];
            break;
          }
        }

        if (!newItem) {
          // If no other page is available to replace the blacklisted item,
          // we need to reorder items s.t. all filler items are in the rightmost
          // indices.
          self.data = data;

        // Replace old item with new item in the most visited data array.
        } else if (oldIndex != -1) {
          var oldData = self.data.concat();
          oldData.splice(oldIndex, 1, newItem);
          self.data = oldData;
          el.classList.add('fade-in');
        }

        // We wrap the title in a <span class=blacklisted-title>. We pass an
        // empty string to the notifier function and use DOM to insert the real
        // string.
        var actionText = localStrings.getString('undothumbnailremove');

        // Show notification and add undo callback function.
        var wasPinned = oldItem.pinned;
        showNotification('', actionText, function() {
          self.removeFromBlackList(url);
          if (wasPinned) {
            addPinnedUrl(oldItem, oldIndex);
          }
          chrome.send('getMostVisited');
        });

        // Now change the DOM.
        var removeText = localStrings.getString('thumbnailremovednotification');
        var notifySpan = document.querySelector('#notification > span');
        notifySpan.textContent = removeText;

        // Focus the undo link.
        var undoLink = document.querySelector(
            '#notification > .link > [tabindex]');
        undoLink.focus();
      });
    },

    removeFromBlackList: function(url) {
      chrome.send('removeURLsFromMostVisitedBlacklist', [url]);
    },

    clearAllBlacklisted: function() {
      chrome.send('clearMostVisitedURLsBlacklist', []);
      hideNotification();
    },

    dirty_: false,
    invalidate_: function() {
      this.dirty_ = true;
    },

    visible_: true,
    get visible() {
      return this.visible_;
    },
    set visible(visible) {
      if (this.visible_ != visible) {
        this.visible_ = visible;
        this.invalidate_();
      }
    },

    useSmallGrid_: false,
    get useSmallGrid() {
      return this.useSmallGrid_;
    },
    set useSmallGrid(b) {
      if (this.useSmallGrid_ != b) {
        this.useSmallGrid_ = b;
        this.invalidate_();
      }
    },

    layout: function() {
      if (!this.dirty_)
        return;
      var d0 = Date.now();
      this.applyMostVisitedRects_();
      this.dirty_ = false;
      logEvent('mostVisited.layout: ' + (Date.now() - d0));
    },

    createThumbnails_: function() {
      var singleHtml =
          '<a class="thumbnail-container filler" tabindex="1">' +
            '<div class="edit-mode-border">' +
              '<div class="edit-bar">' +
                '<div class="pin"></div>' +
                '<div class="spacer"></div>' +
                '<div class="remove"></div>' +
              '</div>' +
              '<span class="thumbnail-wrapper">' +
                '<span class="thumbnail"></span>' +
              '</span>' +
            '</div>' +
            '<div class="title">' +
              '<div></div>' +
            '</div>' +
          '</a>';
      this.element.innerHTML = Array(8 + 1).join(singleHtml);
      var children = this.element.children;
      for (var i = 0; i < 8; i++) {
        children[i].id = 't' + i;
      }
    },

    getMostVisitedLayoutRects_: function() {
      var small = this.useSmallGrid;

      var cols = 4;
      var rows = 2;
      var marginWidth = 10;
      var marginHeight = 7;
      var borderWidth = 4;
      var thumbWidth = small ? 150 : 207;
      var thumbHeight = small ? 93 : 129;
      var w = thumbWidth + 2 * borderWidth + 2 * marginWidth;
      var h = thumbHeight + 40 + 2 * marginHeight;
      var sumWidth = cols * w  - 2 * marginWidth;
      var topSpacing = 10;

      var rtl = isRtl();
      var rects = [];

      if (this.visible) {
        for (var i = 0; i < rows * cols; i++) {
          var row = Math.floor(i / cols);
          var col = i % cols;
          var left = rtl ? sumWidth - col * w - thumbWidth - 2 * borderWidth :
              col * w;

          var top = row * h + topSpacing;

          rects[i] = {left: left, top: top};
        }
      }
      return rects;
    },

    applyMostVisitedRects_: function() {
      if (this.visible) {
        var rects = this.getMostVisitedLayoutRects_();
        var children = this.element.children;
        for (var i = 0; i < 8; i++) {
          var t = children[i];
          t.style.left = rects[i].left + 'px';
          t.style.top = rects[i].top + 'px';
          t.style.right = '';
          var innerStyle = t.firstElementChild.style;
          innerStyle.left = innerStyle.top = '';
        }
      }
    },

    // Work around for http://crbug.com/25329
    ensureSmallGridCorrect: function(expected) {
      if (expected != this.useSmallGrid)
        this.applyMostVisitedRects_();
    },

    getRectByIndex_: function(index) {
      return this.getMostVisitedLayoutRects_()[index];
    },

    // Commands

    handleCommand_: function(e) {
      var commandId = e.command.id;
      switch (commandId) {
        case 'clear-all-blacklisted':
          this.clearAllBlacklisted();
          chrome.send('getMostVisited');
          break;
      }
    },

    handleCanExecute_: function(e) {
      if (e.command.id == 'clear-all-blacklisted')
        e.canExecute = true;
    },

    // DND

    currentOverItem_: null,
    get currentOverItem() {
      return this.currentOverItem_;
    },
    set currentOverItem(item) {
      var style;
      if (item != this.currentOverItem_) {
        if (this.currentOverItem_) {
          style = this.currentOverItem_.firstElementChild.style;
          style.left = style.top = '';
        }
        this.currentOverItem_ = item;

        if (item) {
          // Make the drag over item move 15px towards the source. The movement
          // is done by only moving the edit-mode-border (as in the mocks) and
          // it is done with relative positioning so that the movement does not
          // change the drop target.
          var dragIndex = getThumbnailIndex(this.dragItem_);
          var overIndex = getThumbnailIndex(item);
          if (dragIndex == -1 || overIndex == -1) {
            return;
          }

          var dragRect = this.getRectByIndex_(dragIndex);
          var overRect = this.getRectByIndex_(overIndex);

          var x = dragRect.left - overRect.left;
          var y = dragRect.top - overRect.top;
          var z = Math.sqrt(x * x + y * y);
          var z2 = 15;
          var x2 = x * z2 / z;
          var y2 = y * z2 / z;

          style = this.currentOverItem_.firstElementChild.style;
          style.left = x2 + 'px';
          style.top = y2 + 'px';
        }
      }
    },
    dragItem_: null,
    startX_: 0,
    startY_: 0,
    startScreenX_: 0,
    startScreenY_: 0,
    dragEndTimer_: null,

    isDragging: function() {
      return !!this.dragItem_;
    },

    handleDragStart_: function(e) {
      var thumbnail = getItem(e.target);
      if (thumbnail) {
        // Don't set data since HTML5 does not allow setting the name for
        // url-list. Instead, we just rely on the dragging of link behavior.
        this.dragItem_ = thumbnail;
        this.dragItem_.classList.add('dragging');
        this.dragItem_.style.zIndex = 2;
        e.dataTransfer.effectAllowed = 'copyLinkMove';
      }
    },

    handleDragEnter_: function(e) {
      if (this.canDropOnElement_(this.currentOverItem)) {
        e.preventDefault();
      }
    },

    handleDragOver_: function(e) {
      var item = getItem(e.target);
      this.currentOverItem = item;
      if (this.canDropOnElement_(item)) {
        e.preventDefault();
        e.dataTransfer.dropEffect = 'move';
      }
    },

    handleDragLeave_: function(e) {
      var item = getItem(e.target);
      if (item) {
        e.preventDefault();
      }

      this.currentOverItem = null;
    },

    handleDrop_: function(e) {
      var dropTarget = getItem(e.target);
      if (this.canDropOnElement_(dropTarget)) {
        dropTarget.style.zIndex = 1;
        this.swapPosition_(this.dragItem_, dropTarget);
        // The timeout below is to allow WebKit to see that we turned off
        // pointer-event before moving the thumbnails so that we can get out of
        // hover mode.
        window.setTimeout((function() {
          this.invalidate_();
          this.layout();
        }).bind(this), 10);
        e.preventDefault();
        if (this.dragEndTimer_) {
          window.clearTimeout(this.dragEndTimer_);
          this.dragEndTimer_ = null;
        }
        afterTransition(function() {
          dropTarget.style.zIndex = '';
        });
      }
    },

    handleDragEnd_: function(e) {
      var dragItem = this.dragItem_;
      if (dragItem) {
        dragItem.style.pointerEvents = '';
        dragItem.classList.remove('dragging');

        afterTransition(function() {
          // Delay resetting zIndex to let the animation finish.
          dragItem.style.zIndex = '';
          // Same for overflow.
          dragItem.parentNode.style.overflow = '';
        });

        this.invalidate_();
        this.layout();
        this.dragItem_ = null;
      }
    },

    handleDrag_: function(e) {
      // Moves the drag item making sure that it is not displayed outside the
      // browser viewport.
      var item = getItem(e.target);
      var rect = this.element.getBoundingClientRect();
      item.style.pointerEvents = 'none';

      var x = this.startX_ + e.screenX - this.startScreenX_;
      var y = this.startY_ + e.screenY - this.startScreenY_;

      // The position of the item is relative to #most-visited so we need to
      // subtract that when calculating the allowed position.
      x = Math.max(x, -rect.left);
      x = Math.min(x, document.body.clientWidth - rect.left - item.offsetWidth -
                   2);
      // The shadow is 2px
      y = Math.max(-rect.top, y);
      y = Math.min(y, document.body.clientHeight - rect.top -
                   item.offsetHeight - 2);

      // Override right in case of RTL.
      item.style.right = 'auto';
      item.style.left = x + 'px';
      item.style.top = y + 'px';
      item.style.zIndex = 2;
    },

    // We listen to mousedown to get the relative position of the cursor for
    // dnd.
    handleMouseDown_: function(e) {
      var item = getItem(e.target);
      if (item) {
        this.startX_ = item.offsetLeft;
        this.startY_ = item.offsetTop;
        this.startScreenX_ = e.screenX;
        this.startScreenY_ = e.screenY;

        // We don't want to focus the item on mousedown. However, to prevent
        // focus one has to call preventDefault but this also prevents the drag
        // and drop (sigh) so we only prevent it when the user is not doing a
        // left mouse button drag.
        if (e.button != 0) // LEFT
          e.preventDefault();
      }
    },

    canDropOnElement_: function(el) {
      return this.dragItem_ && el &&
          el.classList.contains('thumbnail-container') &&
          !el.classList.contains('filler');
    },


    /// data

    data_: null,
    get data() {
      return this.data_;
    },
    set data(data) {
      // We append the class name with the "filler" so that we can style fillers
      // differently.
      var maxItems = 8;
      data.length = Math.min(maxItems, data.length);
      var len = data.length;
      for (var i = len; i < maxItems; i++) {
        data[i] = {filler: true};
      }

      // On setting we need to update the items
      this.data_ = data;
      this.updateMostVisited_();
      this.updateMiniview_();
      this.updateMenu_();
    },

    updateMostVisited_: function() {

      function getThumbnailClassName(item) {
        return 'thumbnail-container' +
            (item.pinned ? ' pinned' : '') +
            (item.filler ? ' filler' : '');
      }

      var data = this.data;
      var children = this.element.children;
      for (var i = 0; i < data.length; i++) {
        var d = data[i];
        var t = children[i];

        // If we have a filler continue
        var oldClassName = t.className;
        var newClassName = getThumbnailClassName(d);
        if (oldClassName != newClassName) {
          t.className = newClassName;
        }

        // No need to continue if this is a filler.
        if (newClassName == 'thumbnail-container filler') {
          // Make sure the user cannot tab to the filler.
          t.tabIndex = -1;
          t.querySelector('.thumbnail-wrapper').style.backgroundImage = '';
          continue;
        }
        // Allow focus.
        t.tabIndex = 1;

        t.href = d.url;
        t.querySelector('.pin').title = localStrings.getString(d.pinned ?
            'unpinthumbnailtooltip' : 'pinthumbnailtooltip');
        t.querySelector('.remove').title =
            localStrings.getString('removethumbnailtooltip');

        // There was some concern that a malformed malicious URL could cause an
        // XSS attack but setting style.backgroundImage = 'url(javascript:...)'
        // does not execute the JavaScript in WebKit.

        var thumbnailUrl = d.thumbnailUrl || 'chrome://thumb/' + d.url;
        t.querySelector('.thumbnail-wrapper').style.backgroundImage =
            url(thumbnailUrl);
        var titleDiv = t.querySelector('.title > div');
        titleDiv.xtitle = titleDiv.textContent = d.title;
        var faviconUrl = d.faviconUrl || 'chrome://favicon/' + d.url;
        titleDiv.style.backgroundImage = url(faviconUrl);
        titleDiv.dir = d.direction;
      }
    },

    updateMiniview_: function() {
      this.miniview.textContent = '';
      var data = this.data.slice(0, MAX_MINIVIEW_ITEMS);
      for (var i = 0, item; item = data[i]; i++) {
        if (item.filler) {
          continue;
        }

        var span = document.createElement('span');
        var a = span.appendChild(document.createElement('a'));
        a.href = item.url;
        a.textContent = item.title;
        a.style.backgroundImage = url('chrome://favicon/' + item.url);
        a.className = 'item';
        this.miniview.appendChild(span);
      }
      updateMiniviewClipping(this.miniview);
    },

    updateMenu_: function() {
      clearClosedMenu(this.menu);
      var data = this.data.slice(0, MAX_MINIVIEW_ITEMS);
      for (var i = 0, item; item = data[i]; i++) {
        if (!item.filler) {
          addClosedMenuEntry(
              this.menu, item.url, item.title, 'chrome://favicon/' + item.url);
        }
      }
      addClosedMenuFooter(
          this.menu, 'most-visited', MENU_THUMB, Section.THUMB);
    },

    handleClick_: function(e) {
      var target = e.target;
      if (target.classList.contains('pin')) {
        this.togglePinned_(getItem(target));
        e.preventDefault();
      } else if (target.classList.contains('remove')) {
        this.blacklist(getItem(target));
        e.preventDefault();
      } else {
        var item = getItem(target);
        if (item) {
          var index = Array.prototype.indexOf.call(item.parentNode.children,
                                                   item);
          if (index != -1)
            chrome.send('metrics', ['NTP_MostVisited' + index]);
        }
      }
    },

    /**
     * Allow blacklisting most visited site using the keyboard.
     */
    handleKeyDown_: function(e) {
      if (!IS_MAC && e.keyCode == 46 || // Del
          IS_MAC && e.metaKey && e.keyCode == 8) { // Cmd + Backspace
        this.blacklist(e.target);
      }
    }
  };

  return MostVisited;
})();
