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

  document.addEventListener('keydown', this.onKeyDown.bind(this), false);
  document.addEventListener('keyup', this.onKeyUp.bind(this), false);
  cvox.ChromeVoxKbHandler.commandHandler = this.onGotCommand.bind(this);

  // Classic keymap.
  cvox.ChromeVoxKbHandler.handlerKeyMap = cvox.KeyMap.fromDefaults();

  // Live region handler.
  this.liveRegions_ = new LiveRegions(this);

  /** @type {number} @private */
  this.passThroughKeyUpCount_ = 0;

  /** @type {boolean} @private */
  this.inExcursion_ = false;

  if (!chrome.accessibilityPrivate.setKeyboardListener)
    chrome.accessibilityPrivate.setKeyboardListener = function() {};

  if (cvox.ChromeVox.isChromeOS) {
    chrome.accessibilityPrivate.onAccessibilityGesture.addListener(
        this.onAccessibilityGesture_);
  }
};

/**
 * @const {string}
 */
Background.ISSUE_URL = 'https://code.google.com/p/chromium/issues/entry?' +
    'labels=Type-Bug,Pri-2,cvox2,OS-Chrome&' +
    'components=UI>accessibility&' +
    'description=';

/**
 * Map from gesture names (AXGesture defined in ui/accessibility/ax_enums.idl)
 *     to commands when in Classic mode.
 * @type {Object<string, string>}
 * @const
 */
Background.GESTURE_CLASSIC_COMMAND_MAP = {
  'swipeUp1': 'backward',
  'swipeDown1': 'forward',
  'swipeLeft1': 'left',
  'swipeRight1': 'right',
  'swipeUp2': 'jumpToTop',
  'swipeDown2': 'readFromhere',
};

/**
 * Map from gesture names (AXGesture defined in ui/accessibility/ax_enums.idl)
 *     to commands when in Classic mode.
 * @type {Object<string, string>}
 * @const
 */
Background.GESTURE_NEXT_COMMAND_MAP = {
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
      window['prefs'].switchToKeyMap('keymap_classic');
    else
      window['prefs'].switchToKeyMap('keymap_next');

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

    // note that |this.currentRange_| can *change* because the request is
    // async. Save it to ensure we're looking at the currentRange at this moment
    // in time.
    var cur = this.currentRange_;
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
        if (cur && !cur.isWebRange())
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

    // If switching to Classic from any automation-API-based mode,
    // clear the focus ring.
    if (mode === ChromeVoxMode.CLASSIC && mode != this.mode_) {
      if (cvox.ChromeVox.isChromeOS)
        chrome.accessibilityPrivate.setFocusRing([]);
    }

    // If switching away from Classic to any automation-API-based mode,
    // update the range based on what's focused.
    if (this.mode_ === ChromeVoxMode.CLASSIC && mode != this.mode_) {
      chrome.automation.getFocus((function(focus) {
        if (focus)
          this.setCurrentRange(cursors.Range.fromNode(focus));
      }).bind(this));
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

    // Is the range invalid?
    if (newRange.start.node.role === undefined ||
        newRange.end.node.role === undefined) {
      // Restore range to the focused location.
      chrome.automation.getFocus(function(f) {
        newRange = cursors.Range.fromNode(f);
      });
    }

    if (!this.inExcursion_)
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
    // Check for loss of focus which results in us invalidating our current
    // range. Note this call is synchronis.
    chrome.automation.getFocus(function(focusedNode) {
      if (!focusedNode)
        this.currentRange_ = null;
    }.bind(this));

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

          new Output().withRichSpeechAndBraille(
                  this.currentRange_, prevRange, Output.EventType.NAVIGATE)
              .onSpeechEnd(continueReading)
              .go();
        }.bind(this);

        new Output().withRichSpeechAndBraille(
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

        // If the new mode is Classic, return now so we don't announce
        // anything more.
        if (newMode == ChromeVoxMode.CLASSIC)
          return false;
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
        this.startExcursion();
        (new PanelCommand(PanelCommandType.OPEN_MENUS)).send();
        return false;
      case 'toggleSearchWidget':
        (new PanelCommand(PanelCommandType.SEARCH)).send();
        return false;
      case 'showKbExplorerPage':
        var explorerPage = {url: 'chromevox/background/kbexplorer.html'};
        chrome.tabs.create(explorerPage);
        break;
      case 'decreaseTtsRate':
        this.increaseOrDecreaseSpeechProperty_(cvox.AbstractTts.RATE, false);
        return false;
      case 'increaseTtsRate':
        this.increaseOrDecreaseSpeechProperty_(cvox.AbstractTts.RATE, true);
        return false;
      case 'decreaseTtsPitch':
        this.increaseOrDecreaseSpeechProperty_(cvox.AbstractTts.PITCH, false);
        return false;
      case 'increaseTtsPitch':
        this.increaseOrDecreaseSpeechProperty_(cvox.AbstractTts.PITCH, true);
        return false;
      case 'decreaseTtsVolume':
        this.increaseOrDecreaseSpeechProperty_(cvox.AbstractTts.VOLUME, false);
        return false;
      case 'increaseTtsVolume':
        this.increaseOrDecreaseSpeechProperty_(cvox.AbstractTts.VOLUME, true);
        return false;
      case 'stopSpeech':
        cvox.ChromeVox.tts.stop();
        global.isReadingContinuously = false;
        return false;
      case 'toggleEarcons':
        cvox.AbstractEarcons.enabled = !cvox.AbstractEarcons.enabled;
        var announce = cvox.AbstractEarcons.enabled ?
            Msgs.getMsg('earcons_on') :
            Msgs.getMsg('earcons_off');
        cvox.ChromeVox.tts.speak(
            announce, cvox.QueueMode.FLUSH,
            cvox.AbstractTts.PERSONALITY_ANNOTATION);
        return false;
      case 'cycleTypingEcho':
        cvox.ChromeVox.typingEcho =
            cvox.TypingEcho.cycle(cvox.ChromeVox.typingEcho);
        var announce = '';
        switch (cvox.ChromeVox.typingEcho) {
          case cvox.TypingEcho.CHARACTER:
            announce = Msgs.getMsg('character_echo');
            break;
          case cvox.TypingEcho.WORD:
            announce = Msgs.getMsg('word_echo');
            break;
          case cvox.TypingEcho.CHARACTER_AND_WORD:
            announce = Msgs.getMsg('character_and_word_echo');
            break;
          case cvox.TypingEcho.NONE:
            announce = Msgs.getMsg('none_echo');
            break;
        }
        cvox.ChromeVox.tts.speak(
            announce, cvox.QueueMode.FLUSH,
            cvox.AbstractTts.PERSONALITY_ANNOTATION);
        return false;
      case 'cyclePunctuationEcho':
        cvox.ChromeVox.tts.speak(Msgs.getMsg(
            global.backgroundTts.cyclePunctuationEcho()),
                       cvox.QueueMode.FLUSH);
        return false;
      case 'speakTimeAndDate':
        chrome.automation.getDesktop(function(d) {
          // First, try speaking the on-screen time.
          var allTime = d.findAll({role: RoleType.time});
          allTime.filter(function(t) {
            return t.root.role == RoleType.desktop;
          });

          var timeString = '';
          allTime.forEach(function(t) {
            if (t.name)
              timeString = t.name;
          });
          if (timeString) {
            cvox.ChromeVox.tts.speak(timeString, cvox.QueueMode.FLUSH);
          } else {
            // Fallback to the old way of speaking time.
            var output = new Output();
            var dateTime = new Date();
            output.withString(
                dateTime.toLocaleTimeString() +
                    ', ' + dateTime.toLocaleDateString()).go();
          }
        });
        return false;
      case 'readCurrentTitle':
        var target = this.currentRange_.start.node;
        var output = new Output();

        if (target.root.role == RoleType.rootWebArea) {
          // Web.
          target = target.root;
          output.withString(target.name || target.docUrl);
        } else {
          // Views.
          while (target.role != RoleType.window)
            target = target.parent;
          if (target)
            output.withString(target.name || '');
        }
        output.go();
        return false;
      case 'readCurrentURL':
        var output = new Output();
        var target = this.currentRange_.start.node.root;
        output.withString(target.docUrl || '').go();
        return false;
      case 'reportIssue':
        var url = Background.ISSUE_URL;
        var description = {};
        description['Mode'] = this.mode_;
        description['Version'] = chrome.app.getDetails().version;
        description['Reproduction Steps'] = '%0a1.%0a2.%0a3.';
        for (var key in description)
          url += key + ':%20' + description[key] + '%0a';
        chrome.tabs.create({url: url});
        return false;
      default:
        return true;
    }

    if (pred) {
      var node = AutomationUtil.findNextNode(
          current.getBound(dir).node, dir, pred, {skipInitialAncestry: true});

      if (node) {
        node = AutomationUtil.findNodePre(
            node, dir, AutomationPredicate.element) || node;
      }

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
    if (actionNode.role != RoleType.iframe && !actionNode.state.focused &&
        !AutomationPredicate.container(actionNode))
      actionNode.focus();

    var prevRange = this.currentRange_;
    this.setCurrentRange(range);

    range.select();

    new Output().withRichSpeechAndBraille(
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
   * @private
   */
  restoreCurrentRange_: function() {
    if (this.savedRange_) {
      var node = this.savedRange_.start.node;
      var containingWebView = node;
      while (containingWebView && containingWebView.role != RoleType.webView)
        containingWebView = containingWebView.parent;

      if (containingWebView) {
        // Focusing the webView causes a focus change event which steals focus
        // away from the saved range.
        var saved = this.savedRange_;
        var setSavedRange = function(e) {
          if (e.target.root == saved.start.node.root)
            this.navigateToRange_(saved);
          node.root.removeEventListener(EventType.focus, setSavedRange, true);
        }.bind(this);
        node.root.addEventListener(EventType.focus, setSavedRange, true);
        containingWebView.focus();
      }
      this.navigateToRange_(this.savedRange_);
      this.savedRange_ = null;
    }
  },

  /**
   * Move ChromeVox without saving any ranges.
   */
  startExcursion: function() {
    this.inExcursion_ = true;
  },

  /**
   * Move ChromeVox back to the last saved range.
   */
  endExcursion: function() {
    this.inExcursion_ = false;
    this.restoreCurrentRange_();
  },

  /**
   * Move ChromeVox back to the last saved range.
   */
  saveExcursion: function() {
    this.savedRange_ =
        new cursors.Range(this.currentRange_.start, this.currentRange_.end);
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
    if (this.mode_ == ChromeVoxMode.CLASSIC) {
      if (this.handleClassicGesture_(gesture))
        return;
    }

    var command = Background.GESTURE_NEXT_COMMAND_MAP[gesture];
    if (command)
      this.onGotCommand(command);
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
