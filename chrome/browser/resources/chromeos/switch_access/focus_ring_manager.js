// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Class to handle focus rings.
 */
class FocusRingManager {
  /**
   * @param {!SwitchAccessInterface} switchAccess
   * @param {!BackButtonManager} backButtonManager
   */
  constructor(switchAccess, backButtonManager) {
    /**
     * A map of all the focus rings.
     * @private {!Map<SAConstants.Focus.ID,
     *     chrome.accessibilityPrivate.FocusRingInfo>}
     */
    this.rings_ = new Map();

    /**
     * Regex pattern to verify valid colors. Checks that the first character
     * is '#', followed by between 3 and 8 valid hex characters, and no other
     * characters (ignoring case).
     */
    this.colorPattern_ = /^#[0-9A-F]{3,8}$/i;

    /**
     * A reference to the Switch Access object.
     * @private {!SwitchAccessInterface}
     */
    this.switchAccess_ = switchAccess;

    /**
     * The back button manager.
     * @private {!BackButtonManager}
     */
    this.backButtonManager_ = backButtonManager;
  }

  /** Finishes setup of focus rings once the preferences are loaded. */
  onPrefsReady() {
    // Currently all focus rings share the same color.
    // TODO(anastasi): Make the primary color a preference.
    const color = SAConstants.Focus.PRIMARY_COLOR;

    // Create each focus ring.
    this.rings_.set(SAConstants.Focus.ID.PRIMARY, {
      id: SAConstants.Focus.ID.PRIMARY,
      rects: [],
      type: chrome.accessibilityPrivate.FocusType.SOLID,
      color: color,
      secondaryColor: SAConstants.Focus.SECONDARY_COLOR
    });
    this.rings_.set(SAConstants.Focus.ID.NEXT, {
      id: SAConstants.Focus.ID.NEXT,
      rects: [],
      type: chrome.accessibilityPrivate.FocusType.DASHED,
      color: color,
      secondaryColor: SAConstants.Focus.SECONDARY_COLOR
    });
    this.rings_.set(SAConstants.Focus.ID.TEXT, {
      id: SAConstants.Focus.ID.TEXT,
      rects: [],
      type: chrome.accessibilityPrivate.FocusType.DASHED,
      color: color,
      secondaryColor: SAConstants.Focus.SECONDARY_COLOR
    });
  }

  /**
   * Sets the focus ring color.
   * @param {!string} color
   */
  setColor(color) {
    if (this.colorPattern_.test(color) !== true) {
      const errorString = 'Problem setting focus ring color: color is not' +
          'a valid CSS color string.';
      throw new Error(errorString);
    }
    this.rings_.forEach((ring) => ring.color = color);
  }

  /**
   * Sets the primary and next focus rings based on the current primary and
   *     group nodes used for navigation.
   * @param {!chrome.automation.AutomationNode} primary
   * @param {!chrome.automation.AutomationNode} group
   */
  setFocusNodes(primary, group) {
    if (this.rings_.size === 0) return;

    if (primary === this.backButtonManager_.backButtonNode()) {
      this.backButtonManager_.show(group.location);

      this.rings_.get(SAConstants.Focus.ID.PRIMARY).rects = [];
      // Clear the dashed ring between transitions, as the animation is
      // distracting.
      this.rings_.get(SAConstants.Focus.ID.NEXT).rects = [];
      this.updateFocusRings_();

      this.rings_.get(SAConstants.Focus.ID.NEXT).rects = [group.location];
      this.updateFocusRings_();
      return;
    }
    this.backButtonManager_.hide();

    // If the primary node is a group, show its first child as the "next" focus.
    if (SwitchAccessPredicate.isGroup(primary, group)) {
      const firstChild = new AutomationTreeWalker(
                             primary, constants.Dir.FORWARD,
                             SwitchAccessPredicate.restrictions(primary))
                             .next()
                             .node;

      // Clear the dashed ring between transitions, as the animation is
      // distracting.
      this.rings_.get(SAConstants.Focus.ID.NEXT).rects = [];
      this.updateFocusRings_();

      let focusRect = primary.location;
      if (firstChild && firstChild.location) {
        // If the current element is not the back button, the focus rect should
        // expand to contain the child rect.
        focusRect = RectHelper.expandToFitWithPadding(
            SAConstants.Focus.GROUP_BUFFER, focusRect, firstChild.location);
        this.rings_.get(SAConstants.Focus.ID.NEXT).rects =
            [firstChild.location];
      }
      this.rings_.get(SAConstants.Focus.ID.PRIMARY).rects = [focusRect];
      this.updateFocusRings_();
      return;
    }

    this.rings_.get(SAConstants.Focus.ID.PRIMARY).rects = [primary.location];
    this.rings_.get(SAConstants.Focus.ID.NEXT).rects = [];
    this.updateFocusRings_();
  }

  /**
   * Clears all focus rings.
   */
  clearAll() {
    this.rings_.forEach((ring) => ring.rects = []);
    this.updateFocusRings_();
  }

  /**
   * Updates all focus rings to reflect new location, color, style, or other
   * changes.
   */
  updateFocusRings_() {
    let focusRings = [];
    this.rings_.forEach((ring) => focusRings.push(ring));
    chrome.accessibilityPrivate.setFocusRings(focusRings);
  }
}
