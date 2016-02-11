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
goog.require('ChromeVoxState');
goog.require('LiveRegions');
goog.require('NextEarcons');
goog.require('Output');
goog.require('Output.EventType');
goog.require('PanelCommand');
goog.require('constants');
goog.require('cursors.Cursor');
goog.require('cvox.BrailleKeyCommand');
goog.require('cvox.ChromeVoxEditableTextBase');
goog.require('cvox.ChromeVoxKbHandler');
goog.require('cvox.ClassicEarcons');
goog.require('cvox.ExtensionBridge');
goog.require('cvox.NavBraille');

goog.scope(function() {
var AutomationNode = chrome.automation.AutomationNode;
var Dir = constants.Dir;
var EventType = chrome.automation.EventType;
var RoleType = chrome.automation.RoleType;

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
   * Regular expression for blacklisting classic.
   * @type {RegExp}
   * @private
   */
  this.classicBlacklistRegExp_ = Background.globsToRegExp_(
      chrome.runtime.getManifest()['content_scripts'][0]['exclude_globs']);

  /**
   * @type {cursors.Range}
   * @private
   */
  this.currentRange_ = null;

  /**
   * @type {cursors.Range}
   * @private
   */
  this.savedRange_ = null;

  /**
   * Which variant of ChromeVox is active.
   * @type {ChromeVoxMode}
   * @private
   */
  this.mode_ = ChromeVoxMode.COMPAT;

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
      if (this.mode_ === ChromeVoxMode.FORCE_NEXT ||
          this.mode_ === ChromeVoxMode.NEXT) {
        return this.nextEarcons_;
      } else {
        return this.classicEarcons_;
      }
    }).bind(this)
  });

  if (cvox.ChromeVox.isChromeOS) {
    Object.defineProperty(cvox.ChromeVox, 'modKeyStr', {
      get: function() {
        return (this.mode_ == ChromeVoxMode.CLASSIC || this.mode_ ==
            ChromeVoxMode.COMPAT) ?
                'Search+Shift' : 'Search';
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

  document.addEventListener('keydown', this.onKeyDown.bind(this), true);
  document.addEventListener('keyup', this.onKeyUp.bind(this), true);
  cvox.ChromeVoxKbHandler.commandHandler = this.onGotCommand.bind(this);

  // Classic keymap.
  cvox.ChromeVoxKbHandler.handlerKeyMap = cvox.KeyMap.fromDefaults();

  // Live region handler.
  this.liveRegions_ = new LiveRegions(this);

  /** @type {number} @private */
  this.passThroughKeyUpCount_ = 0;

  if (!chrome.accessibilityPrivate.setKeyboardListener)
    chrome.accessibilityPrivate.setKeyboardListener = function() {};
};

Background.prototype = {
  __proto__: ChromeVoxState.prototype,

  /**
   * @override
   */
  getMode: function() {
    return this.mode_;
  },

  /**
   * @override
   */
  setMode: function(mode, opt_injectClassic) {
    // Switching key maps potentially affects the key codes that involve
    // sequencing. Without resetting this list, potentially stale key codes
    // remain. The key codes themselves get pushed in
    // cvox.KeySequence.deserialize which gets called by cvox.KeyMap.
    cvox.ChromeVox.sequenceSwitchKeyCodes = [];
    if (mode === ChromeVoxMode.CLASSIC || mode === ChromeVoxMode.COMPAT)
      cvox.ChromeVoxKbHandler.handlerKeyMap = cvox.KeyMap.fromDefaults();
    else
      cvox.ChromeVoxKbHandler.handlerKeyMap = cvox.KeyMap.fromNext();

    if (mode == ChromeVoxMode.CLASSIC) {
      if (chrome.commands &&
          chrome.commands.onCommand.hasListener(this.onGotCommand))
        chrome.commands.onCommand.removeListener(this.onGotCommand);
      chrome.accessibilityPrivate.setKeyboardListener(false, false);
    } else {
      if (chrome.commands &&
          !chrome.commands.onCommand.hasListener(this.onGotCommand))
        chrome.commands.onCommand.addListener(this.onGotCommand);
        chrome.accessibilityPrivate.setKeyboardListener(
            true, cvox.ChromeVox.isStickyPrefOn);
    }

    chrome.tabs.query({active: true}, function(tabs) {
      if (mode === ChromeVoxMode.CLASSIC) {
        // Generally, we don't want to inject classic content scripts as it is
        // done by the extension system at document load. The exception is when
        // we toggle classic on manually as part of a user command.
        if (opt_injectClassic)
          cvox.ChromeVox.injectChromeVoxIntoTabs(tabs);
      } else {
        // When in compat mode, if the focus is within the desktop tree proper,
        // then do not disable content scripts.
        if (this.currentRange_ && !this.currentRange_.isWebRange())
          return;

        this.disableClassicChromeVox_();
      }
    }.bind(this));

    // If switching out of a ChromeVox Next mode, make sure we cancel
    // the progress loading sound just in case.
    if ((this.mode_ === ChromeVoxMode.NEXT ||
         this.mode_ === ChromeVoxMode.FORCE_NEXT) &&
        this.mode_ != mode) {
      cvox.ChromeVox.earcons.cancelEarcon(cvox.Earcon.PAGE_START_LOADING);
    }

    if (mode === ChromeVoxMode.NEXT ||
        mode === ChromeVoxMode.FORCE_NEXT) {
      (new PanelCommand(PanelCommandType.ENABLE_MENUS)).send();
    } else {
      (new PanelCommand(PanelCommandType.DISABLE_MENUS)).send();
    }

    this.mode_ = mode;
  },

  /**
   * Mode refreshes takes into account both |url| and the current ChromeVox
   *    range. The latter gets used to decide if the user is or isn't in web
   *    content. The focused state also needs to be set for this info to be
   *    reliable.
   * @override
   */
  refreshMode: function(url) {
    var mode = this.mode_;
    if (mode != ChromeVoxMode.FORCE_NEXT) {
      if (this.isWhitelistedForNext_(url)) {
        mode = ChromeVoxMode.NEXT;
      } else if (this.isBlacklistedForClassic_(url) || (this.currentRange_ &&
          !this.currentRange_.isWebRange() &&
          this.currentRange_.start.node.state.focused)) {
        mode = ChromeVoxMode.COMPAT;
      } else {
        mode = ChromeVoxMode.CLASSIC;
      }
    }

    this.setMode(mode);
  },

  /**
   * @override
   */
  getCurrentRange: function() {
    return this.currentRange_;
  },

  /**
   * @override
   */
  setCurrentRange: function(newRange) {
    if (!newRange)
      return;

    var panelUrl = chrome.extension.getURL('cvox2/background/panel.html');
    if (newRange.start.node.root.docUrl.indexOf(panelUrl) != 0)
      this.savedRange_ = new cursors.Range(newRange.start, newRange.end);

    this.currentRange_ = newRange;

    if (this.currentRange_)
      this.currentRange_.start.node.makeVisible();
  },

  /** Forces ChromeVox Next to be active for all tabs. */
  forceChromeVoxNextActive: function() {
    this.setMode(ChromeVoxMode.FORCE_NEXT);
  },

  /**
   * Handles ChromeVox Next commands.
   * @param {string} command
   * @param {boolean=} opt_bypassModeCheck Always tries to execute the command
   *    regardless of mode.
   * @return {boolean} True if the command should propagate.
   */
  onGotCommand: function(command, opt_bypassModeCheck) {
    if (!this.currentRange_)
      return true;

    if (this.mode_ == ChromeVoxMode.CLASSIC && !opt_bypassModeCheck)
      return true;

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
      case 'forward':
      case 'nextLine':
        current = current.move(cursors.Unit.LINE, Dir.FORWARD);
        break;
      case 'backward':
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
      case 'nextCheckbox':
        dir = Dir.FORWARD;
        pred = AutomationPredicate.checkBox;
        predErrorMsg = 'no_next_checkbox';
        break;
      case 'previousCheckbox':
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
      case 'right':
      case 'nextObject':
        current = current.move(cursors.Unit.DOM_NODE, Dir.FORWARD);
        break;
      case 'left':
      case 'previousObject':
        current = current.move(cursors.Unit.DOM_NODE, Dir.BACKWARD);
        break;
      case 'jumpToTop':
        var node =
            AutomationUtil.findNodePost(current.start.node.root,
                                        Dir.FORWARD,
                                        AutomationPredicate.leaf);
        if (node)
          current = cursors.Range.fromNode(node);
        break;
      case 'jumpToBottom':
        var node =
            AutomationUtil.findNodePost(current.start.node.root,
                                        Dir.BACKWARD,
                                        AutomationPredicate.leaf);
        if (node)
          current = cursors.Range.fromNode(node);
        break;
      case 'forceClickOnCurrentItem':
      case 'performDefaultAction':
        if (this.currentRange_) {
          var actionNode = this.currentRange_.start.node;
          if (actionNode.role == RoleType.inlineTextBox)
            actionNode = actionNode.parent;
          actionNode.doDefault();
        }
        // Skip all other processing; if focus changes, we should get an event
        // for that.
        return false;
      case 'readFromHere':
        global.isReadingContinuously = true;
        var continueReading = function() {
          if (!global.isReadingContinuously || !this.currentRange_)
            return;

          var prevRange = this.currentRange_;
          var newRange =
              this.currentRange_.move(cursors.Unit.DOM_NODE, Dir.FORWARD);

          // Stop if we've wrapped back to the document.
          var maybeDoc = newRange.start.node;
          if (maybeDoc.role == RoleType.rootWebArea &&
              maybeDoc.parent.root.role == RoleType.desktop) {
            global.isReadingContinuously = false;
            return;
          }

          this.setCurrentRange(newRange);

          new Output().withSpeechAndBraille(
                  this.currentRange_, prevRange, Output.EventType.NAVIGATE)
              .onSpeechEnd(continueReading)
              .go();
        }.bind(this);

        new Output().withSpeechAndBraille(
                this.currentRange_, null, Output.EventType.NAVIGATE)
            .onSpeechEnd(continueReading)
            .go();

        return false;
      case 'contextMenu':
        if (this.currentRange_) {
          var actionNode = this.currentRange_.start.node;
          if (actionNode.role == RoleType.inlineTextBox)
            actionNode = actionNode.parent;
          actionNode.showContextMenu();
          return false;
        }
        break;
      case 'showOptionsPage':
        chrome.runtime.openOptionsPage();
        break;
      case 'toggleChromeVox':
        if (cvox.ChromeVox.isChromeOS)
          return false;

        cvox.ChromeVox.isActive = !cvox.ChromeVox.isActive;
        if (!cvox.ChromeVox.isActive) {
          var msg = Msgs.getMsg('chromevox_inactive');
          cvox.ChromeVox.tts.speak(msg, cvox.QueueMode.FLUSH);
          return false;
        }
        break;
      case 'toggleChromeVoxVersion':
        var newMode;
        if (this.mode_ == ChromeVoxMode.FORCE_NEXT) {
          var inWeb = current.isWebRange();
          newMode = inWeb ? ChromeVoxMode.CLASSIC : ChromeVoxMode.COMPAT;
        } else {
          newMode = ChromeVoxMode.FORCE_NEXT;
        }
        this.setMode(newMode, true);

        var isClassic =
            newMode == ChromeVoxMode.CLASSIC || newMode == ChromeVoxMode.COMPAT;

        // Leaving unlocalized as 'next' isn't an official name.
        cvox.ChromeVox.tts.speak(isClassic ?
            'classic' : 'next', cvox.QueueMode.FLUSH, {doNotInterrupt: true});
        break;
      case 'toggleStickyMode':
        cvox.ChromeVoxBackground.setPref('sticky',
                                         !cvox.ChromeVox.isStickyPrefOn,
                                         true);

        if (cvox.ChromeVox.isStickyPrefOn)
          chrome.accessibilityPrivate.setKeyboardListener(true, true);
        else
          chrome.accessibilityPrivate.setKeyboardListener(true, false);
        return false;
      case 'passThroughMode':
        cvox.ChromeVox.passThroughMode = true;
        cvox.ChromeVox.tts.speak(
            Msgs.getMsg('pass_through_key'), cvox.QueueMode.QUEUE);
        return true;
      case 'openChromeVoxMenus':
        (new PanelCommand(PanelCommandType.OPEN_MENUS)).send();
        break;
      case 'decreaseTtsRate':
        this.increaseOrDecreaseSpeechProperty_(cvox.AbstractTts.RATE, false);
        break;
      case 'increaseTtsRate':
        this.increaseOrDecreaseSpeechProperty_(cvox.AbstractTts.RATE, true);
        break;
      case 'decreaseTtsPitch':
        this.increaseOrDecreaseSpeechProperty_(cvox.AbstractTts.PITCH, false);
        break;
      case 'increaseTtsPitch':
        this.increaseOrDecreaseSpeechProperty_(cvox.AbstractTts.PITCH, true);
        break;
      case 'decreaseTtsVolume':
        this.increaseOrDecreaseSpeechProperty_(cvox.AbstractTts.VOLUME, false);
        break;
      case 'increaseTtsVolume':
        this.increaseOrDecreaseSpeechProperty_(cvox.AbstractTts.VOLUME, true);
        break;
      default:
        return true;
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
        return false;
      }
    }

    if (current)
      this.navigateToRange_(current);

    return false;
  },

  /**
   * Increase or decrease a speech property and make an announcement.
   * @param {string} propertyName The name of the property to change.
   * @param {boolean} increase If true, increases the property value by one
   *     step size, otherwise decreases.
   */
  increaseOrDecreaseSpeechProperty_: function(propertyName, increase) {
    cvox.ChromeVox.tts.increaseOrDecreaseProperty(propertyName, increase);
    var announcement;
    var valueAsPercent = Math.round(
        cvox.ChromeVox.tts.propertyToPercentage(propertyName) * 100);
    switch (propertyName) {
      case cvox.AbstractTts.RATE:
        announcement = Msgs.getMsg('announce_rate', [valueAsPercent]);
        break;
      case cvox.AbstractTts.PITCH:
        announcement = Msgs.getMsg('announce_pitch', [valueAsPercent]);
        break;
      case cvox.AbstractTts.VOLUME:
        announcement = Msgs.getMsg('announce_volume', [valueAsPercent]);
        break;
    }
    if (announcement) {
      cvox.ChromeVox.tts.speak(
          announcement, cvox.QueueMode.FLUSH,
          cvox.AbstractTts.PERSONALITY_ANNOTATION);
    }
  },

  /**
   * Navigate to the given range - it both sets the range and outputs it.
   * @param {!cursors.Range} range The new range.
   * @private
   */
  navigateToRange_: function(range) {
    // TODO(dtseng): Figure out what it means to focus a range.
    var actionNode = range.start.node;
    if (actionNode.role == RoleType.inlineTextBox)
      actionNode = actionNode.parent;

    // Iframes, when focused, causes the child webArea to fire focus event.
    // This can result in getting stuck when navigating backward.
    if (actionNode.role != RoleType.iframe && !actionNode.state.focused)
      actionNode.focus();

    var prevRange = this.currentRange_;
    this.setCurrentRange(range);

    new Output().withSpeechAndBraille(
        range, prevRange, Output.EventType.NAVIGATE)
        .withQueueMode(cvox.QueueMode.FLUSH)
        .go();
  },

  /**
   * Handles key down events.
   * @param {Event} evt The key down event to process.
   * @return {boolean} True if the default action should be performed.
   */
  onKeyDown: function(evt) {
    evt.stickyMode = cvox.ChromeVox.isStickyModeOn() && cvox.ChromeVox.isActive;
    if (cvox.ChromeVox.passThroughMode)
      return false;

    if (this.mode_ != ChromeVoxMode.CLASSIC &&
        !cvox.ChromeVoxKbHandler.basicKeyDownActionsListener(evt)) {
      evt.preventDefault();
      evt.stopPropagation();
    }
    Output.flushNextSpeechUtterance();
    return false;
  },

  /**
   * Handles key up events.
   * @param {Event} evt The key down event to process.
   * @return {boolean} True if the default action should be performed.
   */
  onKeyUp: function(evt) {
    // Reset pass through mode once a keyup (not involving the pass through key)
    // is seen. The pass through command involves three keys.
    if (cvox.ChromeVox.passThroughMode) {
      if (this.passThroughKeyUpCount_ >= 3) {
        cvox.ChromeVox.passThroughMode = false;
        this.passThroughKeyUpCount_ = 0;
      } else {
        this.passThroughKeyUpCount_++;
      }
    }
    return false;
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
    if (this.mode_ === ChromeVoxMode.CLASSIC)
      return false;

    switch (evt.command) {
      case cvox.BrailleKeyCommand.PAN_LEFT:
        this.onGotCommand('previousObject');
        break;
      case cvox.BrailleKeyCommand.PAN_RIGHT:
        this.onGotCommand('nextObject');
        break;
      case cvox.BrailleKeyCommand.LINE_UP:
        this.onGotCommand('previousLine');
        break;
      case cvox.BrailleKeyCommand.LINE_DOWN:
        this.onGotCommand('nextLine');
        break;
      case cvox.BrailleKeyCommand.TOP:
        this.onGotCommand('jumpToTop');
        break;
      case cvox.BrailleKeyCommand.BOTTOM:
        this.onGotCommand('jumpToBottom');
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
   * Returns true if the url should have Classic running.
   * @return {boolean}
   * @private
   */
  shouldEnableClassicForUrl_: function(url) {
    return this.mode_ != ChromeVoxMode.FORCE_NEXT &&
        !this.isBlacklistedForClassic_(url) &&
        !this.isWhitelistedForNext_(url);
  },

  /**
   * @param {string} url
   * @return {boolean}
   * @private
   */
  isBlacklistedForClassic_: function(url) {
    return this.classicBlacklistRegExp_.test(url);
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
   */
  disableClassicChromeVox_: function() {
    cvox.ExtensionBridge.send({
        message: 'SYSTEM_COMMAND',
        command: 'killChromeVox'
    });
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
    if (actionNode.role === RoleType.inlineTextBox)
      actionNode = actionNode.parent;
    actionNode.doDefault();
    if (selectionSpan) {
      var start = text.getSpanStart(selectionSpan);
      var targetPosition = position - start + selectionSpan.offset;
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
        } else if (action == 'onCommand') {
          this.onGotCommand(msg['command']);
        } else if (action == 'flushNextUtterance') {
          Output.flushNextSpeechUtterance();
        }
        break;
    }
  },

  /**
   * Restore the range to the last range that was *not* in the ChromeVox
   * panel. This is used when the ChromeVox Panel closes.
   */
  restoreCurrentRange: function() {
    if (this.savedRange_) {
      var containingWebView = this.savedRange_.start.node;
      while (containingWebView && containingWebView.role != RoleType.webView)
        containingWebView = containingWebView.parent;
      if (containingWebView)
        containingWebView.focus();

      this.navigateToRange_(this.savedRange_);
      this.savedRange_ = null;
    }
  },
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

/** @type {Background} */
global.backgroundObj = new Background();

});  // goog.scope
