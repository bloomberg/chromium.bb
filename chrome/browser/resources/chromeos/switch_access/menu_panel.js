// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Handles the Switch Access menu panel.
 * @implements {PanelInterface}
 */
class Panel {
  constructor() {
    /**
     * The menu manager.
     * @private {MenuManager}
     */
    this.menuManager_;
  }

  /**
   * Initialize the panel and buttons.
   */
  init() {
    const div = document.getElementById(Panel.MENU_ID);
    for (const button of div.children)
      this.setupButton_(button);

    const background = chrome.extension.getBackgroundPage();
    if (background.document.readyState === 'complete')
      this.connectToBackground();
    else
      background.addEventListener('load', this.connectToBackground.bind(this));
  }

  /**
   * Once both the menu panel and the background page have loaded, pass a
   * reference to this object for communication.
   */
  connectToBackground() {
    const switchAccess = chrome.extension.getBackgroundPage().switchAccess;
    this.menuManager_ = switchAccess.connectMenuPanel(this);
  }

  /**
   * Adds an event listener to the given button to send a message when clicked.
   * @param {!HTMLElement} button
   * @private
   */
  setupButton_(button) {
    let id = button.id;
    button.addEventListener('click', function() {
      MessageHandler.sendMessage(MessageHandler.Destination.BACKGROUND, id);
    }.bind(id));
  }

  /**
   * Temporary function, until multiple focus rings is implemented.
   * Puts a focus ring around the given menu item.
   * TODO(crbug/925103): Implement multiple focus rings.
   *
   * @param {string} id
   * @param {boolean} enable
   */
  setFocusRing(id, enable) {
    this.updateClass_(id, Panel.FOCUS_CLASS, enable);
    return;
  }

  /**
   * Sets which buttons are enabled/disabled, based on |actions|.
   * @param {!Array<string>} actions
   */
  setActions(actions) {
    const div = document.getElementById(Panel.MENU_ID);
    for (const button of div.children)
      button.hidden = !actions.includes(button.id);

    this.setHeight_(actions.length);
  }

  /**
   * Either adds or removes the class |className| for the element with the given
   * |id|.
   * @param {string} id
   * @param {string} className
   * @param {bool} shouldAdd
   */
  updateClass_(id, className, shouldAdd) {
    const htmlNode = document.getElementById(id);
    if (shouldAdd)
      htmlNode.classList.add(className);
    else
      htmlNode.classList.remove(className);
  }

  /**
   * Sets the height of the menu (minus the body padding) based on the number of
   * actions in the menu. This is necessary because floated elements do not
   * contribute to their parent's height, and the elements are floated to avoid
   * arbitrary space being added between buttons.
   *
   * @param {number} numActions
   */
  setHeight_(numActions) {
    // TODO(anastasi): This should be a preference that the user can change.
    const maxCols = 3;
    const numRows = Math.ceil(numActions / maxCols);
    const height = 60 * numRows;
    document.getElementById(Panel.MENU_ID).style.height = height + 'px';
  }
}

/**
 * This must be kept in sync with the div ID in menu_panel.html.
 * @type {string}
 */
Panel.MENU_ID = 'switchaccess_menu_actions';
/**
 * This must be kept in sync with the class name in menu_panel.css.
 * @type {string}
 */
Panel.FOCUS_CLASS = 'focus';

let switchAccessMenuPanel = new Panel();

if (document.readyState === 'complete')
  switchAccessMenuPanel.init();
else
  window.addEventListener(
      'load', switchAccessMenuPanel.init.bind(switchAccessMenuPanel));
