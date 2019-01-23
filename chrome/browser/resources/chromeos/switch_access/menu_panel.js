// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Handles the Switch Access menu panel.
 */
const Panel = {
  /**
   * This must be kept in sync with the div ID in menu_panel.html.
   * @type {string}
   */
  MENU_ID: 'switchaccess_menu_actions',
  /**
   * This must be kept in sync with the class name in menu_panel.css.
   * @type {string}
   */
  FOCUS_CLASS: 'focus',

  /**
   * Keeps track of whether we have received a 'ready' from the background page.
   * @type {boolean}
   */
  readyReceived: false,
  /**
   * Keeps track of whether the context menu is loaded.
   * @type {boolean}
   */
  loaded: false,

  /**
   * Captures messages before the Panel is initialized.
   */
  preMessageHandler: () => {
    Panel.readyReceived = true;
    if (Panel.loaded)
      Panel.sendReady();
    window.removeEventListener('message', Panel.preMessageHandler);
  },

  /**
   * Initialize the panel and buttons.
   */
  init: () => {
    Panel.loaded = true;
    if (Panel.readyReceived)
      Panel.sendReady();

    const div = document.getElementById(Panel.MENU_ID);
    for (const button of div.children)
      Panel.setupButton(button);
    window.addEventListener('message', Panel.matchMessage);
  },

  /**
   * Sends a message to the background when both pages are loaded.
   */
  sendReady: () => {
    MessageHandler.sendMessage(
        MessageHandler.Destination.BACKGROUND, MessageHandler.READY);
  },

  /**
   * Adds an event listener to the given button to send a message when clicked.
   * @param {!HTMLElement} button
   */
  setupButton: (button) => {
    let id = button.id;
    button.addEventListener('click', function() {
      MessageHandler.sendMessage(MessageHandler.Destination.BACKGROUND, id);
    }.bind(id));
  },

  /**
   * Takes the given message and sees if it matches any expected pattern. If
   * it does, extract the parameters and pass them to the appropriate handler.
   * If not, log as an unrecognized action.
   *
   * @param {Object} message
   */
  matchMessage: (message) => {
    let matches = message.data.match(MessageHandler.SET_ACTIONS_REGEX);
    if (matches && matches.length === 2) {
      const actions = matches[1].split(',');
      Panel.setActions(actions);
      return;
    }
    matches = message.data.match(MessageHandler.SET_FOCUS_RING_REGEX);
    if (matches && matches.length === 3) {
      const id = matches[1];
      const shouldAdd = matches[2] === 'on';
      Panel.updateClass(id, Panel.FOCUS_CLASS, shouldAdd);
      return;
    }
    console.log('Action not recognized: ' + message.data);
  },

  /**
   * Sets which buttons are enabled/disabled, based on |actions|.
   * @param {!Array<string>} actions
   */
  setActions: (actions) => {
    const div = document.getElementById(Panel.MENU_ID);
    for (const button of div.children)
      button.hidden = !actions.includes(button.id);
  },

  /**
   * Either adds or removes the class |className| for the element with the given
   * |id|.
   * @param {string} id
   * @param {string} className
   * @param {bool} shouldAdd
   */
  updateClass: (id, className, shouldAdd) => {
    const htmlNode = document.getElementById(id);
    if (shouldAdd)
      htmlNode.classList.add(className);
    else
      htmlNode.classList.remove(className);
  }
};

window.addEventListener('message', Panel.preMessageHandler);
window.addEventListener('load', Panel.init);
