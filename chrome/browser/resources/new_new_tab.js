// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// To avoid creating tons of unnecessary nodes. We assume we cannot fit more
// than this many items in the miniview.
var MAX_MINIVIEW_ITEMS = 15;

// Extra spacing at the top of the layout.
var LAYOUT_SPACING_TOP = 25;

function getSectionCloseButton(sectionId) {
  return document.querySelector('#' + sectionId + ' .section-close-button');
}

function getSectionMenuButton(sectionId) {
  return $(sectionId + '-button');
}

function getSectionMenuButtonTextId(sectionId) {
  return sectionId.replace(/-/g, '');
}

function setSectionMenuMode(sectionId, section, menuModeEnabled, menuModeMask) {
  var el = $(sectionId);
  if (!menuModeEnabled) {
    // Because sections are collapsed when they are in menu mode, it is not
    // necessary to restore the maxiview here. It will happen if the section
    // header is clicked.
    // TODO(aa): Sections should maintain their collapse state when minimized.
    el.classList.remove('menu');
    shownSections &= ~menuModeMask;
  } else {
    if (section) {
      hideSection(section);  // To hide the maxiview.
    }
    el.classList.add('menu');
    shownSections |= menuModeMask;
  }
  layoutSections();
}

function clearClosedMenu(menu) {
  menu.innerHTML = '';
}

function addClosedMenuEntryWithLink(menu, a) {
  var span = document.createElement('span');
  a.className += ' item menuitem';
  span.appendChild(a);
  menu.appendChild(span);
}

function addClosedMenuEntry(menu, url, title, imageUrl) {
  var a = document.createElement('a');
  a.href = url;
  a.textContent = title;
  a.style.backgroundImage = 'url(' + imageUrl + ')';
  addClosedMenuEntryWithLink(menu, a);
}

function addClosedMenuFooter(menu, sectionId, mask, opt_section) {
  menu.appendChild(document.createElement('hr'));

  var span = document.createElement('span');
  var a = span.appendChild(document.createElement('a'));
  a.href = '';
  if (cr.isChromeOS) {
    a.textContent = localStrings.getString('expandMenu');
  } else {
    a.textContent =
        localStrings.getString(getSectionMenuButtonTextId(sectionId));
  }
  a.className = 'item';
  a.addEventListener(
      'click',
      function(e) {
        getSectionMenuButton(sectionId).hideMenu();
        e.preventDefault();
        setSectionMenuMode(sectionId, opt_section, false, mask);
        shownSections &= ~mask;
        saveShownSections();
      });
  menu.appendChild(span);
}

function initializeSection(sectionId, mask, opt_section) {
  var button = getSectionCloseButton(sectionId);
  button.addEventListener(
    'click',
    function() {
      setSectionMenuMode(sectionId, opt_section, true, mask);
      saveShownSections();
    });
}

function updateSimpleSection(id, section) {
  var elm = $(id);
  var maxiview = getSectionMaxiview(elm);
  var miniview = getSectionMiniview(elm);
  if (shownSections & section) {
    // The section is expanded, so the maxiview should be opaque (visible) and
    // the miniview should be hidden.
    elm.classList.remove('collapsed');
    if (maxiview) {
      maxiview.classList.remove('collapsed');
      maxiview.classList.add('opaque');
    }
    if (miniview)
      miniview.classList.remove('opaque');
  } else {
    // The section is collapsed, so the maxiview should be hidden and the
    // miniview should be opaque.
    elm.classList.add('collapsed');
    if (maxiview) {
      maxiview.classList.add('collapsed');
      maxiview.classList.remove('opaque');
    }
    if (miniview)
      miniview.classList.add('opaque');
  }
}

var sessionItems = [];

function foreignSessions(data) {
  logEvent('received foreign sessions');
  // We need to store the foreign sessions so we can update the layout on a
  // resize.
  sessionItems = data;
  renderForeignSessions();
  layoutSections();
}

function renderForeignSessions() {
  // Remove all existing items and create new items.
  var sessionElement = $('foreign-sessions');
  var parentSessionElement = sessionElement.lastElementChild;
  parentSessionElement.textContent = '';

  // For each client, create entries and append the lists together.
  sessionItems.forEach(function(item, i) {
    // TODO(zea): Get real client names. See crbug/59672.
    var name = 'Client ' + i;
    parentSessionElement.appendChild(createForeignSession(item, name));
  });

  layoutForeignSessions();
}

function layoutForeignSessions() {
  var sessionElement = $('foreign-sessions');
  // We cannot use clientWidth here since the width has a transition.
  var availWidth = useSmallGrid() ? 692 : 920;
  var parentSessEl = sessionElement.lastElementChild;

  if (parentSessEl.hasChildNodes()) {
    sessionElement.classList.remove('disabled');
  } else {
    sessionElement.classList.add('disabled');
  }
}

function createForeignSession(client, name) {
  // Vertically stack the windows in a client.
  var stack = document.createElement('div');
  stack.className = 'foreign-session-client item link';
  stack.textContent = name;
  stack.sessionTag = client[0].sessionTag;

  client.forEach(function(win, i) {
    // Create a window entry.
    var winSpan = document.createElement('span');
    var winEl = document.createElement('p');
    winEl.className = 'item link window';
    winEl.tabItems = win.tabs;
    winEl.tabIndex = 0;
    winEl.textContent = formatTabsText(win.tabs.length);
    winEl.xtitle = win.title;
    winEl.sessionTag = win.sessionTag;
    winEl.winNum = i;
    winEl.addEventListener('click', maybeOpenForeignWindow);
    winEl.addEventListener('keydown',
                           handleIfEnterKey(maybeOpenForeignWindow));
    winSpan.appendChild(winEl);

    // Sort tabs by MRU order
    win.tabs.sort(function(a, b) {
      return a.timestamp < b.timestamp;
    });

    // Create individual tab information.
    win.tabs.forEach(function(data) {
        var tabEl = document.createElement('a');
        tabEl.className = 'item link tab';
        tabEl.href = data.timestamp;
        tabEl.style.backgroundImage = url('chrome://favicon/' + data.url);
        tabEl.dir = data.direction;
        tabEl.textContent = data.title;
        tabEl.sessionTag = win.sessionTag;
        tabEl.winNum = i;
        tabEl.sessionId = data.sessionId;
        tabEl.addEventListener('click', maybeOpenForeignTab);
        tabEl.addEventListener('keydown',
                               handleIfEnterKey(maybeOpenForeignTab));

        winSpan.appendChild(tabEl);
    });

    // Append the window.
    stack.appendChild(winSpan);
  });
  return stack;
}

var recentItems = [];

function recentlyClosedTabs(data) {
  logEvent('received recently closed tabs');
  // We need to store the recent items so we can update the layout on a resize.
  recentItems = data;
  renderRecentlyClosed();
  layoutSections();
}

function renderRecentlyClosed() {
  // Remove all existing items and create new items.
  var recentElement = $('recently-closed');
  var parentEl = recentElement.lastElementChild;
  parentEl.textContent = '';
  var recentMenu = $('recently-closed-menu');
  clearClosedMenu(recentMenu);

  recentItems.forEach(function(item) {
    parentEl.appendChild(createRecentItem(item));
    addRecentMenuItem(recentMenu, item);
  });
  addClosedMenuFooter(recentMenu, 'recently-closed', MENU_RECENT);

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
  el.sessionTag = data.sessionTag;
  var wrapperEl = document.createElement('span');
  wrapperEl.appendChild(el);
  return wrapperEl;
}

function addRecentMenuItem(menu, data) {
  var isWindow = data.type == 'window';
  var a = document.createElement('a');
  if (isWindow) {
    a.textContent = formatTabsText(data.tabs.length);
    a.className = 'window';  // To get the icon from the CSS .window rule.
    a.href = '';  // To make underline show up.
  } else {
    a.href = data.url;
    a.style.backgroundImage = 'url(chrome://favicon/' + data.url + ')';
    a.textContent = data.title;
  }
  function clickHandler(e) {
    chrome.send('reopenTab', [String(data.sessionId)]);
    e.preventDefault();
  }
  a.addEventListener('click', clickHandler);
  addClosedMenuEntryWithLink(menu, a);
}

function saveShownSections() {
  chrome.send('setShownSections', [String(shownSections)]);
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

  // TODO(jstritar): Remove the small-layout class and revert back to the
  // @media (max-width) directive once http://crbug.com/70930 is fixed.
  var oldLayoutMode = layoutMode;
  var b = useSmallGrid();
  if (b) {
    layoutMode = LayoutMode.SMALL;
    document.body.classList.add('small-layout');
  } else {
    layoutMode = LayoutMode.NORMAL;
    document.body.classList.remove('small-layout');
  }

  if (layoutMode != oldLayoutMode){
    mostVisited.useSmallGrid = b;
    mostVisited.layout();
    apps.layout({force:true});
    renderRecentlyClosed();
    renderForeignSessions();
    updateAllMiniviewClippings();
  }

  layoutSections();
}

// Stores some information about each section necessary to layout. A new
// instance is constructed for each section on each layout.
function SectionLayoutInfo(section) {
  this.section = section;
  this.header = section.querySelector('h2');
  this.miniview = section.querySelector('.miniview');
  this.maxiview = getSectionMaxiview(section);
  this.expanded = this.maxiview && !section.classList.contains('collapsed');
  this.fixedHeight = this.section.offsetHeight;
  this.scrollingHeight = 0;

  if (this.expanded)
    this.scrollingHeight = this.maxiview.offsetHeight;
}

// Get all sections to be layed out.
SectionLayoutInfo.getAll = function() {
  var sections = document.querySelectorAll(
      '.section:not(.disabled):not(.menu)');
  var result = [];
  for (var i = 0, section; section = sections[i]; i++) {
    result.push(new SectionLayoutInfo(section));
  }
  return result;
};

// Ensure the miniview sections don't have any clipped items.
function updateMiniviewClipping(miniview) {
  var clipped = false;
  for (var j = 0, item; item = miniview.children[j]; j++) {
    item.style.display = '';
    if (clipped ||
        (item.offsetLeft + item.offsetWidth) > miniview.offsetWidth) {
      item.style.display = 'none';
      clipped = true;
    } else {
      item.style.display = '';
    }
  }
}

// Ensure none of the miniviews have any clipped items.
function updateAllMiniviewClippings() {
  var miniviews = document.querySelectorAll('.section.collapsed .miniview');
  for (var i = 0, miniview; miniview = miniviews[i]; i++) {
    updateMiniviewClipping(miniview);
  }
}

// Returns whether or not vertical scrollbars are present.
function hasScrollBars() {
  return window.innerHeight != document.body.clientHeight;
}

// Enables scrollbars (they will only show up if needed).
function showScrollBars() {
  document.body.classList.remove('noscroll');
}

// Hides all scrollbars.
function hideScrollBars() {
  document.body.classList.add('noscroll');
}

// Returns whether or not the sections are currently animating due to a
// section transition.
function isAnimating() {
  var de = document.documentElement;
  return de.getAttribute('enable-section-animations') == 'true';
}

// Layout the sections in a modified accordian. The header and miniview, if
// visible are fixed within the viewport. If there is an expanded section, its
// it scrolls.
//
// =============================
// | collapsed section         |  <- Any collapsed sections are fixed position.
// | and miniview              |
// |---------------------------|
// | expanded section          |
// |                           |  <- There can be one expanded section and it
// | and maxiview              |     is absolutely positioned so that it can
// |                           |     scroll "underneath" the fixed elements.
// |                           |
// |---------------------------|
// | another collapsed section |
// |---------------------------|
//
// We want the main frame scrollbar to be the one that scrolls the expanded
// region. To get this effect, we make the fixed elements position:fixed and the
// scrollable element position:absolute. We also artificially increase the
// height of the document so that it is possible to scroll down enough to
// display the end of the document, even with any fixed elements at the bottom
// of the viewport.
//
// There is a final twist: If the intrinsic height of the expanded section is
// less than the available height (because the window is tall), any collapsed
// sections sinch up and sit below the expanded section. This is so that we
// don't have a bunch of dead whitespace in the case of expanded sections that
// aren't very tall.
function layoutSections() {
  // While transitioning sections, we only want scrollbars to appear if they're
  // already present or the window is being resized (so there's no animation).
  if (!hasScrollBars() && isAnimating())
    hideScrollBars();

  var sections = SectionLayoutInfo.getAll();
  var expandedSection = null;
  var headerHeight = LAYOUT_SPACING_TOP;
  var footerHeight = 0;

  // Calculate the height of the fixed elements above the expanded section. Also
  // take note of the expanded section, if there is one.
  var i;
  var section;
  for (i = 0; section = sections[i]; i++) {
    headerHeight += section.fixedHeight;
    if (section.expanded) {
      expandedSection = section;
      i++;
      break;
    }
  }

  // Calculate the height of the fixed elements below the expanded section, if
  // any.
  for (; section = sections[i]; i++) {
    footerHeight += section.fixedHeight;
  }
  // Leave room for bottom bar if it's visible.
  footerHeight += $('closed-sections-bar').offsetHeight;


  // Determine the height to use for the expanded section. If there isn't enough
  // space to show the expanded section completely, this will be the available
  // height. Otherwise, we use the intrinsic height of the expanded section.
  var expandedSectionHeight;
  if (expandedSection) {
    var flexHeight = window.innerHeight - headerHeight - footerHeight;
    if (flexHeight < expandedSection.scrollingHeight) {
      expandedSectionHeight = flexHeight;

      // Also, artificially expand the height of the document so that we can see
      // the entire expanded section.
      //
      // TODO(aa): Where does this come from? It is the difference between what
      // we set document.body.style.height to and what
      // document.body.scrollHeight measures afterward. I expect them to be the
      // same if document.body has no margins.
      var fudge = 44;
      document.body.style.height =
          headerHeight +
          expandedSection.scrollingHeight +
          footerHeight +
          fudge +
          'px';
    } else {
      expandedSectionHeight = expandedSection.scrollingHeight;
      document.body.style.height = '';
    }
  } else {
    // We only set the document height when a section is expanded. If
    // all sections are collapsed, then get rid of the previous height.
    document.body.style.height = '';
  }

  // Now position all the elements.
  var y = LAYOUT_SPACING_TOP;
  for (i = 0, section; section = sections[i]; i++) {
    section.section.style.top = y + 'px';
    y += section.fixedHeight;

    if (section.maxiview) {
      if (section == expandedSection) {
        section.maxiview.style.top = y + 'px';
      } else {
        // The miniviews fade out gradually, so it may have height at this
        // point. We position the maxiview as if the miniview was not displayed
        // by subtracting off the miniview's total height (height + margin).
        var miniviewFudge = 40;  // miniview margin-bottom + margin-top
        var miniviewHeight = section.miniview.offsetHeight + miniviewFudge;
        section.maxiview.style.top = y - miniviewHeight + 'px';
      }
    }

    if (section.maxiview && section == expandedSection)
      updateMask(section.maxiview, expandedSectionHeight);

    if (section == expandedSection)
      y += expandedSectionHeight;
  }
  if (cr.isChromeOS)
    $('closed-sections-bar').style.top = y + 'px';

  updateMenuSections();
  updateAttributionDisplay(y);
}

function updateMask(maxiview, visibleHeightPx) {
  // We want to end up with 10px gradients at the top and bottom of
  // visibleHeight, but webkit-mask only supports expression in terms of
  // percentages.

  // We might not have enough room to do 10px gradients on each side. To get the
  // right effect, we don't want to make the gradients smaller, but make them
  // appear to mush into each other.
  var gradientHeightPx = Math.min(10, Math.floor(visibleHeightPx / 2));
  var gradientDestination = 'rgba(0,0,0,' + (gradientHeightPx / 10) + ')';

  var bottomSpacing = 15;
  var first = parseFloat(maxiview.style.top) / window.innerHeight;
  var second = first + gradientHeightPx / window.innerHeight;
  var fourth = first + (visibleHeightPx - bottomSpacing) / window.innerHeight;
  var third = fourth - gradientHeightPx / window.innerHeight;

  var gradientArguments = [
    'transparent',
    getColorStopString(first, 'transparent'),
    getColorStopString(second, gradientDestination),
    getColorStopString(third, gradientDestination),
    getColorStopString(fourth, 'transparent'),
    'transparent'
  ];

  var gradient = '-webkit-linear-gradient(' + gradientArguments.join(',') + ')';
  maxiview.style.WebkitMaskImage = gradient;
}

function getColorStopString(height, color) {
  // TODO(arv): The CSS3 gradient syntax allows px units so we should simplify
  // this to use pixels instead.
  return color + ' ' + height * 100 + '%';
}

// Updates the visibility of the menu buttons for each section, based on
// whether they are currently enabled and in menu mode.
function updateMenuSections() {
  var elms = document.getElementsByClassName('section');
  for (var i = 0, elm; elm = elms[i]; i++) {
    var button = getSectionMenuButton(elm.id);
    if (!button)
      continue;

    if (!elm.classList.contains('disabled') &&
        elm.classList.contains('menu')) {
      button.style.display = 'inline-block';
    } else {
      button.style.display = 'none';
    }
  }
}

window.addEventListener('resize', handleWindowResize);

var sectionToElementMap;
function getSectionElement(section) {
  if (!sectionToElementMap) {
    sectionToElementMap = {};
    for (var key in Section) {
      sectionToElementMap[Section[key]] =
          document.querySelector('.section[section=' + key + ']');
    }
  }
  return sectionToElementMap[section];
}

function getSectionMaxiview(section) {
  return $(section.id + '-maxiview');
}

function getSectionMiniview(section) {
  return section.querySelector('.miniview');
}

// You usually want to call |showOnlySection()| instead of this.
function showSection(section) {
  if (!(section & shownSections)) {
    shownSections |= section;
    var el = getSectionElement(section);
    if (el) {
      el.classList.remove('collapsed');

      var maxiview = getSectionMaxiview(el);
      if (maxiview) {
        maxiview.classList.remove('collapsing');
        maxiview.classList.remove('collapsed');
        // The opacity won't transition if you toggle the display property
        // at the same time. To get a fade effect, we set the opacity
        // asynchronously from another function, after the display is toggled.
        //   1) 'collapsed' (display: none, opacity: 0)
        //   2) none (display: block, opacity: 0)
        //   3) 'opaque' (display: block, opacity: 1)
        setTimeout(function () {
          maxiview.classList.add('opaque');
        }, 0);
      }

      var miniview = getSectionMiniview(el);
      if (miniview) {
        // The miniview is hidden immediately (no need to set this async).
        miniview.classList.remove('opaque');
      }
    }

    switch (section) {
      case Section.THUMB:
        mostVisited.visible = true;
        mostVisited.layout();
        break;
      case Section.APPS:
        apps.visible = true;
        apps.layout({disableAnimations:true});
        break;
    }
  }
}

// Show this section and hide all other sections - at most one section can
// be open at one time.
function showOnlySection(section) {
  for (var p in Section) {
    if (p == section)
      showSection(Section[p]);
    else
      hideSection(Section[p]);
  }
}

function hideSection(section) {
  if (section & shownSections) {
    shownSections &= ~section;

    switch (section) {
      case Section.THUMB:
        mostVisited.visible = false;
        mostVisited.layout();
        break;
      case Section.APPS:
        apps.visible = false;
        apps.layout();
        break;
    }

    var el = getSectionElement(section);
    if (el) {
      el.classList.add('collapsed');

      var maxiview = getSectionMaxiview(el);
      if (maxiview) {
        maxiview.classList.add(isDoneLoading() ? 'collapsing' : 'collapsed');
        maxiview.classList.remove('opaque');
      }

      var miniview = getSectionMiniview(el);
      if (miniview) {
        // We need to set this asynchronously to properly get the fade effect.
        setTimeout(function() {
          miniview.classList.add('opaque');
        }, 0);
        updateMiniviewClipping(miniview);
      }
    }
  }
}

window.addEventListener('webkitTransitionEnd', function(e) {
  if (e.target.classList.contains('collapsing')) {
    e.target.classList.add('collapsed');
    e.target.classList.remove('collapsing');
  }

  if (e.target.classList.contains('maxiview') ||
      e.target.classList.contains('miniview'))  {
    document.documentElement.removeAttribute('enable-section-animations');
    showScrollBars();
  }
});

/**
 * Callback when the shown sections changes in another NTP.
 * @param {number} newShownSections Bitmask of the shown sections.
 */
function setShownSections(newShownSections) {
  for (var key in Section) {
    if (newShownSections & Section[key])
      showSection(Section[key]);
    else
      hideSection(Section[key]);
  }
  setSectionMenuMode('apps', Section.APPS, newShownSections & MENU_APPS,
                     MENU_APPS);
  setSectionMenuMode('most-visited', Section.THUMB,
                     newShownSections & MENU_THUMB, MENU_THUMB);
  setSectionMenuMode('recently-closed', undefined,
                     newShownSections & MENU_RECENT, MENU_RECENT);
  layoutSections();
}

// Recently closed

function layoutRecentlyClosed() {
  var recentElement = $('recently-closed');
  var miniview = getSectionMiniview(recentElement);

  updateMiniviewClipping(miniview);

  if (miniview.hasChildNodes()) {
    recentElement.classList.remove('disabled');
    miniview.classList.add('opaque');
  } else {
    recentElement.classList.add('disabled');
    miniview.classList.remove('opaque');
  }

  layoutSections();
}

/**
 * This function is called by the backend whenever the sync status section
 * needs to be updated to reflect recent sync state changes. The backend passes
 * the new status information in the newMessage parameter. The state includes
 * the following:
 *
 * syncsectionisvisible: true if the sync section needs to show up on the new
 *                       tab page and false otherwise.
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

  // Hide the section if the message is emtpy.
  if (!newMessage['syncsectionisvisible']) {
    syncStatusElement.classList.add('disabled');
    return;
  }

  syncStatusElement.classList.remove('disabled');

  var content = syncStatusElement.children[0];

  // Set the sync section background color based on the state.
  if (newMessage.msgtype == 'error') {
    content.style.backgroundColor = 'tomato';
  } else {
    content.style.backgroundColor = '';
  }

  // Set the text for the header and sync message.
  var titleElement = content.firstElementChild;
  titleElement.textContent = newMessage.title;
  var messageElement = titleElement.nextElementSibling;
  messageElement.textContent = newMessage.msg;

  // Remove what comes after the message
  while (messageElement.nextSibling) {
    content.removeChild(messageElement.nextSibling);
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
    content.appendChild(el);
    fixLinkUnderline(el);
  }

  layoutSections();
}

/**
 * Invoked when the link in the sync promo or sync status section is clicked.
 */
function syncSectionLinkClicked(e) {
  chrome.send('SyncLinkClicked');
  e.preventDefault();
}

/**
 * Invoked when link to start sync in the promo message is clicked, and Chrome
 * has already been synced to an account.
 */
function syncAlreadyEnabled(message) {
  showNotification(message.syncEnabledMessage,
                   localStrings.getString('close'));
}

/**
 * Returns the text used for a recently closed window.
 * @param {number} numTabs Number of tabs in the window.
 * @return {string} The text to use.
 */
function formatTabsText(numTabs) {
  if (numTabs == 1)
    return localStrings.getString('closedwindowsingle');
  return localStrings.getStringF('closedwindowmultiple', numTabs);
}

// Theme related

function themeChanged(hasAttribution) {
  document.documentElement.setAttribute('hasattribution', hasAttribution);
  $('themecss').href = 'chrome://theme/css/newtab.css?' + Date.now();
  updateAttribution();
}

function updateAttribution() {
  // Default value for standard NTP with no theme attribution or custom logo.
  logEvent('updateAttribution called');
  var imageId = 'IDR_PRODUCT_LOGO';
  // Theme attribution always overrides custom logos.
  if (document.documentElement.getAttribute('hasattribution') == 'true') {
    logEvent('updateAttribution called with THEME ATTR');
    imageId = 'IDR_THEME_NTP_ATTRIBUTION';
  } else if (document.documentElement.getAttribute('customlogo') == 'true') {
    logEvent('updateAttribution with CUSTOMLOGO');
    imageId = 'IDR_CUSTOM_PRODUCT_LOGO';
  }

  $('attribution-img').src = 'chrome://theme/' + imageId + '?' + Date.now();
}

// If the content overlaps with the attribution, we bump its opacity down.
function updateAttributionDisplay(contentBottom) {
  var attribution = $('attribution');
  var main = $('main');
  var rtl = document.documentElement.dir == 'rtl';
  var contentRect = main.getBoundingClientRect();
  var attributionRect = attribution.getBoundingClientRect();

  // Hack. See comments for '.haslayout' in new_new_tab.css.
  if (attributionRect.width == 0)
    return;
  else
    attribution.classList.remove('nolayout');

  if (contentBottom > attribution.offsetTop) {
    if ((!rtl && contentRect.right > attributionRect.left) ||
        (rtl && attributionRect.right > contentRect.left)) {
      attribution.classList.add('obscured');
      return;
    }
  }

  attribution.classList.remove('obscured');
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

// We apply the size class here so that we don't trigger layout animations
// onload.

handleWindowResize();

var localStrings = new LocalStrings();

///////////////////////////////////////////////////////////////////////////////
// Things we know are not needed at startup go below here

function afterTransition(f) {
  if (!isDoneLoading()) {
    // Make sure we do not use a timer during load since it slows down the UI.
    f();
  } else {
    // The duration of all transitions are .15s
    window.setTimeout(f, 150);
  }
}

// Notification


var notificationTimeout;

/*
 * Displays a message (either a string or a document fragment) in the
 * notification slot at the top of the NTP.
 * @param {string|Node} message String or node to use as message.
 * @param {string} actionText The text to show as a link next to the message.
 * @param {function=} opt_f Function to call when the user clicks the action
 *                          link.
 * @param {number=} opt_delay The time in milliseconds before hiding the
 *                            notification.
 * @param {boolean=} close If true, show a close link next to the notification.
 */
function showNotification(message, actionText, opt_f, opt_delay, opt_close) {
// TODO(arv): Create a notification component.
  var notificationElement = $('notification');
  var f = opt_f || function() {};
  var delay = opt_delay || 10000;

  function show() {
    window.clearTimeout(notificationTimeout);
    notificationElement.classList.add('show');
    document.body.classList.add('notification-shown');
  }

  function delayedHide() {
    notificationTimeout = window.setTimeout(hideNotification, delay);
  }

  function doAction() {
    f();
    hideNotification();
  }

  function closeNotification() {
    chrome.send('closePromo');
    hideNotification();
  }

  // Remove classList entries from previous notifications.
  notification.classList.remove('first-run');
  notification.classList.remove('promo');

  var messageContainer = notificationElement.firstElementChild;
  var actionLink = notificationElement.querySelector('#action-link');

  if (opt_close) {
    var closeLink = notificationElement.querySelector('#close-link');
    closeLink.textContent =
        localStrings.getString('closefirstrunnotification');
    closeLink.onclick = closeNotification;
    closeLink.onkeydown = handleIfEnterKey(closeNotification);
    closeLink.tabIndex = 1;
  }

  if (typeof message == 'string') {
    messageContainer.textContent = message;
  } else {
    messageContainer.textContent = '';  // Remove all children.
    messageContainer.appendChild(message);
  }

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

/**
 * Hides the notifier.
 */
function hideNotification() {
  var notificationElement = $('notification');
  notificationElement.classList.remove('show');
  document.body.classList.remove('notification-shown');
  var actionLink = notificationElement.querySelector('#actionlink');
  var closeLink = notificationElement.querySelector('#closelink');
  // Prevent tabbing to the hidden link.
  actionLink.tabIndex = -1;
  closeLink.tabIndex = -1;
  // Setting tabIndex to -1 only prevents future tabbing to it. If, however, the
  // user switches window or a tab and then moves back to this tab the element
  // may gain focus. We therefore make sure that we blur the element so that the
  // element focus is not restored when coming back to this window.
  actionLink.blur();
  closeLink.blur();
}

function showFirstRunNotification() {
  showNotification(localStrings.getString('firstrunnotification'),
                   localStrings.getString('closefirstrunnotification'),
                   null, 30000);
  var notificationElement = $('notification');
  notification.classList.add('first-run');
}

function showPromoNotification() {
  showNotification(parseHtmlSubset(localStrings.getString('serverpromo')),
                   localStrings.getString('syncpromotext'),
                   function () { chrome.send('SyncLinkClicked'); },
                   60000,
                   true);
  var notificationElement = $('notification');
  notification.classList.add('promo');
}

$('main').addEventListener('click', function(e) {
  var p = e.target;
  while (p && p.tagName != 'H2') {
    // In case the user clicks on a button we do not want to expand/collapse a
    // section.
    if (p.tagName == 'BUTTON')
      return;
    p = p.parentNode;
  }

  if (!p)
    return;

  p = p.parentNode;
  if (!getSectionMaxiview(p))
    return;

  toggleSectionVisibilityAndAnimate(p.getAttribute('section'));
});

$('most-visited-settings').addEventListener('click', function() {
  $('clear-all-blacklisted').execute();
});

function toggleSectionVisibilityAndAnimate(section) {
  if (!section)
    return;

  // It looks better to return the scroll to the top when toggling sections.
  document.body.scrollTop = 0;

  // We set it back in webkitTransitionEnd.
  document.documentElement.setAttribute('enable-section-animations', 'true');
  if (shownSections & Section[section]) {
    hideSection(Section[section]);
  } else {
    showOnlySection(section);
  }
  layoutSections();
  saveShownSections();
}

function handleIfEnterKey(f) {
  return function(e) {
    if (e.keyIdentifier == 'Enter')
      f(e);
  };
}

function maybeReopenTab(e) {
  var el = findAncestor(e.target, function(el) {
    return el.sessionId !== undefined;
  });
  if (el) {
    chrome.send('reopenTab', [String(el.sessionId)]);
    e.preventDefault();

    setWindowTooltipTimeout();
  }
}

// Note that the openForeignSession calls can fail, resulting this method to
// not have any action (hence the maybe).
function maybeOpenForeignSession(e) {
  var el = findAncestor(e.target, function(el) {
    return el.sessionTag !== undefined;
  });
  if (el) {
    chrome.send('openForeignSession', [String(el.sessionTag)]);
    e.stopPropagation();
    e.preventDefault();
    setWindowTooltipTimeout();
  }
}

function maybeOpenForeignWindow(e) {
  var el = findAncestor(e.target, function(el) {
    return el.winNum !== undefined;
  });
  if (el) {
    chrome.send('openForeignSession', [String(el.sessionTag),
        String(el.winNum)]);
    e.stopPropagation();
    e.preventDefault();
    setWindowTooltipTimeout();
  }
}

function maybeOpenForeignTab(e) {
  var el = findAncestor(e.target, function(el) {
    return el.sessionId !== undefined;
  });
  if (el) {
    chrome.send('openForeignSession', [String(el.sessionTag), String(el.winNum),
        String(el.sessionId)]);
    e.stopPropagation();
    e.preventDefault();
    setWindowTooltipTimeout();
  }
}

// HACK(arv): After the window onblur event happens we get a mouseover event
// on the next item and we want to make sure that we do not show a tooltip
// for that.
function setWindowTooltipTimeout(e) {
  window.setTimeout(function() {
    windowTooltip.hide();
  }, 2 * WindowTooltip.DELAY);
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

var foreignSessionElement = $('foreign-sessions');

foreignSessionElement.addEventListener('click', maybeOpenForeignSession);
foreignSessionElement.addEventListener('keydown',
                                       handleIfEnterKey(
                                           maybeOpenForeignSession));

foreignSessionElement.addEventListener('mouseover', maybeShowWindowTooltip);
foreignSessionElement.addEventListener('focus', maybeShowWindowTooltip, true);

/**
 * This object represents a tooltip representing a closed window. It is
 * shown when hovering over a closed window item or when the item is focused. It
 * gets hidden when blurred or when mousing out of the menu or the item.
 * @param {Element} tooltipEl The element to use as the tooltip.
 * @constructor
 */
function WindowTooltip(tooltipEl) {
  this.tooltipEl = tooltipEl;
  this.boundHide_ = this.hide.bind(this);
  this.boundHandleMouseOut_ = this.handleMouseOut.bind(this);
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
    this.timer = window.setTimeout(this.show.bind(this, e.type, linkEl, tabs),
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

window.addEventListener('load',
                        logEvent.bind(global, 'Tab.NewTabOnload', true));

window.addEventListener('resize', handleWindowResize);
document.addEventListener('DOMContentLoaded',
    logEvent.bind(global, 'Tab.NewTabDOMContentLoaded', true));

// Whether or not we should send the initial 'GetSyncMessage' to the backend
// depends on the value of the attribue 'syncispresent' which the backend sets
// to indicate if there is code in the backend which is capable of processing
// this message. This attribute is loaded by the JSTemplate and therefore we
// must make sure we check the attribute after the DOM is loaded.
document.addEventListener('DOMContentLoaded',
                          callGetSyncMessageIfSyncIsPresent);

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

// Tooltip for elements that have text that overflows.
document.addEventListener('mouseover', function(e) {
  // We don't want to do this while we are dragging because it makes things very
  // janky
  if (mostVisited.isDragging()) {
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

function initializeLogin() {
  chrome.send('initializeLogin', []);
}

function updateLogin(login) {
  $('login-container').style.display = login ? 'block' : '';
  if (login)
    $('login-username').textContent = login;

}

var mostVisited = new MostVisited(
    $('most-visited-maxiview'),
    document.querySelector('#most-visited .miniview'),
    $('most-visited-menu'),
    useSmallGrid(),
    shownSections & Section.THUMB);

function mostVisitedPages(data, firstRun, hasBlacklistedUrls) {
  logEvent('received most visited pages');

  mostVisited.updateSettingsLink(hasBlacklistedUrls);
  mostVisited.data = data;
  mostVisited.layout();
  layoutSections();

  // Remove class name in a timeout so that changes done in this JS thread are
  // not animated.
  window.setTimeout(function() {
    mostVisited.ensureSmallGridCorrect();
    maybeDoneLoading();
  }, 1);

  // Only show the first run notification if first run.
  if (firstRun) {
    showFirstRunNotification();
  } else if (localStrings.getString('serverpromo')) {
    showPromoNotification();
  }
}

function maybeDoneLoading() {
  if (mostVisited.data && apps.loaded)
    document.body.classList.remove('loading');
}

function isDoneLoading() {
  return !document.body.classList.contains('loading');
}

// Initialize the apps promo.
document.addEventListener('DOMContentLoaded', function() {
  var promoLink = document.querySelector('#apps-promo-text1 a');
  promoLink.id = 'apps-promo-link';
  promoLink.href = localStrings.getString('web_store_url');

  $('apps-promo-hide').addEventListener('click', function() {
    chrome.send('hideAppsPromo', []);
    document.documentElement.classList.remove('apps-promo-visible');
    layoutSections();
  });
});
