// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'cr-settings-section' shows a paper material themed section with a header
 * which shows its page title and icon.
 *
 * Example:
 *
 *    <cr-settings-section page-title="[[pageTitle]]" icon="[[icon]]">
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
     * Title for the page header and navigation menu.
     */
    pageTitle: String,

    /**
     * Name of the 'iron-icon' to show.
     */
    icon: String,

    /**
     * True if the section should be expanded to take up the full height of
     * the page (except the toolbar). The title and icon of the section will be
     * hidden, and the section contents is expected to provide its own subtitle.
     */
    expanded: {
      type: Boolean,
      observer: 'expandedChanged',
    },

    /**
     * Container that determines the sizing of expanded sections.
     */
    expandContainer: {
      type: Object,
      notify: true,
    },

    animationConfig: {
      value: function() {
        return {
          expand: {
            name: 'expand-card-animation',
            node: this,
          },
          collapse: {
            name: 'collapse-card-animation',
            node: this,
          }
        };
      },
    },
  },

  expandedChanged: function() {
    if (this.expanded) {
      this.playAnimation('expand');
    } else {
      this.playAnimation('collapse');
    }
  },
});

Polymer({
  is: 'expand-card-animation',

  behaviors: [
    Polymer.NeonAnimationBehavior
  ],

  configure: function(config) {
    var node = config.node;
    var containerRect = node.expandContainer.getBoundingClientRect();
    var nodeRect = node.getBoundingClientRect();

    // Save section's original height.
    node.unexpandedHeight = nodeRect.height;

    var headerHeight = node.$.header.getBoundingClientRect().height;
    var newTop = containerRect.top - headerHeight;
    var newHeight = containerRect.height + headerHeight;

    node.style.position = 'fixed';

    this._effect = new KeyframeEffect(node, [
      {'top': nodeRect.top, 'height': nodeRect.height},
      {'top': newTop, 'height': newHeight},
    ], this.timingFromConfig(config));
    return this._effect;
  },

  complete: function(config) {
    config.node.style.position = 'absolute';
    config.node.style.top =
        -config.node.$.header.getBoundingClientRect().height + 'px';
    config.node.style.bottom = 0;
  }
});

Polymer({
  is: 'collapse-card-animation',

  behaviors: [
    Polymer.NeonAnimationBehavior
  ],

  configure: function(config) {
    var node = config.node;

    var oldRect = node.getBoundingClientRect();

    // Temporarily set position to static to determine new height.
    node.style.position = '';
    var newTop = node.getBoundingClientRect().top;
    var newHeight = node.unexpandedHeight;

    node.style.position = 'fixed';

    this._effect = new KeyframeEffect(node, [
      {'top': oldRect.top, 'height': oldRect.height},
      {'top': newTop, 'height': newHeight},
    ], this.timingFromConfig(config));
    return this._effect;
  },

  complete: function(config) {
    config.node.style.position = '';
  }
});
