// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The entry point for all ChromeVox2 related code for the
 * background page.
 */

goog.provide('Background');

goog.require('AutomationPredicate');
goog.require('AutomationUtil');
goog.require('BackgroundKeyboardHandler');
goog.require('BrailleCommandHandler');
goog.require('ChromeVoxState');
goog.require('CommandHandler');
goog.require('FindHandler');
goog.require('LiveRegions');
goog.require('MediaAutomationHandler');
goog.require('NextEarcons');
goog.require('Notifications');
goog.require('Output');
goog.require('Output.EventType');
goog.require('PanelCommand');
goog.require('Stubs');
goog.require('constants');
goog.require('cursors.Cursor');
goog.require('cvox.BrailleKeyCommand');
goog.require('cvox.ChromeVoxBackground');
goog.require('cvox.ChromeVoxEditableTextBase');
goog.require('cvox.ClassicEarcons');
goog.require('cvox.ExtensionBridge');
goog.require('cvox.NavBraille');

goog.scope(function() {
var AutomationNode = chrome.automation.AutomationNode;
var Dir = constants.Dir;
var EventType = chrome.automation.EventType;
var RoleType = chrome.automation.RoleType;
var StateType = chrome.automation.StateType;

/**
 * ChromeVox2 background page.
 * @constructor
 * @extends {ChromeVoxState}
 */
Background = function() {
  ChromeVoxState.call(this);

  /**
   * A list of site substring patterns to use with ChromeVox next. Keep these
   * strings relatively specific.
   * @type {!Array<string>}
   * @private
   */
  this.whitelist_ = ['chromevox_next_test'];

  /**
   * A list of site substring patterns to blacklist ChromeVox Classic,
   * putting ChromeVox into classic Compat mode.
   * @type {!Set<string>}
   * @private
   */
  this.classicBlacklist_ = new Set();

  /**
   * Regular expression for blacklisting classic.
   * @type {RegExp}
   * @private
   */
  this.classicBlacklistRegExp_ = Background.globsToRegExp_(
      chrome.runtime.getManifest()['content_scripts'][0]['exclude_globs']);

  /**
   * Regular expression for whitelisting Next compat.
   * @type {RegExp}
   * @private
   */
  this.nextCompatRegExp_ = Background.globsToRegExp_([
    '*docs.google.com/document/*',
    '*docs.google.com/spreadsheets/*',
    '*docs.google.com/presentation/*'
  ]);

  /**
   * @type {cursors.Range}
   * @private
   */
  this.currentRange_ = null;

  // Manually bind all functions to |this|.
  for (var func in this) {
    if (typeof(this[func]) == 'function')
      this[func] = this[func].bind(this);
  }

  /** @type {!cvox.AbstractEarcons} @private */
  this.classicEarcons_ = cvox.ChromeVox.earcons || new cvox.ClassicEarcons();

  /** @type {!cvox.AbstractEarcons} @private */
  this.nextEarcons_ = new NextEarcons();

  // Turn cvox.ChromeVox.earcons into a getter that returns either the
  // Next earcons or the Classic earcons depending on the current mode.
  Object.defineProperty(cvox.ChromeVox, 'earcons', {
    get: (function() {
      if (this.mode === ChromeVoxMode.FORCE_NEXT ||
          this.mode === ChromeVoxMode.NEXT ||
          this.mode === ChromeVoxMode.NEXT_COMPAT) {
        return this.nextEarcons_;
      } else {
        return this.classicEarcons_;
      }
    }).bind(this)
  });

  if (cvox.ChromeVox.isChromeOS) {
    Object.defineProperty(cvox.ChromeVox, 'modKeyStr', {
      get: function() {
        return (this.mode == ChromeVoxMode.CLASSIC ||
            this.mode == ChromeVoxMode.CLASSIC_COMPAT) ?
                'Search+Shift' : 'Search';
      }.bind(this)
    });

    Object.defineProperty(cvox.ChromeVox, 'typingEcho', {
      get: function() {
        return parseInt(localStorage['typingEcho'], 10);
      }.bind(this),
      set: function(v) {
        localStorage['typingEcho'] = v;
      }.bind(this)
    });
  }

  Object.defineProperty(cvox.ChromeVox, 'isActive', {
    get: function() {
      return localStorage['active'] !== 'false';
    },
    set: function(value) {
      localStorage['active'] = value;
    }
  });

  cvox.ExtensionBridge.addMessageListener(this.onMessage_);

  /** @type {!BackgroundKeyboardHandler} @private */
  this.keyboardHandler_ = new BackgroundKeyboardHandler();

  /** @type {!LiveRegions} @private */
  this.liveRegions_ = new LiveRegions(this);

  /**
   * Stores the mode as computed the last time a current range was set.
   * @type {?ChromeVoxMode}
   * @private
   */
  this.mode_ = null;

  chrome.accessibilityPrivate.onAccessibilityGesture.addListener(
      this.onAccessibilityGesture_);

  /**
   * Maps a non-desktop root automation node to a range position suitable for
   *     restoration.
   * @type {WeakMap<AutomationNode, cursors.Range>}
   * @private
   */
  this.focusRecoveryMap_ = new WeakMap();

  chrome.automation.getDesktop(function(desktop) {
    /** @type {string} */
    this.chromeChannel_ = desktop.chromeChannel;
  }.bind(this));

  // Record a metric with the mode we're in on startup.
  var useNext = localStorage['useClassic'] != 'true';
  chrome.metricsPrivate.recordValue(
      { metricName: 'Accessibility.CrosChromeVoxNext',
        type: chrome.metricsPrivate.MetricTypeType.HISTOGRAM_LINEAR,
        min: 1,  // According to histogram.h, this should be 1 for enums.
        max: 2,  // Maximum should be exclusive.
        buckets: 3 },  // Number of buckets: 0, 1 and overflowing 2.
      useNext ? 1 : 0);
};

/**
 * Map from gesture names (AXGesture defined in ui/accessibility/ax_enums.idl)
 *     to commands when in Classic mode.
 * @type {Object<string, string>}
 * @const
 */
Background.GESTURE_CLASSIC_COMMAND_MAP = {
  'click': 'forceClickOnCurrentItem',
  'swipeUp1': 'backward',
  'swipeDown1': 'forward',
  'swipeLeft1': 'left',
  'swipeRight1': 'right',
  'swipeUp2': 'jumpToTop',
  'swipeDown2': 'readFromHere',
};

/**
 * Map from gesture names (AXGesture defined in ui/accessibility/ax_enums.idl)
 *     to commands when in Classic mode.
 * @type {Object<string, string>}
 * @const
 */
Background.GESTURE_NEXT_COMMAND_MAP = {
  'click': 'forceClickOnCurrentItem',
  'swipeUp1': 'previousLine',
  'swipeDown1': 'nextLine',
  'swipeLeft1': 'previousObject',
  'swipeRight1': 'nextObject',
  'swipeUp2': 'jumpToTop',
  'swipeDown2': 'readFromHere',
};

Background.prototype = {
  __proto__: ChromeVoxState.prototype,

  /**
   * Maps the last node with range in a given root.
   * @type {WeakMap<AutomationNode>}
   */
  get focusRecoveryMap() {
    return this.focusRecoveryMap_;
  },

  /**
   * @override
   */
  getMode: function() {
    var useNext = localStorage['useClassic'] !== 'true';

    var target;
    if (!this.getCurrentRange()) {
      chrome.automation.getFocus(function(focus) {
        target = focus;
      });
    } else {
      target = this.getCurrentRange().start.node;
    }

    if (!target)
      return useNext ? ChromeVoxMode.FORCE_NEXT : ChromeVoxMode.CLASSIC;

    // Closure complains, but clearly, |target| is not null.
    var topLevelRoot =
        AutomationUtil.getTopLevelRoot(/** @type {!AutomationNode} */(target));
    if (!topLevelRoot)
      return useNext ? ChromeVoxMode.FORCE_NEXT :
          ChromeVoxMode.CLASSIC_COMPAT;

    var docUrl = topLevelRoot.docUrl || '';
    var nextSite = this.isWhitelistedForNext_(docUrl);
    var nextCompat = this.nextCompatRegExp_.test(docUrl) &&
        this.chromeChannel_ != 'dev';
    var classicCompat =
        this.isWhitelistedForClassicCompat_(docUrl);
    if (nextCompat && useNext)
      return ChromeVoxMode.NEXT_COMPAT;
    else if (classicCompat && !useNext)
      return ChromeVoxMode.CLASSIC_COMPAT;
    else if (nextSite)
      return ChromeVoxMode.NEXT;
    else if (!useNext)
      return ChromeVoxMode.CLASSIC;
    else
      return ChromeVoxMode.FORCE_NEXT;
  },

  /**
   * Handles a mode change.
   * @param {ChromeVoxMode} newMode
   * @param {?ChromeVoxMode} oldMode Can be null at startup when no range was
   *  previously set.
   * @private
   */
  onModeChanged_: function(newMode, oldMode) {
    this.keyboardHandler_.onModeChanged(newMode, oldMode);
    CommandHandler.onModeChanged(newMode, oldMode);
    FindHandler.onModeChanged(newMode, oldMode);
    Notifications.onModeChange(newMode, oldMode);

    // The below logic handles transition between the classic engine
    // (content script) and next engine (no content script) as well as
    // misc states that are not handled above.

    // Classic modes do not use the new focus highlight.
    if (newMode == ChromeVoxMode.CLASSIC ||
        newMode == ChromeVoxMode.NEXT_COMPAT)
      chrome.accessibilityPrivate.setFocusRing([]);

    // Switch on/off content scripts.
    // note that |this.currentRange_| can *change* because the request is
    // async. Save it to ensure we're looking at the currentRange at this moment
    // in time.
    var cur = this.currentRange_;
    chrome.tabs.query({active: true,
                       lastFocusedWindow: true}, function(tabs) {
      if (newMode == ChromeVoxMode.CLASSIC) {
        // Generally, we don't want to inject classic content scripts as it is
        // done by the extension system at document load. The exception is when
        // we toggle classic on manually as part of a user command.
        // Note that classic -> next_compat is ignored here because classic
        // should have already enabled content scripts.
        if (oldMode == ChromeVoxMode.FORCE_NEXT) {
          cvox.ChromeVox.injectChromeVoxIntoTabs(tabs);
        }
      } else if (newMode === ChromeVoxMode.FORCE_NEXT) {
        // Disable ChromeVox everywhere except for things whitelisted
        // for next compat.
        this.disableClassicChromeVox_({forNextCompat: true});
      } else if (newMode != ChromeVoxMode.NEXT_COMPAT) {
        // If we're focused in the desktop tree, do nothing.
        if (cur && !cur.isWebRange())
          return;

        // If we're entering classic compat mode or next mode for just one tab,
        // disable Classic for that tab only.
        this.disableClassicChromeVox_({tabs: tabs});
      }
    }.bind(this));

    // Switching into either compat or classic.
    if (oldMode === ChromeVoxMode.NEXT ||
        oldMode === ChromeVoxMode.FORCE_NEXT) {
      // Make sure we cancel the progress loading sound just in case.
      cvox.ChromeVox.earcons.cancelEarcon(cvox.Earcon.PAGE_START_LOADING);
      (new PanelCommand(PanelCommandType.DISABLE_MENUS)).send();
    }

    // Switching out of next, force next, or uninitialized (on startup).
    if (newMode === ChromeVoxMode.NEXT ||
        newMode === ChromeVoxMode.FORCE_NEXT) {
      (new PanelCommand(PanelCommandType.ENABLE_MENUS)).send();
      if (cvox.TabsApiHandler)
        cvox.TabsApiHandler.shouldOutputSpeechAndBraille = false;
    } else {
      // |newMode| is either classic or compat.
      if (cvox.TabsApiHandler)
        cvox.TabsApiHandler.shouldOutputSpeechAndBraille = true;
    }
  },

  /**
   * Toggles between force next and classic/compat modes.
   * This toggle automatically handles deciding between classic/compat based on
   * the start of the current range.
   * @param {boolean=} opt_setValue Directly set to force next (true) or
   *                                classic/compat (false).
   * @return {boolean} True to announce current position.
   */
  toggleNext: function(opt_setValue) {
    var useNext;
    if (opt_setValue !== undefined)
      useNext = opt_setValue;
    else
      useNext = localStorage['useClassic'] == 'true';

    if (useNext) {
      chrome.metricsPrivate.recordUserAction(
          'Accessibility.ChromeVox.ToggleNextOn');
    } else {
      chrome.metricsPrivate.recordUserAction(
          'Accessibility.ChromeVox.ToggleNextOff');
    }

    localStorage['useClassic'] = !useNext;
    if (useNext)
      this.setCurrentRangeToFocus_();
    else
      this.setCurrentRange(null);

    var announce = Msgs.getMsg(useNext ?
        'switch_to_next' : 'switch_to_classic');
    cvox.ChromeVox.tts.speak(
        announce, cvox.QueueMode.FLUSH, {doNotInterrupt: true});

    // If the new mode is Classic, return false now so we don't announce
    // anything more.
    return useNext;
  },

  /**
   * @override
   */
  getCurrentRange: function() {
    if (this.currentRange_ && this.currentRange_.isValid())
      return this.currentRange_;
    return null;
  },

  /**
   * @override
   */
  setCurrentRange: function(newRange) {
    // Clear anything that was frozen on the braille display whenever
    // the user navigates.
    cvox.ChromeVox.braille.thaw();

    if (newRange && !newRange.isValid())
      return;

    this.currentRange_ = newRange;
    var oldMode = this.mode_;
    var newMode = this.getMode();
    if (oldMode != newMode) {
      this.onModeChanged_(newMode, oldMode);
      this.mode_ = newMode;
    }

    if (this.currentRange_) {
      var start = this.currentRange_.start.node;
      start.makeVisible();

      var root = start.root;
      if (!root || root.role == RoleType.DESKTOP)
        return;

      var position = {};
      var loc = start.location;
      position.x = loc.left + loc.width / 2;
      position.y = loc.top + loc.height / 2;
      var url = root.docUrl;
      url = url.substring(0, url.indexOf('#')) || url;
      cvox.ChromeVox.position[url] = position;
    }
  },

  /**
   * Navigate to the given range - it both sets the range and outputs it.
   * @param {!cursors.Range} range The new range.
   * @param {boolean=} opt_focus Focus the range; defaults to true.
   * @param {Object=} opt_speechProps Speech properties.
   * @private
   */
  navigateToRange: function(range, opt_focus, opt_speechProps) {
    opt_focus = opt_focus === undefined ? true : opt_focus;
    opt_speechProps = opt_speechProps || {};
    var prevRange = this.currentRange_;
    if (opt_focus)
      this.setFocusToRange_(range, prevRange);

    this.setCurrentRange(range);

    var o = new Output();
    var selectedRange;
    if (this.pageSel_ &&
        this.pageSel_.isValid() &&
        range.isValid()) {
      // Compute the direction of the endpoints of each range.

      // Casts are ok because isValid checks node start and end nodes are
      // non-null; Closure just doesn't eval enough to see it.
      var startDir =
          AutomationUtil.getDirection(this.pageSel_.start.node,
              /** @type {!AutomationNode} */ (range.start.node));
      var endDir =
          AutomationUtil.getDirection(this.pageSel_.end.node,
              /** @type {!AutomationNode} */ (range.end.node));

      // Selection across roots isn't supported.
      var pageRootStart = this.pageSel_.start.node.root;
      var pageRootEnd = this.pageSel_.end.node.root;
      var curRootStart = range.start.node.root;
      var curRootEnd = range.end.node.root;

      // Disallow crossing over the start of the page selection and roots.
      if (startDir == Dir.BACKWARD ||
          pageRootStart != pageRootEnd ||
          pageRootStart != curRootStart ||
          pageRootEnd != curRootEnd) {
        o.format('@end_selection');
        this.pageSel_ = null;
      } else {
        // Expand or shrink requires different feedback.
        var msg;
        if (endDir == Dir.FORWARD &&
            (this.pageSel_.end.node != range.end.node ||
                this.pageSel_.end.index <= range.end.index)) {
          msg = '@selected';
        } else {
          msg = '@unselected';
          selectedRange = prevRange;
        }
        this.pageSel_ = new cursors.Range(
            this.pageSel_.start,
            range.end
            );
        if (this.pageSel_)
          this.pageSel_.select();
      }
    } else {
      // Ensure we don't select the editable when we first encounter it.
      var lca = null;
      if (range.start.node && prevRange.start.node) {
        lca = AutomationUtil.getLeastCommonAncestor(prevRange.start.node,
                                                    range.start.node);
      }
      if (!lca || lca.state[StateType.EDITABLE] ||
          !range.start.node.state[StateType.EDITABLE])
        range.select();
    }

    o.withRichSpeechAndBraille(
        selectedRange || range, prevRange, Output.EventType.NAVIGATE)
            .withQueueMode(cvox.QueueMode.FLUSH);

    if (msg)
      o.format(msg);

    for (var prop in opt_speechProps)
      o.format('!' + prop);

    o.go();
  },

  /**
   * Open the options page in a new tab.
   */
  showOptionsPage: function() {
    var optionsPage = {url: 'chromevox/background/options.html'};
    chrome.tabs.create(optionsPage);
  },

  /**
   * Handles a braille command.
   * @param {!cvox.BrailleKeyEvent} evt
   * @param {!cvox.NavBraille} content
   * @return {boolean} True if evt was processed.
   */
  onBrailleKeyEvent: function(evt, content) {
    if (this.mode === ChromeVoxMode.CLASSIC)
      return false;

    switch (evt.command) {
      case cvox.BrailleKeyCommand.PAN_LEFT:
        CommandHandler.onCommand('previousObject');
        break;
      case cvox.BrailleKeyCommand.PAN_RIGHT:
        CommandHandler.onCommand('nextObject');
        break;
      case cvox.BrailleKeyCommand.LINE_UP:
        CommandHandler.onCommand('previousLine');
        break;
      case cvox.BrailleKeyCommand.LINE_DOWN:
        CommandHandler.onCommand('nextLine');
        break;
      case cvox.BrailleKeyCommand.TOP:
        CommandHandler.onCommand('jumpToTop');
        break;
      case cvox.BrailleKeyCommand.BOTTOM:
        CommandHandler.onCommand('jumpToBottom');
        break;
      case cvox.BrailleKeyCommand.ROUTING:
        this.brailleRoutingCommand_(
            content.text,
            // Cast ok since displayPosition is always defined in this case.
            /** @type {number} */ (evt.displayPosition));
        break;
      case cvox.BrailleKeyCommand.CHORD:
        if (!evt.brailleDots)
          return false;

        var command = BrailleCommandHandler.getCommand(evt.brailleDots);
        if (command)
          CommandHandler.onCommand(command);
        break;
      default:
        return false;
    }
    return true;
  },

  /**
   * Returns true if the url should have Classic running.
   * @return {boolean}
   * @private
   */
  shouldEnableClassicForUrl_: function(url) {
    return (this.nextCompatRegExp_.test(url) &&this.chromeChannel_ != 'dev') ||
        (this.mode != ChromeVoxMode.FORCE_NEXT &&
         !this.isBlacklistedForClassic_(url) &&
         !this.isWhitelistedForNext_(url));
  },

  /**
   * Compat mode is on if any of the following are true:
   * 1. a url is blacklisted for Classic.
   * 2. the current range is not within web content.
   * @param {string} url
   * @return {boolean}
   */
  isWhitelistedForClassicCompat_: function(url) {
    return (this.isBlacklistedForClassic_(url) || (this.getCurrentRange() &&
        !this.getCurrentRange().isWebRange() &&
        this.getCurrentRange().start.node.state[StateType.FOCUSED])) || false;
  },

  /**
   * @param {string} url
   * @return {boolean}
   * @private
   */
  isBlacklistedForClassic_: function(url) {
    if (this.classicBlacklistRegExp_.test(url))
      return true;
    url = url.substring(0, url.indexOf('#')) || url;
    return this.classicBlacklist_.has(url);
  },

  /**
   * @param {string} url
   * @return {boolean} Whether the given |url| is whitelisted.
   * @private
   */
  isWhitelistedForNext_: function(url) {
    return this.whitelist_.some(function(item) {
      return url.indexOf(item) != -1;
    });
  },

  /**
   * Disables classic ChromeVox in current web content.
   * @param {{tabs: (Array<Tab>|undefined),
   *          forNextCompat: (boolean|undefined)}} params
   * tabs: The tabs where ChromeVox scripts should be disabled. If null, will
   *     disable ChromeVox everywhere.
   * forNextCompat: filters out tabs that have been listed for next compat (i.e.
   *     should retain content script).
   */
  disableClassicChromeVox_: function(params) {
    var disableChromeVoxCommand = {
      message: 'SYSTEM_COMMAND',
      command: 'killChromeVox'
    };

    if (params.forNextCompat) {
      var reStr = this.nextCompatRegExp_.toString();
      disableChromeVoxCommand['excludeUrlRegExp'] =
          reStr.substring(1, reStr.length - 1);
    }

    if (params.tabs) {
      for (var i = 0, tab; tab = params.tabs[i]; i++)
        chrome.tabs.sendMessage(tab.id, disableChromeVoxCommand);
    } else {
      // Send to all ChromeVox clients.
      cvox.ExtensionBridge.send(disableChromeVoxCommand);
    }
  },

  /**
   * @param {!Spannable} text
   * @param {number} position
   * @private
   */
  brailleRoutingCommand_: function(text, position) {
    var actionNodeSpan = null;
    var selectionSpan = null;
    text.getSpans(position).forEach(function(span) {
      if (span instanceof Output.SelectionSpan) {
        selectionSpan = span;
      } else if (span instanceof Output.NodeSpan) {
        if (!actionNodeSpan ||
            text.getSpanLength(span) <= text.getSpanLength(actionNodeSpan)) {
          actionNodeSpan = span;
        }
      }
    });
    if (!actionNodeSpan)
      return;
    var actionNode = actionNodeSpan.node;
    if (actionNode.role === RoleType.INLINE_TEXT_BOX)
      actionNode = actionNode.parent;
    actionNode.doDefault();
    if (selectionSpan) {
      var start = text.getSpanStart(selectionSpan);
      var targetPosition = position - start;
      actionNode.setSelection(targetPosition, targetPosition);
    }
  },

  /**
   * @param {Object} msg A message sent from a content script.
   * @param {Port} port
   * @private
   */
  onMessage_: function(msg, port) {
    var target = msg['target'];
    var action = msg['action'];

    switch (target) {
      case 'next':
        if (action == 'getIsClassicEnabled') {
          var url = msg['url'];
          var isClassicEnabled = this.shouldEnableClassicForUrl_(url);
          port.postMessage({
            target: 'next',
            isClassicEnabled: isClassicEnabled
          });
        } else if (action == 'enableClassicCompatForUrl') {
          var url = msg['url'];
          this.classicBlacklist_.add(url);
          if (this.currentRange_ && this.currentRange_.start.node)
            this.setCurrentRange(this.currentRange_);
        } else if (action == 'onCommand') {
          CommandHandler.onCommand(msg['command']);
        } else if (action == 'flushNextUtterance') {
          Output.forceModeForNextSpeechUtterance(cvox.QueueMode.FLUSH);
        }
        break;
    }
  },

  /**
   * Save the current ChromeVox range.
   */
  markCurrentRange: function() {
    if (!this.currentRange)
      return;

    var root = AutomationUtil.getTopLevelRoot(this.currentRange.start.node);
    if (root)
      this.focusRecoveryMap_.set(root, this.currentRange);
  },

  /**
   * Handles accessibility gestures from the touch screen.
   * @param {string} gesture The gesture to handle, based on the AXGesture enum
   *     defined in ui/accessibility/ax_enums.idl
   * @private
   */
  onAccessibilityGesture_: function(gesture) {
    // If we're in classic mode, some gestures need to be handled by the
    // content script. Other gestures are universal and will be handled in
    // this function.
    if (this.mode == ChromeVoxMode.CLASSIC) {
      if (this.handleClassicGesture_(gesture))
        return;
    }

    var command = Background.GESTURE_NEXT_COMMAND_MAP[gesture];
    if (command)
      CommandHandler.onCommand(command);
  },

  /**
   * Handles accessibility gestures from the touch screen when in CLASSIC
   * mode, by forwarding a command to the content script.
   * @param {string} gesture The gesture to handle, based on the AXGesture enum
   *     defined in ui/accessibility/ax_enums.idl
   * @return {boolean} True if this gesture was handled.
   * @private
   */
  handleClassicGesture_: function(gesture) {
    var command = Background.GESTURE_CLASSIC_COMMAND_MAP[gesture];
    if (!command)
      return false;

    var msg = {
      'message': 'USER_COMMAND',
      'command': command
    };
    cvox.ExtensionBridge.send(msg);
    return true;
  },

  /** @private */
  setCurrentRangeToFocus_: function() {
    chrome.automation.getFocus(function(focus) {
      if (focus)
        this.setCurrentRange(cursors.Range.fromNode(focus));
      else
        this.setCurrentRange(null);
    }.bind(this));
  },

  /**
   * @param {!cursors.Range} range
   * @param {cursors.Range} prevRange
   * @private
   */
  setFocusToRange_: function(range, prevRange) {
    var start = range.start.node;
    var end = range.end.node;

    // First, see if we've crossed a root. Remove once webview handles focus
    // correctly.
    if (prevRange && prevRange.start.node && start) {
      var entered = AutomationUtil.getUniqueAncestors(
          prevRange.start.node, start);
      var embeddedObject = entered.find(function(f) {
        return f.role == RoleType.EMBEDDED_OBJECT; });
      if (embeddedObject && !embeddedObject.state[StateType.FOCUSED])
        embeddedObject.focus();
    }

    if (start.state[StateType.FOCUSED] || end.state[StateType.FOCUSED])
      return;

    var isFocusableLinkOrControl = function(node) {
      return node.state[StateType.FOCUSABLE] &&
          AutomationPredicate.linkOrControl(node);
    };

    // Next, try to focus the start or end node.
    if (!AutomationPredicate.structuralContainer(start) &&
        start.state[StateType.FOCUSABLE]) {
      if (!start.state[StateType.FOCUSED])
        start.focus();
      return;
    } else if (!AutomationPredicate.structuralContainer(end) &&
        end.state[StateType.FOCUSABLE]) {
      if (!end.state[StateType.FOCUSED])
        end.focus();
      return;
    }

    // If a common ancestor of |start| and |end| is a link, focus that.
    var ancestor = AutomationUtil.getLeastCommonAncestor(start, end);
    while (ancestor && ancestor.root == start.root) {
      if (isFocusableLinkOrControl(ancestor)) {
        if (!ancestor.state[StateType.FOCUSED])
          ancestor.focus();
        return;
      }
      ancestor = ancestor.parent;
    }

    // If nothing is focusable, set the sequential focus navigation starting
    // point, which ensures that the next time you press Tab, you'll reach
    // the next or previous focusable node from |start|.
    if (!start.state[StateType.OFFSCREEN])
      start.setSequentialFocusNavigationStartingPoint();
  }
};

/**
 * Converts a list of globs, as used in the extension manifest, to a regular
 * expression that matches if and only if any of the globs in the list matches.
 * @param {!Array<string>} globs
 * @return {!RegExp}
 * @private
 */
Background.globsToRegExp_ = function(globs) {
  return new RegExp('^(' + globs.map(function(glob) {
    return glob.replace(/[.+^$(){}|[\]\\]/g, '\\$&')
        .replace(/\*/g, '.*')
        .replace(/\?/g, '.');
  }).join('|') + ')$');
};

new Background();

});  // goog.scope
