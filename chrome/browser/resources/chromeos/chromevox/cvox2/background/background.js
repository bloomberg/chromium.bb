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
goog.require('Output');
goog.require('Output.EventType');
goog.require('cursors.Cursor');
goog.require('cvox.BrailleKeyCommand');
goog.require('cvox.ChromeVoxEditableTextBase');
goog.require('cvox.ExtensionBridge');
goog.require('cvox.NavBraille');

goog.scope(function() {
var AutomationNode = chrome.automation.AutomationNode;
var Dir = AutomationUtil.Dir;
var EventType = chrome.automation.EventType;

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
  this.mode_ = ChromeVoxMode.CLASSIC;

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
    alert: this.onEventDefault,
    focus: this.onFocus,
    hover: this.onEventDefault,
    loadComplete: this.onLoadComplete,
    menuStart: this.onEventDefault,
    menuEnd: this.onEventDefault,
    menuListValueChanged: this.onEventDefault,
    textChanged: this.onTextOrTextSelectionChanged,
    textSelectionChanged: this.onTextOrTextSelectionChanged,
    valueChanged: this.onValueChanged
  };

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
    var webView = desktop.find({role: chrome.automation.RoleType.webView,
                                state: {focused: true}});
    if (webView) {
      var root = webView.find({role: chrome.automation.RoleType.rootWebArea});
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
        current = current.move(cursors.Unit.NODE, Dir.FORWARD);
        break;
      case 'previousElement':
        current = current.move(cursors.Unit.NODE, Dir.BACKWARD);
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
          if (actionNode.role == chrome.automation.RoleType.inlineTextBox)
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
          if (actionNode.role == chrome.automation.RoleType.inlineTextBox)
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
          cvox.ChromeVox.tts.speak(cvox.ChromeVox.msgs.getMsg(predErrorMsg),
                                   cvox.QueueMode.FLUSH);
        }
        return;
      }
    }

    if (current) {
      // TODO(dtseng): Figure out what it means to focus a range.
      var actionNode = current.start.node;
      if (actionNode.role == chrome.automation.RoleType.inlineTextBox)
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
        (prevRange.start.node.root != node.root &&
        node.state.focused))
      this.setupChromeVoxVariants_(node.root.docUrl || '');

    // Don't process nodes inside of web content if ChromeVox Next is inactive.
    if (node.root.role != chrome.automation.RoleType.desktop &&
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
   * Provides all feedback once a focus event fires.
   * @param {Object} evt
   */
  onFocus: function(evt) {
    var node = evt.target;

    // It doesn't make sense to focus the containing web area if a descendant
    // has focused state.
    if (node.role == 'rootWebArea') {
      node = AutomationUtil.findNodePost(node,
                                         Dir.FORWARD,
                                         AutomationPredicate.focused) || node;
    }
    this.onEventDefault({target: node, type: 'focus'});
  },

  /**
   * Provides all feedback once a load complete event fires.
   * @param {Object} evt
   */
  onLoadComplete: function(evt) {
    this.setupChromeVoxVariants_(evt.target.docUrl);

    // Don't process nodes inside of web content if ChromeVox Next is inactive.
    if (evt.target.root.role != chrome.automation.RoleType.desktop &&
        this.mode_ === ChromeVoxMode.CLASSIC)
      return;

    if (this.currentRange_)
      return;

    var root = evt.target;
    var webView = root;
    while (webView && webView.role != chrome.automation.RoleType.webView)
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
    // Don't process nodes inside of web content if ChromeVox Next is inactive.
    if (evt.target.root.role != chrome.automation.RoleType.desktop &&
        this.mode_ === ChromeVoxMode.CLASSIC)
      return;

    if (!evt.target.state.focused)
      return;

    if (!this.currentRange_) {
      this.onEventDefault(evt);
      this.currentRange_ = cursors.Range.fromNode(evt.target);
    }

    var textChangeEvent = new cvox.TextChangeEvent(
        evt.target.value,
        evt.target.textSelStart,
        evt.target.textSelEnd,
        true);  // triggered by user
    if (!this.editableTextHandler ||
        evt.target != this.currentRange_.start.node) {
      this.editableTextHandler =
          new cvox.ChromeVoxEditableTextBase(
              textChangeEvent.value,
              textChangeEvent.start,
              textChangeEvent.end,
              evt.target.state['protected'],
              cvox.ChromeVox.tts);
    }

    this.editableTextHandler.changed(textChangeEvent);
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
    if (evt.target.root.role != chrome.automation.RoleType.desktop &&
        this.mode_ === ChromeVoxMode.CLASSIC)
      return;

    if (!evt.target.state.focused)
      return;

    // Value change events fire on web text fields and text areas when pressing
    // enter; suppress them.
    if (!this.currentRange_ ||
        evt.target.role != chrome.automation.RoleType.textField) {
      this.onEventDefault(evt);
      this.currentRange_ = cursors.Range.fromNode(evt.target);
    }
  },

  /**
   * Called when the automation tree is changed.
   * @param {chrome.automation.TreeChange} treeChange
   */
  onTreeChange: function(treeChange) {
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
      if (!chrome.commands.onCommand.hasListener(this.onGotCommand))
        chrome.commands.onCommand.addListener(this.onGotCommand);
    } else {
      if (chrome.commands.onCommand.hasListener(this.onGotCommand))
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
    if (actionNode.role === chrome.automation.RoleType.inlineTextBox)
      actionNode = actionNode.parent;
    actionNode.doDefault();
    if (selectionSpan) {
      var start = text.getSpanStart(selectionSpan);
      actionNode.setSelection(position - start, position - start);
    }
  }
};

/** @type {Background} */
global.backgroundObj = new Background();

});  // goog.scope
