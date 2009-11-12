
// Helpers

// TODO(arv): Remove these when classList is available in HTML5.
// https://bugs.webkit.org/show_bug.cgi?id=20709
function hasClass(el, name) {
  return el.nodeType == 1 && el.className.split(/\s+/).indexOf(name) != -1;
}

function addClass(el, name) {
  var names = el.className.split(/\s+/);
  if (names.indexOf(name) == -1) {
    el.className += ' ' + name;
  }
}

function removeClass(el, name) {
  var names = el.className.split(/\s+/);
  el.className = names.filter(function(n) {
    return name != n;
  }).join(' ');
}

function findAncestorByClass(el, className) {
  return findAncestor(el, function(el) {
    return hasClass(el, className);
  });
}

/**
 * Return the first ancestor for which the {@code predicate} returns true.
 * @param {Node} node The node to check.
 * @param {function(Node) : boolean} predicate The function that tests the
 *     nodes.
 * @return {Node} The found ancestor or null if not found.
 */
function findAncestor(node, predicate) {
  var last = false;
  while (node != null && !(last = predicate(node))) {
    node = node.parentNode;
  }
  return last ? node : null;
}

// WebKit does not have Node.prototype.swapNode
// https://bugs.webkit.org/show_bug.cgi?id=26525
function swapDomNodes(a, b) {
  var afterA = a.nextSibling;
  if (afterA == b) {
    swapDomNodes(b, a);
    return;
  }
  var aParent = a.parentNode;
  b.parentNode.replaceChild(a, b);
  aParent.insertBefore(b, afterA);
}

function bind(fn, selfObj, var_args) {
  var boundArgs = Array.prototype.slice.call(arguments, 2);
  return function() {
    var args = Array.prototype.slice.call(arguments);
    args.unshift.apply(args, boundArgs);
    return fn.apply(selfObj, args);
  }
}

var loading = true;
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

var tipCache = {};

function tips(data) {
  logEvent('received tips');
  tipCache = data;
  renderTip();
}

function createTip(data) {
  if (data.length) {
    try {
      return parseHtmlSubset(data[0].tip_html_text);
    } catch (parseErr) {
      console.error('Error parsing tips: ' + parseErr.message);
    }
  }
  // Return an empty DF in case of failure.
  return document.createDocumentFragment();
}

function renderTip() {
  var tipElement = $('tip-line');
  // There should always be only one tip.
  tipElement.textContent = '';
  tipElement.appendChild(createTip(tipCache));
  fixLinkUnderlines(tipElement);
}

function recentlyClosedTabs(data) {
  logEvent('received recently closed tabs');
  // We need to store the recent items so we can update the layout on a resize.
  recentItems = data;
  renderRecentlyClosed();
}

var recentItems = [];

function renderRecentlyClosed() {
  // We remove all items but the header and the nav
  var recentlyClosedElement = $('recently-closed');
  var headerEl = recentlyClosedElement.firstElementChild;
  var navEl = recentlyClosedElement.lastElementChild;

  for (var el = navEl.previousElementSibling; el != headerEl;
       el = navEl.previousElementSibling) {
    recentlyClosedElement.removeChild(el);
  }

  // Create new items
  recentItems.forEach(function(item) {
    var el = createRecentItem(item);
    recentlyClosedElement.insertBefore(el, navEl);
  });

  layoutRecentlyClosed();
}

function createRecentItem(data) {
  var isWindow = data.type == 'window';
  var el;
  if (isWindow) {
    el = document.createElement('span');
    el.className = 'item link window';
    el.tabItems = data.tabs;
    el.tabIndex = 0;
    el.textContent = formatTabsText(data.tabs.length);
  } else {
    el = document.createElement('a');
    el.className = 'item';
    el.href = data.url;
    el.style.backgroundImage = url('chrome://favicon/' + data.url);
    el.dir = data.direction;
    el.textContent = data.title;
  }
  el.sessionId = data.sessionId;
  el.xtitle = data.title;
  var wrapperEl = document.createElement('span');
  wrapperEl.appendChild(el);
  return wrapperEl;
}

function onShownSections(mask) {
  logEvent('received shown sections');
  if (mask != shownSections) {
    var oldShownSections = shownSections;
    shownSections = mask;

    // Only invalidate most visited if needed.
    if ((mask & Section.THUMB) != (oldShownSections & Section.THUMB) ||
        (mask & Section.LIST) != (oldShownSections & Section.LIST)) {
      mostVisited.invalidate();
    }

    mostVisited.updateDisplayMode();
    renderRecentlyClosed();
  }
}

function saveShownSections() {
  chrome.send('setShownSections', [String(shownSections)]);
}

function getThumbnailClassName(data) {
  return 'thumbnail-container' +
      (data.pinned ? ' pinned' : '') +
      (data.filler ? ' filler' : '');
}

function url(s) {
  // http://www.w3.org/TR/css3-values/#uris
  // Parentheses, commas, whitespace characters, single quotes (') and double
  // quotes (") appearing in a URI must be escaped with a backslash
  var s2 = s.replace(/(\(|\)|\,|\s|\'|\"|\\)/g, '\\$1');
  // WebKit has a bug when it comes to URLs that end with \
  // https://bugs.webkit.org/show_bug.cgi?id=28885
  if (/\\\\$/.test(s2)) {
    // Add a space to work around the WebKit bug.
    s2 += ' ';
  }
  return 'url("' + s2 + '")';
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
      continue;
    }

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

/**
 * Calls chrome.send with a callback and restores the original afterwards.
 */
function chromeSend(name, params, callbackName, callback) {
  var old = global[callbackName];
  global[callbackName] = function() {
    // restore
    global[callbackName] = old;

    var args = Array.prototype.slice.call(arguments);
    return callback.apply(global, args);
  };
  chrome.send(name, params);
}

var LayoutMode = {
  SMALL: 1,
  NORMAL: 2
};

var layoutMode = useSmallGrid() ? LayoutMode.SMALL : LayoutMode.NORMAL;

function handleWindowResize() {
  if (window.innerWidth < 10) {
    // We're probably a background tab, so don't do anything.
    return;
  }

  var oldLayoutMode = layoutMode;
  layoutMode = useSmallGrid() ? LayoutMode.SMALL : LayoutMode.NORMAL

  if (layoutMode != oldLayoutMode){
    mostVisited.invalidate();
    mostVisited.layout();
    renderRecentlyClosed();
  }
}

function showSection(section) {
  if (!(section & shownSections)) {
    shownSections |= section;

    // THUMBS and LIST are mutually exclusive.
    if (section == Section.THUMB) {
      // hide LIST
      shownSections &= ~Section.LIST;
    } else if (section == Section.LIST) {
      // hide THUMB
      shownSections &= ~Section.THUMB;
    }
    switch (section) {
      case Section.THUMB:
      case Section.LIST:
        mostVisited.invalidate();
        mostVisited.updateDisplayMode();
        mostVisited.layout();
        break;
      case Section.RECENT:
        renderRecentlyClosed();
        break;
      case Section.TIPS:
        $('tip-line').style.display = '';
        break;
      case Section.SYNC:
        $('sync-status').style.display = '';
        break;
    }
  }
}

function hideSection(section) {
  if (section & shownSections) {
    shownSections &= ~section;

    switch (section) {
      case Section.THUMB:
      case Section.LIST:
        mostVisited.invalidate();
        mostVisited.updateDisplayMode();
        mostVisited.layout();
        break;
      case Section.RECENT:
        renderRecentlyClosed();
        break;
      case Section.TIPS:
        $('tip-line').style.display = 'none';
        break;
      case Section.SYNC:
        $('sync-status').style.display = 'none';
        break;
    }
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
    });
  },

  removeFromBlackList: function(url) {
    chrome.send('removeURLsFromMostVisitedBlacklist', [url]);
  },

  clearAllBlacklisted: function() {
    chrome.send('clearMostVisitedURLsBlacklist', []);
  },

  updateDisplayMode: function() {
    if (!this.dirty_) {
      return;
    }

    var thumbCheckbox = $('thumb-checkbox');
    var listCheckbox = $('list-checkbox');
    var mostVisitedElement = $('most-visited');

    if (shownSections & Section.THUMB) {
      thumbCheckbox.checked = true;
      listCheckbox.checked = false;
      removeClass(mostVisitedElement, 'list');
      removeClass(mostVisitedElement, 'collapsed');
    } else if (shownSections & Section.LIST) {
      thumbCheckbox.checked = false;
      listCheckbox.checked = true;
      addClass(mostVisitedElement, 'list');
      removeClass(mostVisitedElement, 'collapsed');
    } else {
      thumbCheckbox.checked = false;
      listCheckbox.checked = false;
      addClass(mostVisitedElement, 'collapsed');
    }
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
    var collapsed = false;

    if (shownSections & Section.LIST) {
      addClass(mostVisitedElement, 'list');
    } else if (shownSections & Section.THUMB) {
      removeClass(mostVisitedElement, 'list');
    } else {
      collapsed = true;
    }

    // We set overflow to hidden so that the most visited element does not
    // "leak" when we hide and show it.
    if (collapsed) {
      mostVisitedElement.style.overflow = 'hidden';
    }

    applyMostVisitedRects();

    // Only set overflow to visible if the element is shown.
    if (!collapsed) {
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

// Recently closed

function layoutRecentlyClosed() {
  var recentShown = shownSections & Section.RECENT;
  updateSimpleSection('recently-closed', Section.RECENT);

  if (recentShown) {
    var recentElement = $('recently-closed');
    var style = recentElement.style;
    // We cannot use clientWidth here since the width has a transition.
    var spacing = 20;
    var headerEl = recentElement.firstElementChild;
    var navEl = recentElement.lastElementChild;
    var navWidth = navEl.offsetWidth;
    // Subtract 10 for the padding
    var availWidth = (useSmallGrid() ? 690 : 918) - navWidth - 10;

    // Now go backwards and hide as many elements as needed.
    var elementsToHide = [];
    for (var el = navEl.previousElementSibling; el != headerEl;
         el = el.previousElementSibling) {
      if (el.offsetLeft + el.offsetWidth + spacing > availWidth) {
        elementsToHide.push(el);
      }
    }

    elementsToHide.forEach(function(el) {
      el.parentNode.removeChild(el);
    });
  }
}

/**
 * This function is called by the backend whenever the sync status section
 * needs to be updated to reflect recent sync state changes. The backend passes
 * the new status information in the newMessage parameter. The state includes
 * the following:
 *
 * syncsectionisvisible: true if the sync section needs to show up on the new
 *                       tab page and false otherwise.
 * msgtype: represents the states - "error", "presynced" or "synced".
 * title: the header for the sync status section.
 * msg: the actual message (e.g. "Synced to foo@gmail.com").
 * linkisvisible: true if the link element should be visible within the sync
 *                section and false otherwise.
 * linktext: the text to display as the link in the sync status (only used if
 *           linkisvisible is true).
 * linkurlisset: true if an URL should be set as the href for the link and false
 *               otherwise. If this field is false, then clicking on the link
 *               will result in sending a message to the backend (see
 *               'SyncLinkClicked').
 * linkurl: the URL to use as the element's href (only used if linkurlisset is
 *          true).
 */
function syncMessageChanged(newMessage) {
  var syncStatusElement = $('sync-status');
  var style = syncStatusElement.style;
  $('sync-menu-item').style.display = 'block';

  // Hide the section if the message is emtpy.
  if (!newMessage['syncsectionisvisible'] || !(shownSections & Section.SYNC)) {
    style.display = 'none';
    return;
  }
  style.display = 'block';

  // Set the sync section background color based on the state.
  if (newMessage.msgtype == 'error') {
    style.backgroundColor = 'tomato';
  } else {
    style.backgroundColor = '';
  }

  // Set the text for the header and sync message.
  var titleElement = syncStatusElement.firstElementChild;
  titleElement.textContent = newMessage.title;
  var messageElement = titleElement.nextElementSibling;
  messageElement.textContent = newMessage.msg;

  // Remove what comes after the message
  while (messageElement.nextSibling) {
    syncStatusElement.removeChild(messageElement.nextSibling);
  }

  if (newMessage.linkisvisible) {
    var el;
    if (newMessage.linkurlisset) {
      // Use a link
      el = document.createElement('a');
      el.href = newMessage.linkurl;
    } else {
      el = document.createElement('button');
      el.className = 'link';
      el.addEventListener('click', syncSectionLinkClicked);
    }
    el.textContent = newMessage.linktext;
    syncStatusElement.appendChild(el);
    fixLinkUnderline(el);
  }
}

/**
 * Invoked when the link in the sync status section is clicked.
 */
function syncSectionLinkClicked(e) {
  chrome.send('SyncLinkClicked');
  e.preventDefault();
}

/**
 * Returns the text used for a recently closed window.
 * @param {number} numTabs Number of tabs in the window.
 * @return {string} The text to use.
 */
function formatTabsText(numTabs) {
  if (numTabs == 1)
    return localStrings.getString('closedwindowsingle');
  return localStrings.formatString('closedwindowmultiple', numTabs);
}

/**
 * We need both most visited and the shown sections to be considered loaded.
 * @return {boolean}
 */
function onDataLoaded() {
  if (gotMostVisited) {
    mostVisited.layout();
    loading = false;
    // Remove class name in a timeout so that changes done in this JS thread are
    // not animated.
    window.setTimeout(function() {
      removeClass(document.body, 'loading');
    }, 1);
  }
}

// Theme related

function themeChanged() {
  $('themecss').href = 'chrome://theme/css/newtab.css?' + Date.now();
  updateAttribution();
}

function updateAttribution() {
  $('attribution-img').src = 'chrome://theme/theme_ntp_attribution?' +
      Date.now();
}

function bookmarkBarAttached() {
  document.documentElement.setAttribute('bookmarkbarattached', 'true');
}

function bookmarkBarDetached() {
  document.documentElement.setAttribute('bookmarkbarattached', 'false');
}

function viewLog() {
  var lines = [];
  var start = log[0][1];

  for (var i = 0; i < log.length; i++) {
    lines.push((log[i][1] - start) + ': ' + log[i][0]);
  }

  console.log(lines.join('\n'));
}

// Updates the visibility of the menu items.
function updateOptionMenu() {
  var menuItems = $('option-menu').children;
  for (var i = 0; i < menuItems.length; i++) {
    var item = menuItems[i];
    var command = item.getAttribute('command');
    if (command == 'show' || command == 'hide') {
      var section = Section[item.getAttribute('section')];
      var visible;
      if (section == Section.THUMB || section == Section.LIST) {
        visible = shownSections & Section.THUMB || shownSections & Section.LIST;
        // If visible we need to make sure we are hiding the visible section.
        if (visible) {
          item.setAttribute('section',
                            shownSections & Section.THUMB ? 'THUMB' : 'LIST');
        }
      } else {
        visible = shownSections & section;
      }
      item.setAttribute('command', visible ? 'hide' : 'show');
    }
  }
}

// We apply the size class here so that we don't trigger layout animations
// onload.

handleWindowResize();

var localStrings = new LocalStrings();

///////////////////////////////////////////////////////////////////////////////
// Things we know are not needed at startup go below here

function afterTransition(f) {
  if (loading) {
    // Make sure we do not use a timer during load since it slows down the UI.
    f();
  } else {
    // The duration of all transitions are .15s
    window.setTimeout(f, 150);
  }
}

// Notification


var notificationTimeout;

function showNotification(text, actionText, opt_f, opt_delay) {
  var notificationElement = $('notification');
  var f = opt_f || function() {};
  var delay = opt_delay || 10000;

  function show() {
    window.clearTimeout(notificationTimeout);
    addClass(notificationElement, 'show');
  }

  function delayedHide() {
    notificationTimeout = window.setTimeout(hideNotification, delay);
  }

  function doAction() {
    f();
    hideNotification();
  }

  // Remove any possible first-run trails.
  removeClass(notification, 'first-run');

  var actionLink = notificationElement.querySelector('.link-color');
  notificationElement.firstElementChild.textContent = text;
  actionLink.textContent = actionText;

  actionLink.onclick = doAction;
  actionLink.onkeydown = handleIfEnterKey(doAction);
  notificationElement.onmouseover = show;
  notificationElement.onmouseout = delayedHide;
  actionLink.onfocus = show;
  actionLink.onblur = delayedHide;
  // Enable tabbing to the link now that it is shown.
  actionLink.tabIndex = 0;

  show();
  delayedHide();
}

function hideNotification() {
  var notificationElement = $('notification');
  removeClass(notificationElement, 'show');
  var actionLink = notificationElement.querySelector('.link-color');
  // Prevent tabbing to the hidden link.
  actionLink.tabIndex = -1;
}

function showFirstRunNotification() {
  showNotification(localStrings.getString('firstrunnotification'),
                   localStrings.getString('closefirstrunnotification'),
                   null, 30000);
  var notificationElement = $('notification');
  addClass(notification, 'first-run');
}


/**
 * This handles the option menu.
 * @param {Element} button The button element.
 * @param {Element} menu The menu element.
 * @constructor
 */
function OptionMenu(button, menu) {
  this.button = button;
  this.menu = menu;
  this.button.onmousedown = bind(this.handleMouseDown, this);
  this.button.onkeydown = bind(this.handleKeyDown, this);
  this.boundHideMenu_ = bind(this.hide, this);
  this.boundMaybeHide_ = bind(this.maybeHide_, this);
  this.menu.onmouseover = bind(this.handleMouseOver, this);
  this.menu.onmouseout = bind(this.handleMouseOut, this);
  this.menu.onmouseup = bind(this.handleMouseUp, this);
}

OptionMenu.prototype = {
  show: function() {
    updateOptionMenu();
    this.menu.style.display = 'block';
    addClass(this.button, 'open');
    this.button.focus();

    // Listen to document and window events so that we hide the menu when the
    // user clicks outside the menu or tabs away or the whole window is blurred.
    document.addEventListener('focus', this.boundMaybeHide_, true);
    document.addEventListener('mousedown', this.boundMaybeHide_, true);
  },

  hide: function() {
    this.menu.style.display = 'none';
    removeClass(this.button, 'open');
    this.setSelectedIndex(-1);

    document.removeEventListener('focus', this.boundMaybeHide_, true);
    document.removeEventListener('mousedown', this.boundMaybeHide_, true);
  },

  isShown: function() {
    return this.menu.style.display == 'block';
  },

  /**
   * Callback for document mousedown and focus. It checks if the user tried to
   * navigate to a different element on the page and if so hides the menu.
   * @param {Event} e The mouse or focus event.
   * @private
   */
  maybeHide_: function(e) {
    if (!this.menu.contains(e.target) && !this.button.contains(e.target)) {
      this.hide();
    }
  },

  handleMouseDown: function(e) {
    if (this.isShown()) {
      this.hide();
    } else {
      this.show();
    }
  },

  handleMouseOver: function(e) {
    var el = e.target;
    if (!el.hasAttribute('command')) {
      this.setSelectedIndex(-1);
    } else {
      var index = Array.prototype.indexOf.call(this.menu.children, el);
      this.setSelectedIndex(index);
    }
  },

  handleMouseOut: function(e) {
    this.setSelectedIndex(-1);
  },

  handleMouseUp: function(e) {
    var item = this.getSelectedItem();
    if (item) {
      this.executeItem(item);
    }
  },

  handleKeyDown: function(e) {
    var item = this.getSelectedItem();

    var self = this;
    function selectNextVisible(m) {
      var children = self.menu.children;
      var len = children.length;
      var i = self.selectedIndex_;
      if (i == -1 && m == -1) {
        // Edge case when we need to go the last item fisrt.
        i = 0;
      }
      while (true) {
        i = (i + m + len) % len;
        item = children[i];
        if (item && item.hasAttribute('command') &&
            item.style.display != 'none') {
          break;
        }
      }
      if (item) {
        self.setSelectedIndex(i);
      }
    }

    switch (e.keyIdentifier) {
      case 'Down':
        if (!this.isShown()) {
          this.show();
        }
        selectNextVisible(1);
        e.preventDefault();
        break;
      case 'Up':
        if (!this.isShown()) {
          this.show();
        }
        selectNextVisible(-1);
        e.preventDefault();
        break;
      case 'Esc':
      case 'U+001B': // Maybe this is remote desktop playing a prank?
        this.hide();
        break;
      case 'Enter':
      case 'U+0020': // Space
        if (this.isShown()) {
          if (item) {
            this.executeItem(item);
          } else {
            this.hide();
          }
        } else {
          this.show();
        }
        e.preventDefault();
        break;
    }
  },

  selectedIndex_: -1,
  setSelectedIndex: function(i) {
    if (i != this.selectedIndex_) {
      var items = this.menu.children;
      var oldItem = items[this.selectedIndex_];
      if (oldItem) {
        oldItem.removeAttribute('selected');
      }
      var newItem = items[i];
      if (newItem) {
        newItem.setAttribute('selected', 'selected');
      }
      this.selectedIndex_ = i;
    }
  },

  getSelectedItem: function() {
    return this.menu.children[this.selectedIndex_] || null;
  },

  executeItem: function(item) {
    var command = item.getAttribute('command');
    if (command in this.commands) {
      this.commands[command].call(this, item);
    }

    this.hide();
  }
};

var optionMenu = new OptionMenu($('option-button'), $('option-menu'));
optionMenu.commands = {
  'clear-all-blacklisted' : function() {
    mostVisited.clearAllBlacklisted();
    chrome.send('getMostVisited');
  },
  'show': function(item) {
    var section = Section[item.getAttribute('section')];
    showSection(section);
    saveShownSections();
  },
  'hide': function(item) {
    var section = Section[item.getAttribute('section')];
    hideSection(section);
    saveShownSections();
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

function handleIfEnterKey(f) {
  return function(e) {
    if (e.keyIdentifier == 'Enter') {
      f(e);
    }
  };
}

function maybeReopenTab(e) {
  var el = findAncestor(e.target, function(el) {
    return el.sessionId !== undefined;
  });
  if (el) {
    chrome.send('reopenTab', [String(el.sessionId)]);
    e.preventDefault();

    // HACK(arv): After the window onblur event happens we get a mouseover event
    // on the next item and we want to make sure that we do not show a tooltip
    // for that.
    window.setTimeout(function() {
      windowTooltip.hide();
    }, 2 * WindowTooltip.DELAY);
  }
}

function maybeShowWindowTooltip(e) {
  var f = function(el) {
    return el.tabItems !== undefined;
  };
  var el = findAncestor(e.target, f);
  var relatedEl = findAncestor(e.relatedTarget, f);
  if (el && el != relatedEl) {
    windowTooltip.handleMouseOver(e, el, el.tabItems);
  }
}


var recentlyClosedElement = $('recently-closed');

recentlyClosedElement.addEventListener('click', maybeReopenTab);
recentlyClosedElement.addEventListener('keydown',
                                       handleIfEnterKey(maybeReopenTab));

recentlyClosedElement.addEventListener('mouseover', maybeShowWindowTooltip);
recentlyClosedElement.addEventListener('focus', maybeShowWindowTooltip, true);

/**
 * This object represents a tooltip representing a closed window. It is
 * shown when hovering over a closed window item or when the item is focused. It
 * gets hidden when blurred or when mousing out of the menu or the item.
 * @param {Element} tooltipEl The element to use as the tooltip.
 * @constructor
 */
function WindowTooltip(tooltipEl) {
  this.tooltipEl = tooltipEl;
  this.boundHide_ = bind(this.hide, this);
  this.boundHandleMouseOut_ = bind(this.handleMouseOut, this);
}

WindowTooltip.trackMouseMove_ = function(e) {
  WindowTooltip.clientX = e.clientX;
  WindowTooltip.clientY = e.clientY;
};

/**
 * Time in ms to delay before the tooltip is shown.
 * @type {number}
 */
WindowTooltip.DELAY = 300;

WindowTooltip.prototype = {
  timer: 0,
  handleMouseOver: function(e, linkEl, tabs) {
    this.linkEl_ = linkEl;
    if (e.type == 'mouseover') {
      this.linkEl_.addEventListener('mousemove', WindowTooltip.trackMouseMove_);
      this.linkEl_.addEventListener('mouseout', this.boundHandleMouseOut_);
    } else { // focus
      this.linkEl_.addEventListener('blur', this.boundHide_);
    }
    this.timer = window.setTimeout(bind(this.show, this, e.type, linkEl, tabs),
                                   WindowTooltip.DELAY);
  },
  show: function(type, linkEl, tabs) {
    window.addEventListener('blur', this.boundHide_);
    this.linkEl_.removeEventListener('mousemove',
                                     WindowTooltip.trackMouseMove_);
    window.clearTimeout(this.timer);

    this.renderItems(tabs);
    var rect = linkEl.getBoundingClientRect();
    var bodyRect = document.body.getBoundingClientRect();
    var rtl = document.documentElement.dir == 'rtl';

    this.tooltipEl.style.display = 'block';
    var tooltipRect = this.tooltipEl.getBoundingClientRect();
    var x, y;

    // When focused show below, like a drop down menu.
    if (type == 'focus') {
      x = rtl ?
          rect.left + bodyRect.left + rect.width - this.tooltipEl.offsetWidth :
          rect.left + bodyRect.left;
      y = rect.top + bodyRect.top + rect.height;
    } else {
      x = bodyRect.left + (rtl ?
          WindowTooltip.clientX - this.tooltipEl.offsetWidth :
          WindowTooltip.clientX);
      // Offset like a tooltip
      y = 20 + WindowTooltip.clientY + bodyRect.top;
    }

    // We need to ensure that the tooltip is inside the window viewport.
    x = Math.min(x, bodyRect.width - tooltipRect.width);
    x = Math.max(x, 0);
    y = Math.min(y, bodyRect.height - tooltipRect.height);
    y = Math.max(y, 0);

    this.tooltipEl.style.left = x + 'px';
    this.tooltipEl.style.top = y + 'px';
  },
  handleMouseOut: function(e) {
    // Don't hide when move to another item in the link.
    var f = function(el) {
      return el.tabItems !== undefined;
    };
    var el = findAncestor(e.target, f);
    var relatedEl = findAncestor(e.relatedTarget, f);
    if (el && el != relatedEl) {
      this.hide();
    }
  },
  hide: function() {
    window.clearTimeout(this.timer);
    window.removeEventListener('blur', this.boundHide_);
    this.linkEl_.removeEventListener('mousemove',
                                     WindowTooltip.trackMouseMove_);
    this.linkEl_.removeEventListener('mouseout', this.boundHandleMouseOut_);
    this.linkEl_.removeEventListener('blur', this.boundHide_);
    this.linkEl_ = null;

    this.tooltipEl.style.display  = 'none';
  },
  renderItems: function(tabs) {
    var tooltip = this.tooltipEl;
    tooltip.textContent = '';

    tabs.forEach(function(tab) {
      var span = document.createElement('span');
      span.className = 'item';
      span.style.backgroundImage = url('chrome://favicon/' + tab.url);
      span.dir = tab.direction;
      span.textContent = tab.title;
      tooltip.appendChild(span);
    });
  }
};

var windowTooltip = new WindowTooltip($('window-tooltip'));

function getCheckboxHandler(section) {
  return function(e) {
    if (e.type == 'keydown') {
      if (e.keyIdentifier == 'Enter') {
        e.target.checked = !e.target.checked;
      } else {
        return;
      }
    }
    if (e.target.checked) {
      showSection(section);
    } else {
      hideSection(section);
    }
    saveShownSections();
  }
}

$('thumb-checkbox').addEventListener('change',
                                     getCheckboxHandler(Section.THUMB));
$('thumb-checkbox').addEventListener('keydown',
                                     getCheckboxHandler(Section.THUMB));
$('list-checkbox').addEventListener('change',
                                    getCheckboxHandler(Section.LIST));
$('list-checkbox').addEventListener('keydown',
                                    getCheckboxHandler(Section.LIST));

window.addEventListener('load', bind(logEvent, global, 'Tab.NewTabOnload', true));
window.addEventListener('load', onDataLoaded);

window.addEventListener('resize', handleWindowResize);
document.addEventListener('DOMContentLoaded',
    bind(logEvent, global, 'Tab.NewTabDOMContentLoaded', true));

// Whether or not we should send the initial 'GetSyncMessage' to the backend
// depends on the value of the attribue 'syncispresent' which the backend sets
// to indicate if there is code in the backend which is capable of processing
// this message. This attribute is loaded by the JSTemplate and therefore we
// must make sure we check the attribute after the DOM is loaded.
document.addEventListener('DOMContentLoaded',
                          callGetSyncMessageIfSyncIsPresent);

// This link allows user to make new tab page as homepage from the new tab
// page itself (without going to Options dialog box).
document.addEventListener('DOMContentLoaded', showSetAsHomePageLink);

/**
 * The sync code is not yet built by default on all platforms so we have to
 * make sure we don't send the initial sync message to the backend unless the
 * backend told us that the sync code is present.
 */
function callGetSyncMessageIfSyncIsPresent() {
  if (document.documentElement.getAttribute('syncispresent') == 'true') {
    chrome.send('GetSyncMessage');
  }
}

function setAsHomePageLinkClicked(e) {
  chrome.send('setHomePage');
  e.preventDefault();
}

function showSetAsHomePageLink() {
  var setAsHomePageElement = $('set-as-home-page');
  var style = setAsHomePageElement.style;
  if (document.documentElement.getAttribute('showsetashomepage') != 'true') {
    // Hide the section (if new tab page is already homepage).
    return;
  }

  style.display = 'block';
  var buttonElement = setAsHomePageElement.firstElementChild;
  buttonElement.addEventListener('click', setAsHomePageLinkClicked);
}

function onHomePageSet(data) {
  $('set-as-home-page').style.display = 'none';
  showNotification(data[0], data[1]);
}

function hideAllMenus() {
  optionMenu.hide();
}

window.addEventListener('blur', hideAllMenus);
window.addEventListener('keydown', function(e) {
  if (e.keyIdentifier == 'Alt' || e.keyIdentifier == 'Meta') {
    hideAllMenus();
  }
}, true);

// Tooltip for elements that have text that overflows.
document.addEventListener('mouseover', function(e) {
  // We don't want to do this while we are dragging because it makes things very
  // janky
  if (dnd.dragItem) {
    return;
  }

  var el = findAncestor(e.target, function(el) {
    return el.xtitle;
  });
  if (el && el.xtitle != el.title) {
    if (el.scrollWidth > el.clientWidth) {
      el.title = el.xtitle;
    } else {
      el.title = '';
    }
  }
});

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
    // WebKit fires dragend before drop.
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
      var self = this;
      this.dragEndTimer = window.setTimeout(function() {
        // These things needto happen after the drop event.
        mostVisited.invalidate();
        mostVisited.layout();
        self.dragItem = null;
      }, 10);
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

/**
 * Whitelist of tag names allowed in parseHtmlSubset.
 * @type {[string]}
 */
var allowedTags = ['A', 'B', 'STRONG'];

/**
 * Parse a very small subset of HTML.
 * @param {string} s The string to parse.
 * @throws {Error} In case of non supported markup.
 * @return {DocumentFragment} A document fragment containing the DOM tree.
 */
var allowedAttributes = {
  'href': function(node, value) {
    // Only allow a[href] starting with http:// and https://
    return node.tagName == 'A' && (value.indexOf('http://') == 0 ||
        value.indexOf('https://') == 0);
  }
}

/**
 * Parse a very small subset of HTML.  This ensures that insecure HTML /
 * javascript cannot be injected into the new tab page.
 * @param {string} s The string to parse.
 * @throws {Error} In case of non supported markup.
 * @return {DocumentFragment} A document fragment containing the DOM tree.
 */
function parseHtmlSubset(s) {
  function walk(n, f) {
    f(n);
    for (var i = 0; i < n.childNodes.length; i++) {
      walk(n.childNodes[i], f);
    }
  }

  function assertElement(node) {
    if (allowedTags.indexOf(node.tagName) == -1)
      throw Error(node.tagName + ' is not supported');
  }

  function assertAttribute(attrNode, node) {
    var n = attrNode.nodeName;
    var v = attrNode.nodeValue;
    if (!allowedAttributes.hasOwnProperty(n) || !allowedAttributes[n](node, v))
      throw Error(node.tagName + '[' + n + '="' + v + '"] is not supported');
  }

  var r = document.createRange();
  r.selectNode(document.body);
  // This does not execute any scripts.
  var df = r.createContextualFragment(s);
  walk(df, function(node) {
    switch (node.nodeType) {
      case Node.ELEMENT_NODE:
        assertElement(node);
        var attrs = node.attributes;
        for (var i = 0; i < attrs.length; i++) {
          assertAttribute(attrs[i], node);
        }
        break;

      case Node.COMMENT_NODE:
      case Node.DOCUMENT_FRAGMENT_NODE:
      case Node.TEXT_NODE:
        break;

      default:
        throw Error('Node type ' + node.nodeType + ' is not supported');
    }
  });
  return df;
}

/**
 * Makes links and buttons support a different underline color.
 * @param {Node} node The node to search for links and buttons in.
 */
function fixLinkUnderlines(node) {
  var elements = node.querySelectorAll('a,button');
  Array.prototype.forEach.call(elements, fixLinkUnderline);
}

/**
 * Wraps the content of an element in a a link-color span.
 * @param {Element} el The element to wrap.
 */
function fixLinkUnderline(el) {
  var span = document.createElement('span');
  span.className = 'link-color';
  while (el.hasChildNodes()) {
    span.appendChild(el.firstChild);
  }
  el.appendChild(span);
}

updateAttribution();
