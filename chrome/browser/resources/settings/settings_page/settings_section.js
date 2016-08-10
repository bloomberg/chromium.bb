// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-section' shows a paper material themed section with a header
 * which shows its page title.
 *
 * The section can expand vertically to fill its container's padding edge.
 *
 * Example:
 *
 *    <settings-section page-title="[[pageTitle]]" section="privacy">
 *      <!-- Insert your section controls here -->
 *    </settings-section>
 */

// Fast out, slow in.
var EASING_FUNCTION = 'cubic-bezier(0.4, 0, 0.2, 1)';
var EXPAND_DURATION = 350;

var SettingsSectionElement = Polymer({
  is: 'settings-section',

  properties: {
    /**
     * The section name should match a name specified in route.js. The
     * MainPageBehavior will expand this section if this section name matches
     * currentRoute.section.
     */
    section: String,

    /** Title for the section header. */
    pageTitle: String,

    /**
     * A CSS attribute used for temporarily hiding a SETTINGS-SECTION for the
     * purposes of searching.
     */
    hiddenBySearch: {
      type: Boolean,
      value: false,
      reflectToAttribute: true,
    },
  },

  /**
   * Freezes the section's height so its card can be removed from the flow
   * without affecting the layout of the surrounding sections.
   * @param {boolean} frozen True to freeze, false to unfreeze.
   * @private
   */
  setFrozen: function(frozen) {
    var card = this.$.card;
    if (frozen) {
      this.style.height = this.clientHeight + 'px';

      var cardHeight = card.offsetHeight;
      var cardWidth = card.offsetWidth;
      // If the section is not displayed yet (e.g., navigated directly to a
      // sub-page), cardHeight and cardWidth are 0, so do not set the height or
      // width explicitly.
      // TODO(michaelpg): Improve this logic when refactoring
      // settings-animated-pages.
      if (cardHeight && cardWidth) {
        // TODO(michaelpg): Temporary hack to store the height the section
        // should collapse to when it closes.
        card.origHeight_ = cardHeight;

        card.style.height = cardHeight + 'px';
        card.style.width = cardWidth + 'px';
      } else {
        // Set an invalid value so we don't try to use it later.
        card.origHeight_ = NaN;
      }

      // Place the section's card at its current position but removed from the
      // flow.
      card.style.top = card.getBoundingClientRect().top + 'px';
      this.classList.add('frozen');
    } else {
      // Restore the section to its normal height.
      if (!this.classList.contains('frozen'))
        return;
      this.classList.remove('frozen');
      this.$.card.style.top = '';
      this.$.card.style.height = '';
      this.$.card.style.width = '';
      this.style.height = '';
    }
  },

  /**
   * @return {boolean} True if the section is currently rendered and not
   *     already expanded or transitioning.
   */
  canAnimateExpand: function() {
    return !this.classList.contains('expanded');
  },

  /**
   * Animates the section expanding to fill the container. The section is fixed
   * in the viewport during the animation. The section adds the "expanding"
   * class while the animation plays.
   *
   * @param {!HTMLElement} container The scrolling container to fill.
   * @return {?settings.animation.Animation} Animation played, if any.
   */
  animateExpand: function(container) {
    var card = this.$.card;

    // The card should start at the top of the page.
    var targetTop = container.getBoundingClientRect().top;

    this.classList.add('expanding');

    // Nothing to animate.
    if (!card.style.top || !card.style.height)
      return null;

    // Expand the card, using minHeight. (The card must span the container's
    // client height, so it must be at least 100% in case the card is too short.
    // If the card is already taller than the container's client height, we
    // don't want to shrink the card to 100% or the content will overflow, so
    // we can't use height, and animating height wouldn't look right anyway.)
    var keyframes = [{
      top: card.style.top,
      minHeight: card.style.height,
      easing: EASING_FUNCTION,
    }, {
      top: targetTop + 'px',
      minHeight: 'calc(100% - ' + targetTop + 'px)',
    }];
    var options = /** @type {!KeyframeEffectOptions} */({
      duration: EXPAND_DURATION
    });
    // TODO(michaelpg): Change elevation of sections.
    return new settings.animation.Animation(card, keyframes, options);
  },

  /**
   * Cleans up after animateExpand().
   * @param {boolean} finished Whether the animation finished successfully.
   */
  cleanUpAnimateExpand: function(finished) {
    if (finished) {
      this.classList.add('expanded');
      this.$.card.style.top = '';
      this.$.header.hidden = true;
      this.style.height = '';
    }

    this.classList.remove('expanding');
    this.$.card.style.height = '';
    this.$.card.style.width = '';
  },

  /** @return {boolean} True if the section is currently expanded. */
  canAnimateCollapse: function() {
    return this.classList.contains('expanded');
  },

  /**
   * Collapses an expanded section's card back into position in the main page.
   * Adds the "expanding" class during the animation.
   * @param {!HTMLElement} container The scrolling container the card fills.
   * @param {number} prevScrollTop scrollTop of the container before this
   *     section expanded.
   * @return {?settings.animation.Animation} Animation played, if any.
   */
  animateCollapse: function(container, prevScrollTop) {
    this.$.header.hidden = false;

    var startingTop = container.getBoundingClientRect().top;

    var card = this.$.card;
    var cardHeightStart = card.clientHeight;
    var cardWidthStart = card.clientWidth;

    this.classList.add('collapsing');
    this.classList.remove('expanding', 'expanded');

    // If we navigated here directly, we don't know the original height of the
    // section, so we skip the animation.
    // TODO(michaelpg): remove this condition once sliding is implemented.
    if (Number.isNaN(card.origHeight_))
      return null;

    // Restore the section to its proper height to make room for the card.
    this.style.height = (this.clientHeight + card.origHeight_) + 'px';

    // TODO(michaelpg): this should be in MainPageBehavior(), but we need to
    // wait until the full page height is available (setting the section
    // height).
    container.scrollTop = prevScrollTop;

    // The card is unpositioned, so use its position as the ending state,
    // but account for scroll.
    var targetTop = card.getBoundingClientRect().top - container.scrollTop;

    // Account for the section header.
    var headerStyle = getComputedStyle(this.$.header);
    targetTop += this.$.header.offsetHeight +
        parseInt(headerStyle.marginBottom, 10) +
        parseInt(headerStyle.marginTop, 10);

    var keyframes = [{
      top: startingTop + 'px',
      minHeight: cardHeightStart + 'px',
      easing: EASING_FUNCTION,
    }, {
      top: targetTop + 'px',
      minHeight: card.origHeight_ + 'px',
    }];
    var options = /** @type {!KeyframeEffectOptions} */({
      duration: EXPAND_DURATION
    });

    card.style.width = cardWidthStart + 'px';

    return new settings.animation.Animation(card, keyframes, options);
  },

  /**
   * Cleans up after animateCollapse().
   * @param {boolean} finished Whether the animation finished successfully.
   */
  cleanUpAnimateCollapse: function(finished) {
    if (finished)
      this.$.card.style.width = '';
  },
});
