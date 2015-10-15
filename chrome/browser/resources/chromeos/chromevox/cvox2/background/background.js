// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The entry point for all ChromeVox2 related code for the
 * background page.
 */

goog.provide('Background');
goog.provide('global');

goog.require('AutomationPredicate');
goog.require('AutomationUtil');
goog.require('ClassicCompatibility');
goog.require('NextEarcons');
goog.require('Output');
goog.require('Output.EventType');
goog.require('cursors.Cursor');
goog.require('cvox.BrailleKeyCommand');
goog.require('cvox.ChromeVoxEditableTextBase');
goog.require('cvox.ClassicEarcons');
goog.require('cvox.ExtensionBridge');
goog.require('cvox.NavBraille');

goog.scope(function() {
var AutomationNode = chrome.automation.AutomationNode;
var Dir = AutomationUtil.Dir;
var EventType = chrome.automation.EventType;
var RoleType = chrome.automation.RoleType;

/**
 * All possible modes ChromeVox can run.
 * @enum {string}
 */
var ChromeVoxMode = {
  CLASSIC: 'classic',
  COMPAT: 'compat',
  NEXT: 'next',
  FORCE_NEXT: 'force_next'
};

/**
 * ChromeVox2 background page.
 * @constructor
 */
Background = function() {
  /**
   * A list of site substring patterns to use with ChromeVox next. Keep these
   * strings relatively specific.
   * @type {!Array<string>}
   * @private
   */
  this.whitelist_ = ['chromevox_next_test'];

  /**
   * @type {cursors.Range}
   * @private
   */
  this.currentRange_ = null;

  /**
   * Which variant of ChromeVox is active.
   * @type {ChromeVoxMode}
   * @private
   */
  this.mode_ = ChromeVoxMode.COMPAT;

  /** @type {!ClassicCompatibility} @private */
  this.compat_ = new ClassicCompatibility();

  // Manually bind all functions to |this|.
  for (var func in this) {
    if (typeof(this[func]) == 'function')
      this[func] = this[func].bind(this);
  }

  /**
   * Maps an automation event to its listener.
   * @type {!Object<EventType, function(Object) : void>}
   */
  this.listeners_ = {
    alert: this.onAlert,
    focus: this.onFocus,
    hover: this.onEventDefault,
    loadComplete: this.onLoadComplete,
    menuStart: this.onEventDefault,
    menuEnd: this.onEventDefault,
    textChanged: this.onTextOrTextSelectionChanged,
    textSelectionChanged: this.onTextOrTextSelectionChanged,
    valueChanged: this.onValueChanged
  };

  /**
   * The object that speaks changes to an editable text field.
   * @type {?cvox.ChromeVoxEditableTextBase}
   */
  this.editableTextHandler_ = null;

  chrome.automation.getDesktop(this.onGotDesktop);

  // Handle messages directed to the Next background page.
  cvox.ExtensionBridge.addMessageListener(function(msg, port) {
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
        }
        break;
    }
  }.bind(this));

  /** @type {!cvox.AbstractEarcons} @private */
  this.classicEarcons_ = cvox.ChromeVox.earcons || new cvox.ClassicEarcons();

  /** @type {!cvox.AbstractEarcons} @private */
  this.nextEarcons_ = new NextEarcons();

  // Turn cvox.ChromeVox.earcons into a getter that returns either the
  // Next earcons or the Classic earcons depending on the current mode.
  Object.defineProperty(cvox.ChromeVox, 'earcons', {
    get: (function() {
      if (this.mode_ === ChromeVoxMode.FORCE_NEXT ||
          this.mode_ === ChromeVoxMode.NEXT) {
        return this.nextEarcons_;
      } else {
        return this.classicEarcons_;
      }
    }).bind(this)
  });
};

Background.prototype = {
  /** Forces ChromeVox Next to be active for all tabs. */
  forceChromeVoxNextActive: function() {
    this.setChromeVoxMode(ChromeVoxMode.FORCE_NEXT);
  },

  /**
   * Handles all setup once a new automation tree appears.
   * @param {chrome.automation.AutomationNode} desktop
   */
  onGotDesktop: function(desktop) {
    // Register all automation event listeners.
    for (var eventType in this.listeners_)
      desktop.addEventListener(eventType, this.listeners_[eventType], true);

    // Register a tree change observer.
    chrome.automation.addTreeChangeObserver(this.onTreeChange);

    // The focused state gets set on the containing webView node.
    var webView = desktop.find({role: RoleType.webView,
                                state: {focused: true}});
    if (webView) {
      var root = webView.find({role: RoleType.rootWebArea});
      if (root) {
        this.onLoadComplete(
            {target: root,
             type: chrome.automation.EventType.loadComplete});
      }
    }
  },

  /**
   * Handles chrome.commands.onCommand.
   * @param {string} command
   * @param {boolean=} opt_skipCompat Whether to skip compatibility checks.
   */
  onGotCommand: function(command, opt_skipCompat) {
    if (!this.currentRange_)
      return;

    if (!opt_skipCompat && this.mode_ === ChromeVoxMode.COMPAT) {
      if (this.compat_.onGotCommand(command))
        return;
    }

    var current = this.currentRange_;
    var dir = Dir.FORWARD;
    var pred = null;
    var predErrorMsg = undefined;
    switch (command) {
      case 'nextCharacter':
        current = current.move(cursors.Unit.CHARACTER, Dir.FORWARD);
        break;
      case 'previousCharacter':
        current = current.move(cursors.Unit.CHARACTER, Dir.BACKWARD);
        break;
      case 'nextWord':
        current = current.move(cursors.Unit.WORD, Dir.FORWARD);
        break;
      case 'previousWord':
        current = current.move(cursors.Unit.WORD, Dir.BACKWARD);
        break;
      case 'nextLine':
        current = current.move(cursors.Unit.LINE, Dir.FORWARD);
        break;
      case 'previousLine':
        current = current.move(cursors.Unit.LINE, Dir.BACKWARD);
        break;
      case 'nextButton':
        dir = Dir.FORWARD;
        pred = AutomationPredicate.button;
        predErrorMsg = 'no_next_button';
        break;
      case 'previousButton':
        dir = Dir.BACKWARD;
        pred = AutomationPredicate.button;
        predErrorMsg = 'no_previous_button';
        break;
      case 'nextCheckBox':
        dir = Dir.FORWARD;
        pred = AutomationPredicate.checkBox;
        predErrorMsg = 'no_next_checkbox';
        break;
      case 'previousCheckBox':
        dir = Dir.BACKWARD;
        pred = AutomationPredicate.checkBox;
        predErrorMsg = 'no_previous_checkbox';
        break;
      case 'nextComboBox':
        dir = Dir.FORWARD;
        pred = AutomationPredicate.comboBox;
        predErrorMsg = 'no_next_combo_box';
        break;
      case 'previousComboBox':
        dir = Dir.BACKWARD;
        pred = AutomationPredicate.comboBox;
        predErrorMsg = 'no_previous_combo_box';
        break;
      case 'nextEditText':
        dir = Dir.FORWARD;
        pred = AutomationPredicate.editText;
        predErrorMsg = 'no_next_edit_text';
        break;
      case 'previousEditText':
        dir = Dir.BACKWARD;
        pred = AutomationPredicate.editText;
        predErrorMsg = 'no_previous_edit_text';
        break;
      case 'nextFormField':
        dir = Dir.FORWARD;
        pred = AutomationPredicate.formField;
        predErrorMsg = 'no_next_form_field';
        break;
      case 'previousFormField':
        dir = Dir.BACKWARD;
        pred = AutomationPredicate.formField;
        predErrorMsg = 'no_previous_form_field';
        break;
      case 'nextHeading':
        dir = Dir.FORWARD;
        pred = AutomationPredicate.heading;
        predErrorMsg = 'no_next_heading';
        break;
      case 'previousHeading':
        dir = Dir.BACKWARD;
        pred = AutomationPredicate.heading;
        predErrorMsg = 'no_previous_heading';
        break;
      case 'nextLink':
        dir = Dir.FORWARD;
        pred = AutomationPredicate.link;
        predErrorMsg = 'no_next_link';
        break;
      case 'previousLink':
        dir = Dir.BACKWARD;
        pred = AutomationPredicate.link;
        predErrorMsg = 'no_previous_link';
        break;
      case 'nextTable':
        dir = Dir.FORWARD;
        pred = AutomationPredicate.table;
        predErrorMsg = 'no_next_table';
        break;
      case 'previousTable':
        dir = Dir.BACKWARD;
        pred = AutomationPredicate.table;
        predErrorMsg = 'no_previous_table';
        break;
      case 'nextVisitedLink':
        dir = Dir.FORWARD;
        pred = AutomationPredicate.visitedLink;
        predErrorMsg = 'no_next_visited_link';
        break;
      case 'previousVisitedLink':
        dir = Dir.BACKWARD;
        pred = AutomationPredicate.visitedLink;
        predErrorMsg = 'no_previous_visited_link';
        break;
      case 'nextElement':
        current = current.move(cursors.Unit.DOM_NODE, Dir.FORWARD);
        break;
      case 'previousElement':
        current = current.move(cursors.Unit.DOM_NODE, Dir.BACKWARD);
        break;
      case 'goToBeginning':
        var node =
            AutomationUtil.findNodePost(current.start.node.root,
                                        Dir.FORWARD,
                                        AutomationPredicate.leaf);
        if (node)
          current = cursors.Range.fromNode(node);
        break;
      case 'goToEnd':
        var node =
            AutomationUtil.findNodePost(current.start.node.root,
                                        Dir.BACKWARD,
                                        AutomationPredicate.leaf);
        if (node)
          current = cursors.Range.fromNode(node);
        break;
      case 'doDefault':
        if (this.currentRange_) {
          var actionNode = this.currentRange_.start.node;
          if (actionNode.role == RoleType.inlineTextBox)
            actionNode = actionNode.parent;
          actionNode.doDefault();
        }
        // Skip all other processing; if focus changes, we should get an event
        // for that.
        return;
      case 'continuousRead':
        global.isReadingContinuously = true;
        var continueReading = function(prevRange) {
          if (!global.isReadingContinuously || !this.currentRange_)
            return;

          new Output().withSpeechAndBraille(
                  this.currentRange_, prevRange, Output.EventType.NAVIGATE)
              .onSpeechEnd(function() { continueReading(prevRange); })
              .go();
          prevRange = this.currentRange_;
          this.currentRange_ =
              this.currentRange_.move(cursors.Unit.NODE, Dir.FORWARD);

          if (!this.currentRange_ || this.currentRange_.equals(prevRange))
            global.isReadingContinuously = false;
        }.bind(this);

        continueReading(null);
        return;
      case 'showContextMenu':
        if (this.currentRange_) {
          var actionNode = this.currentRange_.start.node;
          if (actionNode.role == RoleType.inlineTextBox)
            actionNode = actionNode.parent;
          actionNode.showContextMenu();
          return;
        }
        break;
      case 'showOptionsPage':
        var optionsPage = {url: 'chromevox/background/options.html'};
        chrome.tabs.create(optionsPage);
        break;
    }

    if (pred) {
      var node = AutomationUtil.findNextNode(
          current.getBound(dir).node, dir, pred);

      if (node) {
        current = cursors.Range.fromNode(node);
      } else {
        if (predErrorMsg) {
          cvox.ChromeVox.tts.speak(Msgs.getMsg(predErrorMsg),
                                   cvox.QueueMode.FLUSH);
        }
        return;
      }
    }

    if (current) {
      // TODO(dtseng): Figure out what it means to focus a range.
      var actionNode = current.start.node;
      if (actionNode.role == RoleType.inlineTextBox)
        actionNode = actionNode.parent;
      actionNode.focus();

      var prevRange = this.currentRange_;
      this.currentRange_ = current;

      new Output().withSpeechAndBraille(
              this.currentRange_, prevRange, Output.EventType.NAVIGATE)
          .go();
    }
  },

  /**
   * Handles a braille command.
   * @param {!cvox.BrailleKeyEvent} evt
   * @param {!cvox.NavBraille} content
   * @return {boolean} True if evt was processed.
   */
  onBrailleKeyEvent: function(evt, content) {
    if (this.mode_ === ChromeVoxMode.CLASSIC)
      return false;

    switch (evt.command) {
      case cvox.BrailleKeyCommand.PAN_LEFT:
        this.onGotCommand('previousElement', true);
        break;
      case cvox.BrailleKeyCommand.PAN_RIGHT:
        this.onGotCommand('nextElement', true);
        break;
      case cvox.BrailleKeyCommand.LINE_UP:
        this.onGotCommand('previousLine', true);
        break;
      case cvox.BrailleKeyCommand.LINE_DOWN:
        this.onGotCommand('nextLine', true);
        break;
      case cvox.BrailleKeyCommand.TOP:
        this.onGotCommand('goToBeginning', true);
        break;
      case cvox.BrailleKeyCommand.BOTTOM:
        this.onGotCommand('goToEnd', true);
        break;
      case cvox.BrailleKeyCommand.ROUTING:
        this.brailleRoutingCommand_(
            content.text,
            // Cast ok since displayPosition is always defined in this case.
            /** @type {number} */ (evt.displayPosition));
        break;
      default:
        return false;
    }
    return true;
  },

  /**
   * Provides all feedback once ChromeVox's focus changes.
   * @param {Object} evt
   */
  onEventDefault: function(evt) {
    var node = evt.target;

    if (!node)
      return;

    var prevRange = this.currentRange_;

    this.currentRange_ = cursors.Range.fromNode(node);

    // Check to see if we've crossed roots. Continue if we've crossed roots or
    // are not within web content.
    if (node.root.role == 'desktop' ||
        !prevRange ||
        prevRange.start.node.root != node.root)
      this.setupChromeVoxVariants_(node.root.docUrl || '');

    // Don't process nodes inside of web content if ChromeVox Next is inactive.
    if (node.root.role != RoleType.desktop &&
        this.mode_ === ChromeVoxMode.CLASSIC) {
      chrome.accessibilityPrivate.setFocusRing([]);
      return;
    }

    // Don't output if focused node hasn't changed.
    if (prevRange &&
        evt.type == 'focus' &&
        this.currentRange_.equals(prevRange))
      return;

    new Output().withSpeechAndBraille(
            this.currentRange_, prevRange, evt.type)
        .go();
  },

  /**
   * Makes an announcement without changing focus.
   * @param {Object} evt
   */
  onAlert: function(evt) {
    var node = evt.target;
    if (!node)
      return;

    // Don't process nodes inside of web content if ChromeVox Next is inactive.
    if (node.root.role != RoleType.desktop &&
        this.mode_ === ChromeVoxMode.CLASSIC) {
      return;
    }

    var range = cursors.Range.fromNode(node);

    new Output().withSpeechAndBraille(range, null, evt.type).go();
  },

  /**
   * Provides all feedback once a focus event fires.
   * @param {Object} evt
   */
  onFocus: function(evt) {
    // Invalidate any previous editable text handler state.
    this.editableTextHandler_ = null;

    var node = evt.target;


    // Discard focus events on embeddedObject nodes.
    if (node.role == RoleType.embeddedObject)
      return;

    // It almost never makes sense to place focus directly on a rootWebArea.
    if (node.role == RoleType.rootWebArea) {
      // Discard focus events for root web areas when focus was previously
      // placed on a descendant.
      if (this.currentRange_.start.node.root == node)
        return;

      // Discard focused root nodes without focused state set.
      if (!node.state.focused)
        return;

      // Try to find a focusable descendant.
      node = node.find({state: {focused: true}}) || node;
    }

    if (evt.target.state.editable)
      this.createEditableTextHandlerIfNeeded_(evt.target);

    this.onEventDefault({target: node, type: 'focus'});
  },

  /**
   * Provides all feedback once a load complete event fires.
   * @param {Object} evt
   */
  onLoadComplete: function(evt) {
    this.setupChromeVoxVariants_(evt.target.docUrl);

    // Don't process nodes inside of web content if ChromeVox Next is inactive.
    if (evt.target.root.role != RoleType.desktop &&
        this.mode_ === ChromeVoxMode.CLASSIC)
      return;

    // If initial focus was already placed on this page (e.g. if a user starts
    // tabbing before load complete), then don't move ChromeVox's position on
    // the page.
    if (this.currentRange_ &&
        this.currentRange_.start.node.role != RoleType.rootWebArea &&
        this.currentRange_.start.node.root.docUrl == evt.target.docUrl)
      return;

    var root = evt.target;
    var webView = root;
    while (webView && webView.role != RoleType.webView)
      webView = webView.parent;

    if (!webView || !webView.state.focused)
      return;

    var node = AutomationUtil.findNodePost(root,
        Dir.FORWARD,
        AutomationPredicate.leaf);

    if (node)
      this.currentRange_ = cursors.Range.fromNode(node);

    if (this.currentRange_)
      new Output().withSpeechAndBraille(
              this.currentRange_, null, evt.type)
          .go();
  },

  /**
   * Provides all feedback once a text selection change event fires.
   * @param {Object} evt
   */
  onTextOrTextSelectionChanged: function(evt) {
    if (!evt.target.state.editable)
      return;

    // Don't process nodes inside of web content if ChromeVox Next is inactive.
    if (evt.target.root.role != RoleType.desktop &&
        this.mode_ === ChromeVoxMode.CLASSIC)
      return;

    if (!evt.target.state.focused)
      return;

    if (evt.target.role != RoleType.textField)
      return;

    if (!this.currentRange_) {
      this.onEventDefault(evt);
      this.currentRange_ = cursors.Range.fromNode(evt.target);
    }

    this.createEditableTextHandlerIfNeeded_(evt.target);

    var textChangeEvent = new cvox.TextChangeEvent(
        evt.target.value,
        evt.target.textSelStart,
        evt.target.textSelEnd,
        true);  // triggered by user

    this.editableTextHandler_.changed(textChangeEvent);

    new Output().withBraille(
            this.currentRange_, null, evt.type)
        .go();
  },

  /**
   * Provides all feedback once a value changed event fires.
   * @param {Object} evt
   */
  onValueChanged: function(evt) {
    // Don't process nodes inside of web content if ChromeVox Next is inactive.
    if (evt.target.root.role != RoleType.desktop &&
        this.mode_ === ChromeVoxMode.CLASSIC)
      return;

    if (!evt.target.state.focused)
      return;

    // Value change events fire on web text fields and text areas when pressing
    // enter; suppress them.
    if (!this.currentRange_ ||
        evt.target.role != RoleType.textField) {
      this.onEventDefault(evt);
      this.currentRange_ = cursors.Range.fromNode(evt.target);
    }
  },

  /**
   * Called when the automation tree is changed.
   * @param {chrome.automation.TreeChange} treeChange
   */
  onTreeChange: function(treeChange) {
    if (this.mode_ === ChromeVoxMode.CLASSIC)
      return;

    var node = treeChange.target;
    if (!node.containerLiveStatus)
      return;

    if (node.containerLiveRelevant.indexOf('additions') >= 0 &&
        treeChange.type == 'nodeCreated')
      this.outputLiveRegionChange_(node, null);
    if (node.containerLiveRelevant.indexOf('text') >= 0 &&
        treeChange.type == 'nodeChanged')
      this.outputLiveRegionChange_(node, null);
    if (node.containerLiveRelevant.indexOf('removals') >= 0 &&
        treeChange.type == 'nodeRemoved')
      this.outputLiveRegionChange_(node, '@live_regions_removed');
  },

  /**
   * Given a node that needs to be spoken as part of a live region
   * change and an additional optional format string, output the
   * live region description.
   * @param {!chrome.automation.AutomationNode} node The changed node.
   * @param {?string} opt_prependFormatStr If set, a format string for
   *     cvox2.Output to prepend to the output.
   * @private
   */
  outputLiveRegionChange_: function(node, opt_prependFormatStr) {
    var range = cursors.Range.fromNode(node);
    var output = new Output();
    if (opt_prependFormatStr) {
      output.format(opt_prependFormatStr);
    }
    output.withSpeech(range, null, Output.EventType.NAVIGATE);
    output.withSpeechCategory(cvox.TtsCategory.LIVE);
    output.go();
  },

  /**
   * Returns true if the url should have Classic running.
   * @return {boolean}
   * @private
   */
  shouldEnableClassicForUrl_: function(url) {
    return this.mode_ != ChromeVoxMode.FORCE_NEXT &&
        !this.isWhitelistedForCompat_(url) &&
        !this.isWhitelistedForNext_(url);
  },

  /**
   * @return {boolean}
   * @private
   */
  isWhitelistedForCompat_: function(url) {
    return url.indexOf('chrome://md-settings') != -1 ||
          url.indexOf('chrome://downloads') != -1 ||
          url.indexOf('chrome://oobe/login') != -1 ||
          url.indexOf(
              'https://accounts.google.com/embedded/setup/chromeos') === 0 ||
          url === '';
  },

  /**
   * @private
   * @param {string} url
   * @return {boolean} Whether the given |url| is whitelisted.
   */
  isWhitelistedForNext_: function(url) {
    return this.whitelist_.some(function(item) {
      return url.indexOf(item) != -1;
    }.bind(this));
  },

  /**
   * Setup ChromeVox variants.
   * @param {string} url
   * @private
   */
  setupChromeVoxVariants_: function(url) {
    var mode = this.mode_;
    if (mode != ChromeVoxMode.FORCE_NEXT) {
      if (this.isWhitelistedForCompat_(url))
        mode = ChromeVoxMode.COMPAT;
      else if (this.isWhitelistedForNext_(url))
        mode = ChromeVoxMode.NEXT;
      else
        mode = ChromeVoxMode.CLASSIC;
    }

    this.setChromeVoxMode(mode);
  },

  /**
   * Disables classic ChromeVox in current web content.
   */
  disableClassicChromeVox_: function() {
    cvox.ExtensionBridge.send({
        message: 'SYSTEM_COMMAND',
        command: 'killChromeVox'
    });
  },

  /**
   * Sets the current ChromeVox mode.
   * @param {ChromeVoxMode} mode
   */
  setChromeVoxMode: function(mode) {
    if (mode === ChromeVoxMode.NEXT ||
        mode === ChromeVoxMode.COMPAT ||
        mode === ChromeVoxMode.FORCE_NEXT) {
      if (chrome.commands &&
          !chrome.commands.onCommand.hasListener(this.onGotCommand))
        chrome.commands.onCommand.addListener(this.onGotCommand);
    } else {
      if (chrome.commands &&
          chrome.commands.onCommand.hasListener(this.onGotCommand))
        chrome.commands.onCommand.removeListener(this.onGotCommand);
    }

    chrome.tabs.query({active: true}, function(tabs) {
      if (mode === ChromeVoxMode.CLASSIC) {
        // This case should do nothing because Classic gets injected by the
        // extension system via our manifest. Once ChromeVox Next is enabled
        // for tabs, re-enable.
        // cvox.ChromeVox.injectChromeVoxIntoTabs(tabs);
      } else {
        // When in compat mode, if the focus is within the desktop tree proper,
        // then do not disable content scripts.
        if (this.currentRange_.start.node.root.role == 'desktop')
          return;

        this.disableClassicChromeVox_();
      }
    }.bind(this));

    this.mode_ = mode;
  },

  /**
   * @param {!cvox.Spannable} text
   * @param {number} position
   * @private
   */
  brailleRoutingCommand_: function(text, position) {
    var actionNode = null;
    var selectionSpan = null;
    text.getSpans(position).forEach(function(span) {
      if (span instanceof Output.SelectionSpan) {
        selectionSpan = span;
      } else if (span instanceof Output.NodeSpan) {
        if (!actionNode ||
            (text.getSpanEnd(actionNode) - text.getSpanStart(actionNode) >
            text.getSpanEnd(span) - text.getSpanStart(span))) {
          actionNode = span.node;
        }
      }
    });
    if (!actionNode)
      return;
    if (actionNode.role === RoleType.inlineTextBox)
      actionNode = actionNode.parent;
    actionNode.doDefault();
    if (selectionSpan) {
      var start = text.getSpanStart(selectionSpan);
      actionNode.setSelection(position - start, position - start);
    }
  },

  /**
   * Create an editable text handler for the given node if needed.
   * @param {Object} node
   */
  createEditableTextHandlerIfNeeded_: function(node) {
    if (!this.editableTextHandler_ || node != this.currentRange_.start.node) {
      var start = node.textSelStart;
      var end = node.textSelEnd;
      if (start > end) {
        var tempOffset = end;
        end = start;
        start = tempOffset;
      }

      this.editableTextHandler_ =
          new cvox.ChromeVoxEditableTextBase(
              node.value,
              start,
              end,
              node.state.protected,
              cvox.ChromeVox.tts);
    }
  }
};

/** @type {Background} */
global.backgroundObj = new Background();

});  // goog.scope
