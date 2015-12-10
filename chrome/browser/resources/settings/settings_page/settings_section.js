// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-section' shows a paper material themed section with a header
 * which shows its page title.
 *
 * Example:
 *
 *    <settings-section page-title="[[pageTitle]]" section="privacy">
 *      <!-- Insert your section controls here -->
 *    </settings-section>
 *
 * @group Chrome Settings Elements
 * @element settings-section
 */
Polymer({
  is: 'settings-section',

  behaviors: [
    Polymer.NeonAnimationRunnerBehavior,
  ],

  properties: {
    /**
     * The current active route.
     */
    currentRoute: {
      type: Object,
      observer: 'currentRouteChanged_',
    },

    /**
     * The section is expanded to a full-page view when this property matches
     * currentRoute.section.
     *
     * The section name must match the name specified in settings_router.js.
     */
    section: {
      type: String,
    },

    /**
     * Title for the page header and navigation menu.
     */
    pageTitle: String,

    animationConfig: {
      value: function() {
        return {
          collapse: {
            name: 'collapse-card-animation',
            node: this,
          },
          expand: {
            name: 'expand-card-animation',
            node: this,
          },
        };
      },
    },
  },

  listeners: {
    'expand-animation-complete': 'onExpandAnimationComplete_',
  },

  /** @private */
  currentRouteChanged_: function(newRoute, oldRoute) {
    var newExpanded = newRoute.section == this.section;
    var oldExpanded = oldRoute && oldRoute.section == this.section;

    var visible = newExpanded || this.currentRoute.section == '';

    // If the user navigates directly to a subpage, skip all the animations.
    if (!oldRoute) {
      if (newExpanded) {
        // If we navigate directly to a subpage, skip animations.
        this.classList.add('expanded');
      } else if (!visible) {
        this.hidden = true;
        this.$.card.elevation = 0;
      }

      return;
    }

    if (newExpanded && !oldExpanded) {
      this.playAnimation('expand');
    } else if (oldExpanded && !newExpanded) {
      // For contraction, we defer the animation to allow
      // settings-animated-pages to reflow the new page correctly.
      this.async(function() {
        this.playAnimation('collapse');
      }.bind(this));
    }

    this.$.card.elevation = visible ? 1 : 0;

    // Remove 'hidden' class immediately, but defer adding it if we are invisble
    // until the animation is complete.
    if (visible)
      this.hidden = false;
  },

  /** @private */
  onExpandAnimationComplete_: function() {
    this.hidden = this.currentRoute.section != '' &&
                  this.currentRoute.section != this.section;
  },
});

Polymer({
  is: 'expand-card-animation',

  behaviors: [
    Polymer.NeonAnimationBehavior
  ],

  configure: function(config) {
    var section = config.node;
    var card = section.$.card;
    var containerRect = section.offsetParent.getBoundingClientRect();
    var cardRect = card.getBoundingClientRect();

    // Set placeholder height so the page does not reflow during animation.
    // TODO(tommycli): For URLs that directly load subpages, this does not work.
    var placeholder = section.$.placeholder;
    placeholder.style.top = card.offsetTop + 'px';
    placeholder.style.height = card.offsetHeight + 'px';

    section.classList.add('neon-animating');

    this._effect = new KeyframeEffect(card, [
      {'top': cardRect.top + 'px', 'height': cardRect.height + 'px'},
      {'top': containerRect.top + 'px', 'height': containerRect.height + 'px'},
    ], this.timingFromConfig(config));
    return this._effect;
  },

  complete: function(config) {
    var section = config.node;
    section.classList.remove('neon-animating');
    section.classList.add('expanded');

    // This event fires on itself as well, but that is benign.
    var sections = section.parentNode.querySelectorAll('settings-section');
    for (var i = 0; i < sections.length; ++i) {
      sections[i].fire('expand-animation-complete');
    }
  }
});

Polymer({
  is: 'collapse-card-animation',

  behaviors: [
    Polymer.NeonAnimationBehavior
  ],

  configure: function(config) {
    var section = config.node;
    var oldRect = section.offsetParent.getBoundingClientRect();

    section.classList.remove('expanded');

    var card = section.$.card;
    var placeholder = section.$.placeholder;
    placeholder.style.top = card.offsetTop + 'px';
    placeholder.style.height = card.offsetHeight + 'px';

    var newRect = card.getBoundingClientRect();

    section.classList.add('neon-animating');

    this._effect = new KeyframeEffect(card, [
      {'top': oldRect.top + 'px', 'height': oldRect.height + 'px'},
      {'top': newRect.top + 'px', 'height': newRect.height + 'px'},
    ], this.timingFromConfig(config));
    return this._effect;
  },

  complete: function(config) {
    config.node.classList.remove('neon-animating');
  }
});
