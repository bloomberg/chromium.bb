// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Class to handle interactions with the Switch Access menu, including moving
 * through and selecting actions.
 */

class MenuManager {
  /**
   * @param {!NavigationManager} navigationManager
   * @param {!chrome.automation.AutomationNode} desktop
   */
  constructor(navigationManager, desktop) {
    /**
     * A list of the Menu actions that are currently enabled.
     * @private {!Array<MenuManager.Action>}
     */
    this.actions_ = [];

    /**
     * The parent automation manager.
     * @private {!NavigationManager}
     */
    this.navigationManager_ = navigationManager;

    /**
     * The root node of the screen.
     * @private {!chrome.automation.AutomationNode}
     */
    this.desktop_ = desktop;

    /**
     * The root node of the menu.
     * @private {chrome.automation.AutomationNode}
     */
    this.menuNode_;

    /**
     * The current node of the menu.
     * @private {chrome.automation.AutomationNode}
     */
    this.node_;

    /**
     * Keeps track of when we're in the Switch Access menu.
     * @private {boolean}
     */
    this.inMenu_ = false;

    this.init_();
  }

  /**
   * Enter the menu and highlight the first available action.
   *
   * @param {!chrome.automation.AutomationNode} navNode the currently selected
   *        node, for which the menu is to be displayed.
   */
  enter(navNode) {
    const actions = this.getActionsForNode_(navNode);
    this.inMenu_ = true;
    if (actions !== this.actions_) {
      this.actions_ = actions;
      MessageHandler.sendMessage(
          MessageHandler.Destination.MENU_PANEL, 'setActions', actions);
    }

    this.node_ = this.menuNode();

    if (navNode.location)
      chrome.accessibilityPrivate.setSwitchAccessMenuState(
          true, navNode.location);
    else
      console.log('Unable to show Switch Access menu.');
    this.moveForward();
  }

  /**
   * Exits the menu.
   */
  exit() {
    this.clearFocusRing_();
    this.inMenu_ = false;
    if (this.node_)
      this.node_ = null;

    chrome.accessibilityPrivate.setSwitchAccessMenuState(
        false, MenuManager.EmptyLocation);
  }

  /**
   * Move to the next available action in the menu. If this is no next action,
   * focus the whole menu to loop again.
   * @return {boolean} Whether this function had any effect.
   */
  moveForward() {
    if (!this.node_ || !this.inMenu_)
      return false;

    this.clearFocusRing_();
    const treeWalker = new AutomationTreeWalker(
        this.node_, constants.Dir.FORWARD,
        SwitchAccessPredicate.restrictions(this.menuNode()));
    const node = treeWalker.next().node;
    if (!node)
      this.node_ = this.menuNode();
    else
      this.node_ = node;
    this.updateFocusRing_();
    return true;
  }

  /**
   * Move to the previous available action in the menu. If we're at the
   * beginning of the list, start again at the end.
   * @return {boolean} Whether this function had any effect.
   */
  moveBackward() {
    if (!this.node_ || !this.inMenu_)
      return false;

    this.clearFocusRing_();
    const treeWalker = new AutomationTreeWalker(
        this.node_, constants.Dir.BACKWARD,
        SwitchAccessPredicate.restrictions(this.menuNode()));
    let node = treeWalker.next().node;

    // If node is null, find the last enabled button.
    let lastChild = this.menuNode().lastChild;
    while (!node && lastChild) {
      if (SwitchAccessPredicate.isActionable(lastChild)) {
        node = lastChild;
        break;
      } else {
        lastChild = lastChild.previousSibling;
      }
    }

    this.node_ = node;
    this.updateFocusRing_();
    return true;
  }

  /**
   * Perform the action indicated by the current button (or no action if the
   * entire menu is selected). Then exit the menu and return to traditional
   * navigation.
   * @return {boolean} Whether this function had any effect.
   */
  selectCurrentNode() {
    if (!this.node_ || !this.inMenu_)
      return false;

    this.clearFocusRing_();
    this.node_.doDefault();
    this.exit();
    return true;
  }

  /**
   * Get the menu node. If it's not defined, search for it.
   * @return {!chrome.automation.AutomationNode}
   */
  menuNode() {
    if (this.menuNode_)
      return this.menuNode_;

    const treeWalker = new AutomationTreeWalker(
        this.desktop_, constants.Dir.FORWARD,
        SwitchAccessPredicate.switchAccessMenuDiscoveryRestrictions());
    const node = treeWalker.next().node;
    if (node) {
      this.menuNode_ = node;
      return this.menuNode_;
    }
    console.log('Unable to find the Switch Access menu.');
    return this.desktop_;
  }

  /**
   * Clear the focus ring.
   * @private
   */
  clearFocusRing_() {
    this.updateFocusRing_(true);
  }

  /**
   * Determines which menu actions are relevant, given the current node.
   * @param {!chrome.automation.AutomationNode} node
   * @return {!Array<MenuManager.Action>}
   * @private
   */
  getActionsForNode_(node) {
    let actions = [MenuManager.Action.SELECT, MenuManager.Action.OPTIONS];

    if (SwitchAccessPredicate.isTextInput(node))
      actions.push(MenuManager.Action.DICTATION);

    let scrollableAncestor = node;
    while (!scrollableAncestor.scrollable && scrollableAncestor.parent)
      scrollableAncestor = scrollableAncestor.parent;

    if (scrollableAncestor.scrollable) {
      // TODO(crbug/920659) This does not work for ARC++. Implement scroll
      // directions via standardActions.
      if (scrollableAncestor.scrollX > scrollableAncestor.scrollXMin)
        actions.push(MenuManager.Action.SCROLL_LEFT);
      if (scrollableAncestor.scrollX < scrollableAncestor.scrollXMax)
        actions.push(MenuManager.Action.SCROLL_RIGHT);
      if (scrollableAncestor.scrollY > scrollableAncestor.scrollYMin)
        actions.push(MenuManager.Action.SCROLL_UP);
      if (scrollableAncestor.scrollY < scrollableAncestor.scrollYMax)
        actions.push(MenuManager.Action.SCROLL_DOWN);
    }

    return actions;
  }

  /**
   * @private
   */
  init_() {
    // Listen for messages from the menu.
    window.addEventListener('message', this.onMessageReceived_.bind(this));
  }

  /**
   * Receive a message from the Switch Access menu, and perform the appropriate
   * action.
   * @private
   */
  onMessageReceived_(event) {
    this.exit();

    if (event.data === MenuManager.Action.SELECT)
      this.navigationManager_.selectCurrentNode();
    else if (event.data === MenuManager.Action.DICTATION)
      chrome.accessibilityPrivate.toggleDictation();
    else if (event.data === MenuManager.Action.OPTIONS)
      window.switchAccess.showOptionsPage();
    else if (
        event.data === MenuManager.Action.SCROLL_DOWN ||
        event.data === MenuManager.Action.SCROLL_UP ||
        event.data === MenuManager.Action.SCROLL_LEFT ||
        event.data === MenuManager.Action.SCROLL_RIGHT)
      this.navigationManager_.scroll(event.data);
  }

  /**
   * Send a message to the menu to update the focus ring around the current
   * node.
   * TODO(anastasi): Revisit focus rings before launch
   * @private
   * @param {boolean=} opt_clear If true, will clear the focus ring.
   */
  updateFocusRing_(opt_clear) {
    if (!this.node_)
      return;
    const id = this.node_.htmlAttributes.id;
    const onOrOff = opt_clear ? 'off' : 'on';
    MessageHandler.sendMessage(
        MessageHandler.Destination.MENU_PANEL, 'setFocusRing', [id, onOrOff]);
  }
}

/**
 * Actions available in the Switch Access Menu.
 * @enum {string}
 * @const
 */
MenuManager.Action = {
  DICTATION: 'dictation',
  // This opens the Switch Access settings in a new Chrome tab.
  OPTIONS: 'options',
  SCROLL_BACKWARD: 'scroll-backward',
  SCROLL_DOWN: 'scroll-down',
  SCROLL_FORWARD: 'scroll-forward',
  SCROLL_LEFT: 'scroll-left',
  SCROLL_RIGHT: 'scroll-right',
  SCROLL_UP: 'scroll-up',
  // This either performs the default action or enters a new scope, as
  // applicable.
  SELECT: 'select'
};

/**
 * The ID for the div containing the Switch Access menu.
 * @const
 */
MenuManager.MenuId = 'switchaccess_menu_actions';

/**
 * Empty location, used for hiding the menu.
 * @const
 */
MenuManager.EmptyLocation = {
  left: 0,
  top: 0,
  width: 0,
  height: 0
};
