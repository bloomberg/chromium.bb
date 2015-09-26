// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'cr-settings-section' shows a paper material themed section with a header
 * which shows its page title.
 *
 * Example:
 *
 *    <cr-settings-section page-title="[[pageTitle]]">
 *      <!-- Insert your section controls here -->
 *    </cr-settings-section>
 *
 * @group Chrome Settings Elements
 * @element cr-settings-section
 */
Polymer({
  is: 'cr-settings-section',

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
     */
    section: {
      type: String,
    },

    /**
     * Title for the page header and navigation menu.
     */
    pageTitle: String,

    /**
     * Container that determines the sizing of expanded sections.
     */
    expandContainer: {
      type: Object,
    },

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
  expanded_: false,

  /** @private */
  currentRouteChanged_: function() {
    var expanded = this.currentRoute.section == this.section;

    if (expanded != this.expanded_) {
      this.expanded_ = expanded;
      this.playAnimation(expanded ? 'expand' : 'collapse');
    }

    var visible = expanded || this.currentRoute.section == '';
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
  }
});

Polymer({
  is: 'expand-card-animation',

  behaviors: [
    Polymer.NeonAnimationBehavior
  ],

  configure: function(config) {
    var section = config.node;
    var card = section.$.card;
    var containerRect = section.expandContainer.getBoundingClientRect();
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
    section.expandContainer.classList.add('expanded');

    // This event fires on itself as well, but that is benign.
    var sections = section.parentNode.querySelectorAll('cr-settings-section');
    for (var i = 0; i < sections.length; ++i) {
      sections[i].fire('expand-animation-complete');
    }

    section.expandContainer.scrollTop = 0;
  }
});

Polymer({
  is: 'collapse-card-animation',

  behaviors: [
    Polymer.NeonAnimationBehavior
  ],

  configure: function(config) {
    var section = config.node;
    var oldRect = section.expandContainer.getBoundingClientRect();

    section.classList.remove('expanded');
    section.expandContainer.classList.remove('expanded');

    // Get the placeholder coordinates before reflowing.
    var newRect = section.$.placeholder.getBoundingClientRect();

    section.classList.add('neon-animating');

    this._effect = new KeyframeEffect(section.$.card, [
      {'top': oldRect.top + 'px', 'height': oldRect.height + 'px'},
      {'top': newRect.top + 'px', 'height': newRect.height + 'px'},
    ], this.timingFromConfig(config));
    return this._effect;
  },

  complete: function(config) {
    config.node.classList.remove('neon-animating');
  }
});
