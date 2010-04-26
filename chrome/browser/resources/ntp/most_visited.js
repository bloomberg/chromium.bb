// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var mostVisitedData = [];
var gotMostVisited = false;

function mostVisitedPages(data, firstRun) {
  logEvent('received most visited pages');

  // We append the class name with the "filler" so that we can style fillers
  // differently.
  var maxItems = 8;
  data.length = Math.min(maxItems, data.length);
  var len = data.length;
  for (var i = len; i < maxItems; i++) {
    data[i] = {filler: true};
  }

  mostVisitedData = data;
  renderMostVisited(data);

  gotMostVisited = true;
  onDataLoaded();

  // Only show the first run notification if first run.
  if (firstRun) {
    showFirstRunNotification();
  }
}
function getThumbnailClassName(data) {
  return 'thumbnail-container' +
      (data.pinned ? ' pinned' : '') +
      (data.filler ? ' filler' : '');
}

function renderMostVisited(data) {
  var parent = $('most-visited');
  var children = parent.children;
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
      continue;
    }
    // Allow focus.
    t.tabIndex = 1;

    t.href = d.url;
    t.querySelector('.pin').title = localStrings.getString(d.pinned ?
        'unpinthumbnailtooltip' : 'pinthumbnailtooltip');
    t.querySelector('.remove').title =
        localStrings.getString('removethumbnailtooltip');

    // There was some concern that a malformed malicious URL could cause an XSS
    // attack but setting style.backgroundImage = 'url(javascript:...)' does
    // not execute the JavaScript in WebKit.

    var thumbnailUrl = d.thumbnailUrl || 'chrome://thumb/' + d.url;
    t.querySelector('.thumbnail-wrapper').style.backgroundImage =
        url(thumbnailUrl);
    var titleDiv = t.querySelector('.title > div');
    titleDiv.xtitle = titleDiv.textContent = d.title;
    var faviconUrl = d.faviconUrl || 'chrome://favicon/' + d.url;
    titleDiv.style.backgroundImage = url(faviconUrl);
    titleDiv.dir = d.direction;
  }
}

var mostVisited = {
  addPinnedUrl_: function(data, index) {
    chrome.send('addPinnedURL', [data.url, data.title, data.faviconUrl || '',
                                 data.thumbnailUrl || '', String(index)]);
  },
  getItem: function(el) {
    return findAncestorByClass(el, 'thumbnail-container');
  },

  getHref: function(el) {
    return el.href;
  },

  togglePinned: function(el) {
    var index = this.getThumbnailIndex(el);
    var data = mostVisitedData[index];
    data.pinned = !data.pinned;
    if (data.pinned) {
      this.addPinnedUrl_(data, index);
    } else {
      chrome.send('removePinnedURL', [data.url]);
    }
    this.updatePinnedDom_(el, data.pinned);
  },

  updatePinnedDom_: function(el, pinned) {
    el.querySelector('.pin').title = localStrings.getString(pinned ?
        'unpinthumbnailtooltip' : 'pinthumbnailtooltip');
    if (pinned) {
      addClass(el, 'pinned');
    } else {
      removeClass(el, 'pinned');
    }
  },

  getThumbnailIndex: function(el) {
    var nodes = el.parentNode.querySelectorAll('.thumbnail-container');
    return Array.prototype.indexOf.call(nodes, el);
  },

  swapPosition: function(source, destination) {
    var nodes = source.parentNode.querySelectorAll('.thumbnail-container');
    var sourceIndex = this.getThumbnailIndex(source);
    var destinationIndex = this.getThumbnailIndex(destination);
    swapDomNodes(source, destination);

    var sourceData = mostVisitedData[sourceIndex];
    this.addPinnedUrl_(sourceData, destinationIndex);
    sourceData.pinned = true;
    this.updatePinnedDom_(source, true);

    var destinationData = mostVisitedData[destinationIndex];
    // Only update the destination if it was pinned before.
    if (destinationData.pinned) {
      this.addPinnedUrl_(destinationData, sourceIndex);
    }
    mostVisitedData[destinationIndex] = sourceData;
    mostVisitedData[sourceIndex] = destinationData;
  },

  blacklist: function(el) {
    var self = this;
    var url = this.getHref(el);
    chrome.send('blacklistURLFromMostVisited', [url]);

    addClass(el, 'hide');

    // Find the old item.
    var oldUrls = {};
    var oldIndex = -1;
    var oldItem;
    for (var i = 0; i < mostVisitedData.length; i++) {
      if (mostVisitedData[i].url == url) {
        oldItem = mostVisitedData[i];
        oldIndex = i;
      }
      oldUrls[mostVisitedData[i].url] = true;
    }

    // Send 'getMostVisitedPages' with a callback since we want to find the new
    // page and add that in the place of the removed page.
    chromeSend('getMostVisited', [], 'mostVisitedPages', function(data) {
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
        mostVisitedPages(data);

      // Replace old item with new item in the mostVisitedData array.
      } else if (oldIndex != -1) {
        mostVisitedData.splice(oldIndex, 1, newItem);
        mostVisitedPages(mostVisitedData);
        addClass(el, 'fade-in');
      }

      // We wrap the title in a <span class=blacklisted-title>. We pass an empty
      // string to the notifier function and use DOM to insert the real string.
      var actionText = localStrings.getString('undothumbnailremove');

      // Show notification and add undo callback function.
      var wasPinned = oldItem.pinned;
      showNotification('', actionText, function() {
        self.removeFromBlackList(url);
        if (wasPinned) {
          self.addPinnedUrl_(oldItem, oldIndex);
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

  updateDisplayMode: function() {
    if (!this.dirty_) {
      return;
    }
    updateSimpleSection('most-visited-section', Section.THUMB);
  },

  dirty_: false,

  invalidate: function() {
    this.dirty_ = true;
  },

  layout: function() {
    if (!this.dirty_) {
      return;
    }
    var d0 = Date.now();

    var mostVisitedElement = $('most-visited');
    var thumbnails = mostVisitedElement.children;
    var hidden = !(shownSections & Section.THUMB);


    // We set overflow to hidden so that the most visited element does not
    // "leak" when we hide and show it.
    if (hidden) {
      mostVisitedElement.style.overflow = 'hidden';
    }

    applyMostVisitedRects();

    // Only set overflow to visible if the element is shown.
    if (!hidden) {
      afterTransition(function() {
        mostVisitedElement.style.overflow = '';
      });
    }

    this.dirty_ = false;

    logEvent('mostVisited.layout: ' + (Date.now() - d0));
  },

  getRectByIndex: function(index) {
    return getMostVisitedLayoutRects()[index];
  }
};

$('most-visited').addEventListener('click', function(e) {
  var target = e.target;
  if (hasClass(target, 'pin')) {
    mostVisited.togglePinned(mostVisited.getItem(target));
    e.preventDefault();
  } else if (hasClass(target, 'remove')) {
    mostVisited.blacklist(mostVisited.getItem(target));
    e.preventDefault();
  }
});

// Allow blacklisting most visited site using the keyboard.
$('most-visited').addEventListener('keydown', function(e) {
  if (!IS_MAC && e.keyCode == 46 || // Del
      IS_MAC && e.metaKey && e.keyCode == 8) { // Cmd + Backspace
    mostVisited.blacklist(e.target);
  }
});

window.addEventListener('load', onDataLoaded);

window.addEventListener('resize', handleWindowResize);

// Work around for http://crbug.com/25329
function ensureSmallGridCorrect() {
  if (wasSmallGrid != useSmallGrid()) {
    applyMostVisitedRects();
  }
}
document.addEventListener('DOMContentLoaded', ensureSmallGridCorrect);

// DnD

var dnd = {
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
        // Make the drag over item move 15px towards the source. The movement is
        // done by only moving the edit-mode-border (as in the mocks) and it is
        // done with relative positioning so that the movement does not change
        // the drop target.
        var dragIndex = mostVisited.getThumbnailIndex(this.dragItem);
        var overIndex = mostVisited.getThumbnailIndex(item);
        if (dragIndex == -1 || overIndex == -1) {
          return;
        }

        var dragRect = mostVisited.getRectByIndex(dragIndex);
        var overRect = mostVisited.getRectByIndex(overIndex);

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
  dragItem: null,
  startX: 0,
  startY: 0,
  startScreenX: 0,
  startScreenY: 0,
  dragEndTimer: null,

  handleDragStart: function(e) {
    var thumbnail = mostVisited.getItem(e.target);
    if (thumbnail) {
      // Don't set data since HTML5 does not allow setting the name for
      // url-list. Instead, we just rely on the dragging of link behavior.
      this.dragItem = thumbnail;
      addClass(this.dragItem, 'dragging');
      this.dragItem.style.zIndex = 2;
      e.dataTransfer.effectAllowed = 'copyLinkMove';
    }
  },

  handleDragEnter: function(e) {
    if (this.canDropOnElement(this.currentOverItem)) {
      e.preventDefault();
    }
  },

  handleDragOver: function(e) {
    var item = mostVisited.getItem(e.target);
    this.currentOverItem = item;
    if (this.canDropOnElement(item)) {
      e.preventDefault();
      e.dataTransfer.dropEffect = 'move';
    }
  },

  handleDragLeave: function(e) {
    var item = mostVisited.getItem(e.target);
    if (item) {
      e.preventDefault();
    }

    this.currentOverItem = null;
  },

  handleDrop: function(e) {
    var dropTarget = mostVisited.getItem(e.target);
    if (this.canDropOnElement(dropTarget)) {
      dropTarget.style.zIndex = 1;
      mostVisited.swapPosition(this.dragItem, dropTarget);
      // The timeout below is to allow WebKit to see that we turned off
      // pointer-event before moving the thumbnails so that we can get out of
      // hover mode.
      window.setTimeout(function() {
        mostVisited.invalidate();
        mostVisited.layout();
      }, 10);
      e.preventDefault();
      if (this.dragEndTimer) {
        window.clearTimeout(this.dragEndTimer);
        this.dragEndTimer = null;
      }
      afterTransition(function() {
        dropTarget.style.zIndex = '';
      });
    }
  },

  handleDragEnd: function(e) {
    var dragItem = this.dragItem;
    if (dragItem) {
      dragItem.style.pointerEvents = '';
      removeClass(dragItem, 'dragging');

      afterTransition(function() {
        // Delay resetting zIndex to let the animation finish.
        dragItem.style.zIndex = '';
        // Same for overflow.
        dragItem.parentNode.style.overflow = '';
      });

      mostVisited.invalidate();
      mostVisited.layout();
      this.dragItem = null;
    }
  },

  handleDrag: function(e) {
    // Moves the drag item making sure that it is not displayed outside the
    // browser viewport.
    var item = mostVisited.getItem(e.target);
    var rect = document.querySelector('#most-visited').getBoundingClientRect();
    item.style.pointerEvents = 'none';

    var x = this.startX + e.screenX - this.startScreenX;
    var y = this.startY + e.screenY - this.startScreenY;

    // The position of the item is relative to #most-visited so we need to
    // subtract that when calculating the allowed position.
    x = Math.max(x, -rect.left);
    x = Math.min(x, document.body.clientWidth - rect.left - item.offsetWidth -
                 2);
    // The shadow is 2px
    y = Math.max(-rect.top, y);
    y = Math.min(y, document.body.clientHeight - rect.top - item.offsetHeight -
                 2);

    // Override right in case of RTL.
    item.style.right = 'auto';
    item.style.left = x + 'px';
    item.style.top = y + 'px';
    item.style.zIndex = 2;
  },

  // We listen to mousedown to get the relative position of the cursor for dnd.
  handleMouseDown: function(e) {
    var item = mostVisited.getItem(e.target);
    if (item) {
      this.startX = item.offsetLeft;
      this.startY = item.offsetTop;
      this.startScreenX = e.screenX;
      this.startScreenY = e.screenY;

      // We don't want to focus the item on mousedown. However, to prevent focus
      // one has to call preventDefault but this also prevents the drag and drop
      // (sigh) so we only prevent it when the user is not doing a left mouse
      // button drag.
      if (e.button != 0) // LEFT
        e.preventDefault();
    }
  },

  canDropOnElement: function(el) {
    return this.dragItem && el && hasClass(el, 'thumbnail-container') &&
        !hasClass(el, 'filler');
  },

  init: function() {
    var el = $('most-visited');
    el.addEventListener('dragstart', bind(this.handleDragStart, this));
    el.addEventListener('dragenter', bind(this.handleDragEnter, this));
    el.addEventListener('dragover', bind(this.handleDragOver, this));
    el.addEventListener('dragleave', bind(this.handleDragLeave, this));
    el.addEventListener('drop', bind(this.handleDrop, this));
    el.addEventListener('dragend', bind(this.handleDragEnd, this));
    el.addEventListener('drag', bind(this.handleDrag, this));
    el.addEventListener('mousedown', bind(this.handleMouseDown, this));
  }
};

dnd.init();

