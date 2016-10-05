// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Implements support for live regions in ChromeVox Next.
 */

goog.provide('LiveRegions');

goog.require('ChromeVoxState');

goog.scope(function() {
var AutomationNode = chrome.automation.AutomationNode;
var RoleType = chrome.automation.RoleType;
var TreeChange = chrome.automation.TreeChange;

/**
 * ChromeVox2 live region handler.
 * @param {!ChromeVoxState} chromeVoxState The ChromeVox state object,
 *     keeping track of the current mode and current range.
 * @constructor
 */
LiveRegions = function(chromeVoxState) {
  /**
   * @type {!ChromeVoxState}
   * @private
   */
  this.chromeVoxState_ = chromeVoxState;

  /**
   * The time the last live region event was output.
   * @type {!Date}
   * @private
   */
  this.lastLiveRegionTime_ = new Date(0);

  /**
   * Set of nodes that have been announced as part of a live region since
   *     |this.lastLiveRegionTime_|, to prevent duplicate announcements.
   * @type {!WeakSet<AutomationNode>}
   * @private
   */
  this.liveRegionNodeSet_ = new WeakSet();
  chrome.automation.addTreeChangeObserver(
      'liveRegionTreeChanges', this.onTreeChange.bind(this));
};

/**
 * Live region events received in fewer than this many milliseconds will
 * queue, otherwise they'll be output with a category flush.
 * @type {number}
 * @const
 */
LiveRegions.LIVE_REGION_QUEUE_TIME_MS = 5000;

/**
 * Live region events received on the same node in fewer than this many
 * milliseconds will be dropped to avoid a stream of constant chatter.
 * @type {number}
 * @const
 */
LiveRegions.LIVE_REGION_MIN_SAME_NODE_MS = 20;

/**
 * Whether live regions from background tabs should be announced or not.
 * @type {boolean}
 * @private
 */
LiveRegions.announceLiveRegionsFromBackgroundTabs_ = true;

LiveRegions.prototype = {
  /**
   * Called when the automation tree is changed.
   * @param {TreeChange} treeChange
   */
  onTreeChange: function(treeChange) {
    var node = treeChange.target;
    if (!node.containerLiveStatus)
      return;

    var mode = this.chromeVoxState_.mode;
    var currentRange = this.chromeVoxState_.currentRange;

    if (mode === ChromeVoxMode.CLASSIC || !cvox.ChromeVox.isActive)
      return;

    if (!currentRange)
      return;

    var webView = AutomationUtil.getTopLevelRoot(node);
    webView = webView ? webView.parent : null;
    if (!LiveRegions.announceLiveRegionsFromBackgroundTabs_ &&
        currentRange.start.node.role != RoleType.desktop &&
        (!webView || !webView.state.focused)) {
      return;
    }

    var type = treeChange.type;
    var relevant = node.containerLiveRelevant;
    var additions = relevant.indexOf('additions') >= 0;
    var text = relevant.indexOf('text') >= 0;
    var removals = relevant.indexOf('removals') >= 0;
    var all = relevant.indexOf('all') >= 0;

    if (all || (additions &&
        (type == 'nodeCreated' || type == 'subtreeCreated'))) {
      this.outputLiveRegionChange_(node, null);
    }

    if (all || (text && type == 'textChanged'))
      this.outputLiveRegionChange_(node, null);

    if (all || (removals && type == 'nodeRemoved'))
      this.outputLiveRegionChange_(node, '@live_regions_removed');
  },

  /**
   * Given a node that needs to be spoken as part of a live region
   * change and an additional optional format string, output the
   * live region description.
   * @param {!AutomationNode} node The changed node.
   * @param {?string=} opt_prependFormatStr If set, a format string for
   *     cvox2.Output to prepend to the output.
   * @private
   */
  outputLiveRegionChange_: function(node, opt_prependFormatStr) {
    if (node.containerLiveBusy)
      return;

    if (node.containerLiveAtomic && !node.liveAtomic) {
      if (node.parent)
        this.outputLiveRegionChange_(node.parent, opt_prependFormatStr);
      return;
    }

    var range = cursors.Range.fromNode(node);
    var output = new Output();
    if (opt_prependFormatStr)
      output.format(opt_prependFormatStr);
    output.withSpeech(range, range, Output.EventType.NAVIGATE);

    if (!output.hasSpeech && node.liveAtomic)
      output.format('$joinedDescendants', node);

    output.withSpeechCategory(cvox.TtsCategory.LIVE);

    var currentTime = new Date();
    if (!output.hasSpeech)
      return;

    // Queue live regions coming from background tabs.
    var webView = AutomationUtil.getTopLevelRoot(node);
    webView = webView ? webView.parent : null;
    var forceQueueForBackgroundedLiveRegion =
        !webView || !webView.state.focused;

    // Enqueue live region updates that were received at approximately
    // the same time, otherwise flush previous live region updates.
    var queueTime = LiveRegions.LIVE_REGION_QUEUE_TIME_MS;
    var delta = currentTime - this.lastLiveRegionTime_;
    if (delta > queueTime && !forceQueueForBackgroundedLiveRegion)
      output.withQueueMode(cvox.QueueMode.CATEGORY_FLUSH);
    else
      output.withQueueMode(cvox.QueueMode.QUEUE);

    if (delta > LiveRegions.LIVE_REGION_MIN_SAME_NODE_MS)
      this.liveRegionNodeSet_ = new WeakSet();

    var parent = node;
    while (parent) {
      if (this.liveRegionNodeSet_.has(parent)) {
        this.lastLiveRegionTime_ = currentTime;
        return;
      }
      parent = parent.parent;
    }

    this.liveRegionNodeSet_.add(node);
    output.go();
    this.lastLiveRegionTime_ = currentTime;
  },
};

});  // goog.scope
