// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The ChromeVox panel and menus.
 */

goog.provide('Panel');

goog.require('BrailleCommandHandler');
goog.require('ISearchUI');
goog.require('Msgs');
goog.require('PanelCommand');
goog.require('PanelMenu');
goog.require('PanelMenuItem');
goog.require('Tutorial');
goog.require('cvox.ChromeVoxKbHandler');
goog.require('cvox.CommandStore');

/**
 * Class to manage the panel.
 * @constructor
 */
Panel = function() {};

/**
 * @enum {string}
 */
Panel.Mode = {
  COLLAPSED: 'collapsed',
  FOCUSED: 'focused',
  FULLSCREEN_MENUS: 'menus',
  FULLSCREEN_TUTORIAL: 'tutorial',
  SEARCH: 'search'
};

/**
 * @type {!Object<string, {title: string, location: (string|undefined)}>}
 */
Panel.ModeInfo = {
  collapsed: {title: 'panel_title', location: '#'},
  focused: {title: 'panel_title', location: '#focus'},
  menus: {title: 'panel_menus_title', location: '#fullscreen'},
  tutorial: {title: 'panel_tutorial_title', location: '#fullscreen'},
  search: {title: 'panel_title', location: '#focus'}
};

/**
 * A callback function to be executed to perform the action from selecting
 * a menu item after the menu has been closed and focus has been restored
 * to the page or wherever it was previously.
 * @param {?Function} callback
 */
Panel.setPendingCallback = function(callback) {
  /** @type {?Function} @private */
  Panel.pendingCallback_ = callback;
};

/**
 * Initialize the panel.
 */
Panel.init = function() {
  /** @type {Element} @private */
  this.speechContainer_ = $('speech-container');

  /** @type {Element} @private */
  this.speechElement_ = $('speech');

  /** @type {Element} @private */
  this.brailleContainer_ = $('braille-container');

  /** @type {Element} @private */
  this.searchContainer_ = $('search-container');

  /** @type {Element} @private */
  this.searchInput_ = $('search');

  /** @type {Element} @private */
  this.brailleTableElement_ = $('braille-table');
  this.brailleTableElement2_ = $('braille-table2');

  /** @type {Panel.Mode} @private */
  this.mode_ = Panel.Mode.COLLAPSED;

  var blockedSessionQuery =
      location.search.match(/[?&]?blockedUserSession=(true|false)/);
  /**
   * Whether the panel is loaded for blocked user session - e.g. on sign-in or
   * lock screen.
   * @type {boolean}
   * @private @const
   */
  this.isUserSessionBlocked_ =
      !!blockedSessionQuery && blockedSessionQuery[1] == 'true';

  /**
   * The array of top-level menus.
   * @type {!Array<PanelMenu>}
   * @private
   */
  this.menus_ = [];

  /**
   * The currently active menu, if any.
   * @type {PanelMenu}
   * @private
   */
  this.activeMenu_ = null;

  /**
   * True if the menu button in the panel is enabled at all. It's disabled if
   * ChromeVox Next is not active.
   * @type {boolean}
   * @private
   */
  this.menusEnabled_ =
      !this.isUserSessionBlocked_ && localStorage['useClassic'] == 'false';

  /**
   * @type {Tutorial}
   * @private
   */
  this.tutorial_ = new Tutorial();

  Panel.setPendingCallback(null);
  Panel.updateFromPrefs();

  Msgs.addTranslatedMessagesToDom(document);

  window.addEventListener('storage', function(event) {
    if (event.key == 'brailleCaptions') {
      Panel.updateFromPrefs();
    }
  }, false);

  window.addEventListener('message', function(message) {
    var command = JSON.parse(message.data);
    Panel.exec(/** @type {PanelCommand} */ (command));
  }, false);

  if (this.isUserSessionBlocked_) {
    $('menus_button').disabled = true;
    $('triangle').hidden = true;

    $('options').disabled = true;
  } else {
    $('menus_button').addEventListener('mousedown', Panel.onOpenMenus, false);
    $('options').addEventListener('click', Panel.onOptions, false);
  }

  $('close').addEventListener('click', Panel.onClose, false);

  $('tutorial_next').addEventListener('click', Panel.onTutorialNext, false);
  $('tutorial_previous')
      .addEventListener('click', Panel.onTutorialPrevious, false);
  $('close_tutorial').addEventListener('click', Panel.onCloseTutorial, false);

  document.addEventListener('keydown', Panel.onKeyDown, false);
  document.addEventListener('mouseup', Panel.onMouseUp, false);
  window.addEventListener('blur', function(evt) {
    if (evt.target != window || document.activeElement == document.body)
      return;

    Panel.closeMenusAndRestoreFocus();
  }, false);

  /** @type {Window} */
  Panel.ownerWindow = window;
};

/**
 * Update the display based on prefs.
 */
Panel.updateFromPrefs = function() {
  if (Panel.mode_ == Panel.Mode.SEARCH) {
    this.speechContainer_.hidden = true;
    this.brailleContainer_.hidden = true;
    this.searchContainer_.hidden = false;
    return;
  }

  this.speechContainer_.hidden = false;
  this.brailleContainer_.hidden = false;
  this.searchContainer_.hidden = true;

  if (localStorage['brailleCaptions'] === String(true)) {
    this.speechContainer_.style.visibility = 'hidden';
    this.brailleContainer_.style.visibility = 'visible';
  } else {
    this.speechContainer_.style.visibility = 'visible';
    this.brailleContainer_.style.visibility = 'hidden';
  }
};

/**
 * Execute a command to update the panel.
 *
 * @param {PanelCommand} command The command to execute.
 */
Panel.exec = function(command) {
  /**
   * Escape text so it can be safely added to HTML.
   * @param {*} str Text to be added to HTML, will be cast to string.
   * @return {string} The escaped string.
   */
  function escapeForHtml(str) {
    return String(str)
        .replace(/&/g, '&amp;')
        .replace(/</g, '&lt;')
        .replace(/\>/g, '&gt;')
        .replace(/"/g, '&quot;')
        .replace(/'/g, '&#039;')
        .replace(/\//g, '&#x2F;');
  }

  switch (command.type) {
    case PanelCommandType.CLEAR_SPEECH:
      this.speechElement_.innerHTML = '';
      break;
    case PanelCommandType.ADD_NORMAL_SPEECH:
      if (this.speechElement_.innerHTML != '') {
        this.speechElement_.innerHTML += '&nbsp;&nbsp;';
      }
      this.speechElement_.innerHTML +=
          '<span class="usertext">' + escapeForHtml(command.data) + '</span>';
      break;
    case PanelCommandType.ADD_ANNOTATION_SPEECH:
      if (this.speechElement_.innerHTML != '') {
        this.speechElement_.innerHTML += '&nbsp;&nbsp;';
      }
      this.speechElement_.innerHTML += escapeForHtml(command.data);
      break;
    case PanelCommandType.UPDATE_BRAILLE:
      Panel.onUpdateBraille(command.data);
      break;
    case PanelCommandType.ENABLE_MENUS:
      Panel.onEnableMenus();
      break;
    case PanelCommandType.DISABLE_MENUS:
      Panel.onDisableMenus();
      break;
    case PanelCommandType.OPEN_MENUS:
      Panel.onOpenMenus(undefined, command.data);
      break;
    case PanelCommandType.SEARCH:
      Panel.onSearch();
      break;
    case PanelCommandType.TUTORIAL:
      Panel.onTutorial();
      break;
    case PanelCommandType.UPDATE_NOTES:
      Panel.onTutorial('updateNotes');
      break;
  }
};

/**
 * Enable the ChromeVox Menus.
 */
Panel.onEnableMenus = function() {
  if (this.isUserSessionBlocked_)
    return;
  Panel.menusEnabled_ = true;
  $('menus_button').disabled = false;
  $('triangle').hidden = false;
};

/**
 * Disable the ChromeVox Menus.
 */
Panel.onDisableMenus = function() {
  if (this.isUserSessionBlocked_)
    return;
  Panel.menusEnabled_ = false;
  $('menus_button').disabled = true;
  $('triangle').hidden = true;
};

/**
 * Sets the mode, which determines the size of the panel and what objects
 *     are shown or hidden.
 * @param {Panel.Mode} mode The new mode.
 */
Panel.setMode = function(mode) {
  if (this.mode_ == mode)
    return;

  if (this.isUserSessionBlocked_ && mode != Panel.Mode.COLLAPSED &&
      mode != Panel.Mode.FOCUSED)
    return;
  this.mode_ = mode;

  document.title = Msgs.getMsg(Panel.ModeInfo[this.mode_].title);

  // Fully qualify the path here because this function might be called with a
  // window object belonging to the background page.
  Panel.ownerWindow.location =
      chrome.extension.getURL('cvox2/background/panel.html') +
      Panel.ModeInfo[this.mode_].location;

  $('main').hidden = (this.mode_ == Panel.Mode.FULLSCREEN_TUTORIAL);
  $('menus_background').hidden = (this.mode_ != Panel.Mode.FULLSCREEN_MENUS);
  $('tutorial').hidden = (this.mode_ != Panel.Mode.FULLSCREEN_TUTORIAL);
};

/**
 * Open / show the ChromeVox Menus.
 * @param {Event=} opt_event An optional event that triggered this.
 * @param {*=} opt_activateMenuTitle Title msg id of menu to open.
 */
Panel.onOpenMenus = function(opt_event, opt_activateMenuTitle) {
  // Don't open the menu if it's not enabled, such as when ChromeVox Next
  // is not active.
  if (!Panel.menusEnabled_)
    return;

  // Eat the event so that a mousedown isn't turned into a drag, allowing
  // users to click-drag-release to select a menu item.
  if (opt_event) {
    opt_event.stopPropagation();
    opt_event.preventDefault();
  }

  Panel.setMode(Panel.Mode.FULLSCREEN_MENUS);

  // Clear any existing menus and clear the callback.
  Panel.clearMenus();
  Panel.pendingCallback_ = null;

  // Build the top-level menus.
  var jumpMenu = Panel.addMenu('panel_menu_jump');
  var speechMenu = Panel.addMenu('panel_menu_speech');
  var tabsMenu = Panel.addMenu('panel_menu_tabs');
  var chromevoxMenu = Panel.addMenu('panel_menu_chromevox');

  // Create a mapping between categories from CommandStore, and our
  // top-level menus. Some categories aren't mapped to any menu.
  var categoryToMenu = {
    'navigation': jumpMenu,
    'jump_commands': jumpMenu,
    'overview': jumpMenu,
    'tables': jumpMenu,
    'controlling_speech': speechMenu,
    'information': speechMenu,
    'modifier_keys': chromevoxMenu,
    'help_commands': chromevoxMenu,

    'braille': null,
    'developer': null
  };

  // Get the key map from the background page.
  var bkgnd = chrome.extension.getBackgroundPage();
  var keymap = bkgnd['cvox']['KeyMap']['fromCurrentKeyMap']();

  // Make a copy of the key bindings, get the localized title of each
  // command, and then sort them.
  var sortedBindings = keymap.bindings().slice();
  sortedBindings.forEach(goog.bind(function(binding) {
    var command = binding.command;
    var keySeq = binding.sequence;
    binding.keySeq = cvox.KeyUtil.keySequenceToString(keySeq, true);
    var titleMsgId = cvox.CommandStore.messageForCommand(command);
    if (!titleMsgId) {
      console.error('No localization for: ' + command);
      binding.title = '';
      return;
    }
    var title = Msgs.getMsg(titleMsgId);
    // Convert to title case.
    title = title.replace(/\w\S*/g, function(word) {
      return word.charAt(0).toUpperCase() + word.substr(1);
    });
    binding.title = title;
  }, this));
  sortedBindings.sort(function(binding1, binding2) {
    return binding1.title.localeCompare(binding2.title);
  });

  // Insert items from the bindings into the menus.
  var sawBindingSet = {};
  sortedBindings.forEach(goog.bind(function(binding) {
    var command = binding.command;
    if (sawBindingSet[command])
      return;
    sawBindingSet[command] = true;
    var category = cvox.CommandStore.categoryForCommand(binding.command);
    var menu = category ? categoryToMenu[category] : null;
    if (binding.title && menu) {
      menu.addMenuItem(
          binding.title, binding.keySeq,
          BrailleCommandHandler.getDotShortcut(binding.command, true),
          function() {
            var CommandHandler =
                chrome.extension.getBackgroundPage()['CommandHandler'];
            CommandHandler['onCommand'](binding.command);
          });
    }
  }, this));

  // Add all open tabs to the Tabs menu.
  bkgnd.chrome.windows.getLastFocused(function(lastFocusedWindow) {
    bkgnd.chrome.windows.getAll({'populate': true}, function(windows) {
      for (var i = 0; i < windows.length; i++) {
        var tabs = windows[i].tabs;
        for (var j = 0; j < tabs.length; j++) {
          var title = tabs[j].title;
          if (tabs[j].active && windows[i].id == lastFocusedWindow.id)
            title += ' ' + Msgs.getMsg('active_tab');
          tabsMenu.addMenuItem(
              title, '', '', (function(win, tab) {
                               bkgnd.chrome.windows.update(
                                   win.id, {focused: true}, function() {
                                     bkgnd.chrome.tabs.update(
                                         tab.id, {active: true});
                                   });
                             }).bind(this, windows[i], tabs[j]));
        }
      }
    });
  });

  // Add a menu item that disables / closes ChromeVox.
  chromevoxMenu.addMenuItem(
      Msgs.getMsg('disable_chromevox'), 'Ctrl+Alt+Z', '', function() {
        Panel.onClose();
      });

  var roleListMenuMapping = [
    {menuTitle: 'role_heading', predicate: AutomationPredicate.heading},
    {menuTitle: 'role_landmark', predicate: AutomationPredicate.landmark},
    {menuTitle: 'role_link', predicate: AutomationPredicate.link},
    {menuTitle: 'role_form', predicate: AutomationPredicate.formField},
    {menuTitle: 'role_table', predicate: AutomationPredicate.table}
  ];

  var node = bkgnd.ChromeVoxState.instance.getCurrentRange().start.node;
  for (var i = 0; i < roleListMenuMapping.length; ++i) {
    var menuTitle = roleListMenuMapping[i].menuTitle;
    var predicate = roleListMenuMapping[i].predicate;
    // Create node menus asynchronously (because it may require searching a
    // long document) unless that's the specific menu the
    // user requested.
    var async = (menuTitle != opt_activateMenuTitle);
    Panel.addNodeMenu(menuTitle, node, predicate, async);
  }

  // Add actions menu if there are custom actions.
  if (node.customActions && node.customActions.length > 0) {
    var actionsMenu = Panel.addMenu('panel_menu_actions');
    for (var i = 0; i < node.customActions.length; i++) {
      var customAction = node.customActions[i];
      actionsMenu.addMenuItem(customAction.description,
          '' /* menuItemShortcut */, '' /* menuItemBraille */,
          node.performCustomAction.bind(node, customAction.id));
    }
  }

  // Activate either the specified menu or the first menu.
  var selectedMenu = Panel.menus_[0];
  for (var i = 0; i < Panel.menus_.length; i++) {
    if (this.menus_[i].menuMsg == opt_activateMenuTitle)
      selectedMenu = this.menus_[i];
  }
  Panel.activateMenu(selectedMenu);
};

/** Open incremental search. */
Panel.onSearch = function() {
  Panel.setMode(Panel.Mode.SEARCH);
  Panel.clearMenus();
  Panel.pendingCallback_ = null;
  Panel.updateFromPrefs();

  ISearchUI.init(Panel.searchInput_);
};

/**
 * Clear any previous menus. The menus are all regenerated each time the
 * menus are opened.
 */
Panel.clearMenus = function() {
  while (this.menus_.length) {
    var menu = this.menus_.pop();
    $('menu-bar').removeChild(menu.menuBarItemElement);
    $('menus_background').removeChild(menu.menuContainerElement);
  }
  this.activeMenu_ = null;
};

/**
 * Create a new menu with the given name and add it to the menu bar.
 * @param {string} menuMsg The msg id of the new menu to add.
 * @return {PanelMenu} The menu just created.
 */
Panel.addMenu = function(menuMsg) {
  var menu = new PanelMenu(menuMsg);
  $('menu-bar').appendChild(menu.menuBarItemElement);
  menu.menuBarItemElement.addEventListener('mouseover', function() {
    Panel.activateMenu(menu);
  }, false);

  $('menus_background').appendChild(menu.menuContainerElement);
  this.menus_.push(menu);
  return menu;
};

/**
 * Updates the content shown on the virtual braille display.
 * @param {*=} data The data sent through the PanelCommand.
 */
Panel.onUpdateBraille = function(data) {
  var groups = data.groups;
  var cols = data.cols;
  var rows = data.rows;
  var sideBySide = localStorage['brailleSideBySide'] === 'true';

  var addBorders = function(event) {
    var cell = event.target;
    if (cell.tagName == 'TD') {
      cell.className = 'highlighted-cell';
      var companionIDs = cell.getAttribute('data-companionIDs');
      companionIDs.split(' ').map(function(companionID) {
        var companion = $(companionID);
        companion.className = 'highlighted-cell';
      });
    }
  };

  var removeBorders = function(event) {
    var cell = event.target;
    if (cell.tagName == 'TD') {
      cell.className = 'unhighlighted-cell';
      var companionIDs = cell.getAttribute('data-companionIDs');
      companionIDs.split(' ').map(function(companionID) {
        var companion = $(companionID);
        companion.className = 'unhighlighted-cell';
      });
    }
  };

  this.brailleContainer_.addEventListener('mouseover', addBorders);
  this.brailleContainer_.addEventListener('mouseout', removeBorders);

  // Clear the tables.
  var rowCount = this.brailleTableElement_.rows.length;
  for (var i = 0; i < rowCount; i++) {
    this.brailleTableElement_.deleteRow(0);
  }
  rowCount = this.brailleTableElement2_.rows.length;
  for (var i = 0; i < rowCount; i++) {
    this.brailleTableElement2_.deleteRow(0);
  }

  var row1, row2;
  // Number of rows already written.
  rowCount = 0;
  // Number of cells already written in this row.
  var cellCount = cols;
  for (var i = 0; i < groups.length; i++) {
    if (cellCount == cols) {
      cellCount = 0;
      // Check if we reached the limit on the number of rows we can have.
      if (rowCount == rows)
        break;
      rowCount++;
      row1 = this.brailleTableElement_.insertRow(-1);
      if (sideBySide) {
        // Side by side.
        row2 = this.brailleTableElement2_.insertRow(-1);
      } else {
        // Interleaved.
        row2 = this.brailleTableElement_.insertRow(-1);
      }
    }

    var topCell = row1.insertCell(-1);
    topCell.innerHTML = groups[i][0];
    topCell.id = i + '-textCell';
    topCell.setAttribute('data-companionIDs', i + '-brailleCell');
    topCell.className = 'unhighlighted-cell';

    var bottomCell = row2.insertCell(-1);
    bottomCell.id = i + '-brailleCell';
    bottomCell.setAttribute('data-companionIDs', i + '-textCell');
    bottomCell.className = 'unhighlighted-cell';
    if (cellCount + groups[i][1].length > cols) {
      var brailleText = groups[i][1];
      while (cellCount + brailleText.length > cols) {
        // At this point we already have a bottomCell to fill, so fill it.
        bottomCell.innerHTML = brailleText.substring(0, cols - cellCount);
        // Update to see what we still have to fill.
        brailleText = brailleText.substring(cols - cellCount);
        // Make new row.
        if (rowCount == rows)
          break;
        rowCount++;
        row1 = this.brailleTableElement_.insertRow(-1);
        if (sideBySide) {
          // Side by side.
          row2 = this.brailleTableElement2_.insertRow(-1);
        } else {
          // Interleaved.
          row2 = this.brailleTableElement_.insertRow(-1);
        }
        var bottomCell2 = row2.insertCell(-1);
        bottomCell2.id = i + '-brailleCell2';
        bottomCell2.setAttribute(
            'data-companionIDs', i + '-textCell ' + i + '-brailleCell');
        bottomCell.setAttribute(
            'data-companionIDs',
            bottomCell.getAttribute('data-companionIDs') + ' ' + i +
                '-brailleCell2');
        topCell.setAttribute(
            'data-companionID2',
            bottomCell.getAttribute('data-companionIDs') + ' ' + i +
                '-brailleCell2');

        bottomCell2.className = 'unhighlighted-cell';
        bottomCell = bottomCell2;
        cellCount = 0;
      }
      // Fill the rest.
      bottomCell.innerHTML = brailleText;
      cellCount = brailleText.length;
    } else {
      bottomCell.innerHTML = groups[i][1];
      cellCount += groups[i][1].length;
    }
  }
};



/**
 * Create a new node menu with the given name and add it to the menu bar.
 * @param {string} menuMsg The msg id of the new menu to add.
 * @param {!chrome.automation.AutomationNode} node
 * @param {AutomationPredicate.Unary} pred
 * @param {boolean} defer If true, defers populating the menu.
 * @return {PanelMenu} The menu just created.
 */
Panel.addNodeMenu = function(menuMsg, node, pred, defer) {
  var menu = new PanelNodeMenu(menuMsg, node, pred, defer);
  $('menu-bar').appendChild(menu.menuBarItemElement);
  menu.menuBarItemElement.addEventListener('mouseover', function() {
    Panel.activateMenu(menu);
  }, false);

  $('menus_background').appendChild(menu.menuContainerElement);
  this.menus_.push(menu);
  return menu;
};

/**
 * Activate a menu, which implies hiding the previous active menu.
 * @param {PanelMenu} menu The new menu to activate.
 */
Panel.activateMenu = function(menu) {
  if (menu == this.activeMenu_)
    return;

  if (this.activeMenu_) {
    this.activeMenu_.deactivate();
    this.activeMenu_ = null;
  }

  this.activeMenu_ = menu;
  this.pendingCallback_ = null;

  if (this.activeMenu_) {
    this.activeMenu_.activate();
  }
};

/**
 * Sets the index of the current active menu to be 0.
 */
Panel.scrollToTop = function() {
  this.activeMenu_.scrollToTop();
};

/**
 * Sets the index of the current active menu to be the last index.
 */
Panel.scrollToBottom = function() {
  this.activeMenu_.scrollToBottom();
};

/**
 * Advance the index of the current active menu by |delta|.
 * @param {number} delta The number to add to the active menu index.
 */
Panel.advanceActiveMenuBy = function(delta) {
  var activeIndex = -1;
  for (var i = 0; i < this.menus_.length; i++) {
    if (this.activeMenu_ == this.menus_[i]) {
      activeIndex = i;
      break;
    }
  }

  if (activeIndex >= 0) {
    activeIndex += delta;
    activeIndex = (activeIndex + this.menus_.length) % this.menus_.length;
  } else {
    if (delta >= 0)
      activeIndex = 0;
    else
      activeIndex = this.menus_.length - 1;
  }
  Panel.activateMenu(this.menus_[activeIndex]);
};

/**
 * Advance the index of the current active menu item by |delta|.
 * @param {number} delta The number to add to the active menu item index.
 */
Panel.advanceItemBy = function(delta) {
  if (this.activeMenu_)
    this.activeMenu_.advanceItemBy(delta);
};

/**
 * Called when the user releases the mouse button. If it's anywhere other
 * than on the menus button, close the menus and return focus to the page,
 * and if the mouse was released over a menu item, execute that item's
 * callback.
 * @param {Event} event The mouse event.
 */
Panel.onMouseUp = function(event) {
  if (!Panel.activeMenu_)
    return;

  var target = event.target;
  while (target && !target.classList.contains('menu-item')) {
    // Allow the user to click and release on the menu button and leave
    // the menu button.
    if (target.id == 'menus_button')
      return;

    target = target.parentElement;
  }

  if (target && Panel.activeMenu_)
    Panel.pendingCallback_ = Panel.activeMenu_.getCallbackForElement(target);
  Panel.closeMenusAndRestoreFocus();
};

/**
 * Called when a key is pressed. Handle arrow keys to navigate the menus,
 * Esc to close, and Enter/Space to activate an item.
 * @param {Event} event The key event.
 */
Panel.onKeyDown = function(event) {
  if (event.key == 'Escape' && Panel.mode_ == Panel.Mode.FULLSCREEN_TUTORIAL) {
    Panel.setMode(Panel.Mode.COLLAPSED);
    return;
  }

  // Events don't propagate correctly because blur places focus on body.
  if (Panel.mode_ == Panel.Mode.FULLSCREEN_TUTORIAL &&
      !Panel.tutorial_.onKeyDown(event))
    return;

  if (!Panel.activeMenu_)
    return;

  if (event.altKey || event.ctrlKey || event.metaKey || event.shiftKey)
    return;

  switch (event.key) {
    case 'ArrowLeft':
      Panel.advanceActiveMenuBy(-1);
      break;
    case 'ArrowRight':
      Panel.advanceActiveMenuBy(1);
      break;
    case 'ArrowUp':
      Panel.advanceItemBy(-1);
      break;
    case 'ArrowDown':
      Panel.advanceItemBy(1);
      break;
    case 'Escape':
      Panel.closeMenusAndRestoreFocus();
      break;
    case 'PageUp':
      Panel.advanceItemBy(10);
      break;
    case 'PageDown':
      Panel.advanceItemBy(-10);
      break;
    case 'Home':
      Panel.scrollToTop();
      break;
    case 'End':
      Panel.scrollToBottom();
      break;
    case 'Enter':
    case ' ':  // Space
      Panel.pendingCallback_ = Panel.getCallbackForCurrentItem();
      Panel.closeMenusAndRestoreFocus();
      break;
    default:
      // Don't mark this event as handled.
      return;
  }

  event.preventDefault();
  event.stopPropagation();
};

/**
 * Open the ChromeVox Options.
 */
Panel.onOptions = function() {
  var bkgnd =
      chrome.extension.getBackgroundPage()['ChromeVoxState']['instance'];
  bkgnd['showOptionsPage']();
  Panel.setMode(Panel.Mode.COLLAPSED);
};

/**
 * Exit ChromeVox.
 */
Panel.onClose = function() {
  // Change the url fragment to 'close', which signals the native code
  // to exit ChromeVox.
  Panel.ownerWindow.location =
      chrome.extension.getURL('cvox2/background/panel.html') + '#close';
};

/**
 * Get the callback for whatever item is currently selected.
 * @return {Function} The callback for the current item.
 */
Panel.getCallbackForCurrentItem = function() {
  if (this.activeMenu_)
    return this.activeMenu_.getCallbackForCurrentItem();
  return null;
};

/**
 * Close the menus and restore focus to the page. If a menu item's callback
 * was queued, execute it once focus is restored.
 */
Panel.closeMenusAndRestoreFocus = function() {
  // Watch for the next focus event.
  var onFocus = function(desktop, evt) {
    desktop.removeEventListener(chrome.automation.EventType.FOCUS, onFocus);
    if (Panel.pendingCallback_) {
      // Clear it before calling it, in case the callback itself triggers
      // another pending callback.
      var pendingCallback = Panel.pendingCallback_;
      Panel.pendingCallback_ = null;
      pendingCallback();
    }
  }.bind(this);

  var bkgnd = chrome.extension.getBackgroundPage();
  bkgnd.chrome.automation.getDesktop(function(desktop) {
    onFocus = /** @type {function(chrome.automation.AutomationEvent)} */ (
        onFocus.bind(this, desktop));
    desktop.addEventListener(chrome.automation.EventType.FOCUS, onFocus, true);

    // Make sure all menus are cleared to avoid bogous output when we re-open.
    Panel.clearMenus();

    // Make sure we're not in full-screen mode.
    Panel.setMode(Panel.Mode.COLLAPSED);

    this.activeMenu_ = null;
  });
};

/**
 * Open the tutorial.
 * @param {string=} opt_page Show a specific page.
 */
Panel.onTutorial = function(opt_page) {
  // Change the url fragment to 'fullscreen', which signals the native
  // host code to make the window fullscreen, revealing the menus.
  Panel.setMode(Panel.Mode.FULLSCREEN_TUTORIAL);

  switch (opt_page) {
    case 'updateNotes':
      Panel.tutorial_.updateNotes();
      break;
    default:
      Panel.tutorial_.lastViewedPage();
  }
};

/**
 * Move to the next page in the tutorial.
 */
Panel.onTutorialNext = function() {
  Panel.tutorial_.nextPage();
};

/**
 * Move to the previous page in the tutorial.
 */
Panel.onTutorialPrevious = function() {
  Panel.tutorial_.previousPage();
};

/**
 * Close the tutorial.
 */
Panel.onCloseTutorial = function() {
  Panel.setMode(Panel.Mode.COLLAPSED);
};

window.addEventListener('load', function() {
  Panel.init();

  switch (location.search.slice(1)) {
    case 'tutorial':
      Panel.onTutorial();
  }
}, false);

window.addEventListener('hashchange', function() {
  if (location.hash == '#fullscreen' || location.hash == '#focus') {
    this.originalStickyState_ = cvox.ChromeVox.isStickyPrefOn;
    cvox.ChromeVox.isStickyPrefOn = false;
  } else {
    cvox.ChromeVox.isStickyPrefOn = this.originalStickyState_;
  }
}, false);
