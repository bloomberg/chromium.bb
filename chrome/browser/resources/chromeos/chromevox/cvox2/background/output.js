// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Provides output services for ChromeVox.
 */

goog.provide('Output');
goog.provide('Output.EventType');

goog.require('AutomationUtil.Dir');
goog.require('cursors.Cursor');
goog.require('cursors.Range');
goog.require('cursors.Unit');
goog.require('cvox.AbstractEarcons');
goog.require('cvox.NavBraille');
goog.require('cvox.Spannable');
goog.require('cvox.ValueSelectionSpan');
goog.require('cvox.ValueSpan');

goog.scope(function() {
var Dir = AutomationUtil.Dir;

/**
 * An Output object formats a cursors.Range into speech, braille, or both
 * representations. This is typically a cvox.Spannable.
 *
 * The translation from Range to these output representations rely upon format
 * rules which specify how to convert AutomationNode objects into annotated
 * strings.
 * The format of these rules is as follows.
 *
 * $ prefix: used to substitute either an attribute or a specialized value from
 *     an AutomationNode. Specialized values include role and state. Attributes
 *     available for substitution are AutomationNode.prototype.attributes and
 *     AutomationNode.prototype.state.
 *     For example, $value $role $enabled
 * @ prefix: used to substitute a message. Note the ability to specify params to
 *     the message.  For example, '@tag_html' '@selected_index($text_sel_start,
 *     $text_sel_end').
 * = suffix: used to specify substitution only if not previously appended.
 *     For example, $name= would insert the name attribute only if no name
 * attribute had been inserted previously.
 * @constructor
 */
Output = function() {
  // TODO(dtseng): Include braille specific rules.
  /** @type {!cvox.Spannable} */
  this.buffer_ = new cvox.Spannable();
  /** @type {!cvox.Spannable} */
  this.brailleBuffer_ = new cvox.Spannable();
  /** @type {!Array<Object>} */
  this.locations_ = [];
  /** @type {function()} */
  this.speechStartCallback_;
  /** @type {function()} */
  this.speechEndCallback_;

  /**
   * Current global options.
   * @type {{speech: boolean, braille: boolean, location: boolean}}
   */
  this.formatOptions_ = {speech: true, braille: false, location: true};

  /**
   * Speech properties to apply to the entire output.
   * @type {!Object<string, *>}
   */
  this.speechProperties_ = {};
};

/**
 * Delimiter to use between output values.
 * @type {string}
 */
Output.SPACE = ' ';

/**
 * Rules specifying format of AutomationNodes for output.
 * @type {!Object<string, Object<string, Object<string, string>>>}
 */
Output.RULES = {
  navigate: {
    'default': {
      speak: '$name $role $value',
      braille: ''
    },
    alert: {
      speak: '!doNotInterrupt ' +
          '@aria_role_alert $name $earcon(ALERT_NONMODAL) $descendants'
    },
    button: {
      speak: '$name $earcon(BUTTON, @tag_button)'
    },
    checkBox: {
      speak: '$if($checked, @describe_checkbox_checked($name), ' +
          '@describe_checkbox_unchecked($name)) ' +
          '$if($checked, ' +
              '$earcon(CHECK_ON, @input_type_checkbox), ' +
              '$earcon(CHECK_OFF, @input_type_checkbox))'
    },
    dialog: {
      enter: '$name $role'
    },
    heading: {
      enter: '@aria_role_heading',
      speak: '@aria_role_heading $name='
    },
    inlineTextBox: {
      speak: '$value='
    },
    link: {
      enter: '$name= $visited $earcon(LINK, @tag_link)=',
      stay: '$name= $visited @tag_link',
      speak: '$name= $visited $earcon(LINK, @tag_link)='
    },
    list: {
      enter: '$role'
    },
    listItem: {
      enter: '$role'
    },
    menuItem: {
      speak: '$if($haspopup, @describe_menu_item_with_submenu($name), ' +
          '@describe_menu_item($name)) ' +
          '@describe_index($indexInParent, $parentChildCount)'
    },
    menuListOption: {
      speak: '$name $value @aria_role_menuitem ' +
          '@describe_index($indexInParent, $parentChildCount)'
    },
    paragraph: {
      speak: '$value'
    },
    popUpButton: {
      speak: '$value $name @tag_button @aria_has_popup $earcon(LISTBOX) ' +
          '$if($collapsed, @aria_expanded_false, @aria_expanded_true)'
    },
    radioButton: {
      speak: '$if($checked, @describe_radio_selected($name), ' +
          '@describe_radio_unselected($name)) ' +
          '$if($checked, ' +
              '$earcon(CHECK_ON, @input_type_radio), ' +
              '$earcon(CHECK_OFF, @input_type_radio))'
    },
    slider: {
      speak: '@describe_slider($value, $name)'
    },
    staticText: {
      speak: '$value'
    },
    textBox: {
      speak: '$name $value $earcon(EDITABLE_TEXT, @input_type_text)'
    },
    tab: {
      speak: '@describe_tab($name)'
    },
    textField: {
      speak: '$name $value $earcon(EDITABLE_TEXT, @input_type_text) $protected'
    },
    toolbar: {
      enter: '$name $role'
    },
    window: {
      enter: '$name',
      speak: '@describe_window($name) $earcon(OBJECT_OPEN)'
    }
  },
  menuStart: {
    'default': {
      speak: '@chrome_menu_opened($name) $role $earcon(OBJECT_OPEN)'
    }
  },
  menuEnd: {
    'default': {
      speak: '$earcon(OBJECT_CLOSE)'
    }
  },
  menuListValueChanged: {
    'default': {
      speak: '$value $name ' +
          '$find({"state": {"selected": true, "invisible": false}}, ' +
              '@describe_index($indexInParent, $parentChildCount)) '
    }
  },
  alert: {
    default: {
      speak: '!doNotInterrupt ' +
          '@aria_role_alert $name $earcon(ALERT_NONMODAL) $descendants'
    }
  }
};

/**
 * Alias equivalent attributes.
 * @type {!Object<string, string>}
 */
Output.ATTRIBUTE_ALIAS = {
  name: 'value',
  value: 'name'
};

/**
 * Custom actions performed while rendering an output string.
 * @param {function()} action
 * @constructor
 */
Output.Action = function(action) {
  this.action_ = action;
};

Output.Action.prototype = {
  run: function() {
    this.action_();
  }
};

/**
 * Annotation for selection.
 * @param {number} startIndex
 * @param {number} endIndex
 * @constructor
 */
Output.SelectionSpan = function(startIndex, endIndex) {
  // TODO(dtseng): Direction lost below; should preserve for braille panning.
  this.startIndex = startIndex < endIndex ? startIndex : endIndex;
  this.endIndex = endIndex > startIndex ? endIndex : startIndex;
};

/**
 * Possible events handled by ChromeVox internally.
 * @enum {string}
 */
Output.EventType = {
  NAVIGATE: 'navigate'
};

Output.prototype = {
  /**
   * Gets the output buffer for speech.
   * @return {!cvox.Spannable}
   */
  getBuffer: function() {
    return this.buffer_;
  },

  /**
   * Specify ranges for speech.
   * @param {!cursors.Range} range
   * @param {cursors.Range} prevRange
   * @param {chrome.automation.EventType|Output.EventType} type
   * @return {!Output}
   */
  withSpeech: function(range, prevRange, type) {
    this.formatOptions_ = {speech: true, braille: false, location: true};
    this.render_(range, prevRange, type, this.buffer_);
    return this;
  },

  /**
   * Specify ranges for braille.
   * @param {!cursors.Range} range
   * @param {cursors.Range} prevRange
   * @param {chrome.automation.EventType|Output.EventType} type
   * @return {!Output}
   */
  withBraille: function(range, prevRange, type) {
    this.formatOptions_ = {speech: false, braille: true, location: false};
    this.render_(range, prevRange, type, this.brailleBuffer_);
    return this;
  },

  /**
   * Specify the same ranges for speech and braille.
   * @param {!cursors.Range} range
   * @param {cursors.Range} prevRange
   * @param {chrome.automation.EventType|Output.EventType} type
   * @return {!Output}
   */
  withSpeechAndBraille: function(range, prevRange, type) {
    this.withSpeech(range, prevRange, type);
    this.withBraille(range, prevRange, type);
    return this;
  },

  /**
   * Triggers callback for a speech event.
   * @param {function()} callback
   */
  onSpeechStart: function(callback) {
    this.speechStartCallback_ = callback;
    return this;
  },

  /**
   * Triggers callback for a speech event.
   * @param {function()} callback
   */
  onSpeechEnd: function(callback) {
    this.speechEndCallback_ = callback;
    return this;
  },

  /**
   * Executes all specified output.
   */
  go: function() {
    // Speech.
    var buff = this.buffer_;
    if (buff.toString()) {
      if (this.speechStartCallback_)
        this.speechProperties_['startCallback'] = this.speechStartCallback_;
      if (this.speechEndCallback_) {
        this.speechProperties_['endCallback'] = this.speechEndCallback_;
      }

      cvox.ChromeVox.tts.speak(
          buff.toString(), cvox.QueueMode.FLUSH, this.speechProperties_);
    }

    var actions = buff.getSpansInstanceOf(Output.Action);
    if (actions) {
      actions.forEach(function(a) {
        a.run();
      });
    }

    // Braille.
    var selSpan =
        this.brailleBuffer_.getSpanInstanceOf(Output.SelectionSpan);
    var startIndex = -1, endIndex = -1;
    if (selSpan) {
      var valueStart = this.brailleBuffer_.getSpanStart(selSpan);
      var valueEnd = this.brailleBuffer_.getSpanEnd(selSpan);
      if (valueStart === undefined || valueEnd === undefined) {
        valueStart = -1;
        valueEnd = -1;
      } else {
        startIndex = valueStart + selSpan.startIndex;
        endIndex = valueStart + selSpan.endIndex;
        this.brailleBuffer_.setSpan(new cvox.ValueSpan(valueStart),
                                    valueStart, valueEnd);
        this.brailleBuffer_.setSpan(new cvox.ValueSelectionSpan(),
                                    startIndex, endIndex);
      }
    }

    var output = new cvox.NavBraille({
      text: this.brailleBuffer_,
      startIndex: startIndex,
      endIndex: endIndex
    });

    if (this.brailleBuffer_)
      cvox.ChromeVox.braille.write(output);

    // Display.
    chrome.accessibilityPrivate.setFocusRing(this.locations_);
  },

  /**
   * Renders the given range using optional context previous range and event
   * type.
   * @param {!cursors.Range} range
   * @param {cursors.Range} prevRange
   * @param {chrome.automation.EventType|string} type
   * @param {!cvox.Spannable} buff Buffer to receive rendered output.
   * @private
   */
  render_: function(range, prevRange, type, buff) {
    if (range.isSubNode())
      this.subNode_(range, prevRange, type, buff);
    else
      this.range_(range, prevRange, type, buff);
  },

  /**
   * Format the node given the format specifier.
   * @param {!chrome.automation.AutomationNode} node
   * @param {string|!Object} format The output format either specified as an
   * output template string or a parsed output format tree.
   * @param {!Object=} opt_exclude A set of attributes to exclude.
   * @private
   */
  format_: function(node, format, buff, opt_exclude) {
    opt_exclude = opt_exclude || {};
    var tokens = [];
    var args = null;

    // Hacky way to support args.
    if (typeof(format) == 'string') {
      format = format.replace(/([,:])\W/g, '$1');
      tokens = format.split(' ');
    } else {
      tokens = [format];
    }

    tokens.forEach(function(token) {
      // Ignore empty tokens.
      if (!token)
        return;

      // Parse the token.
      var tree;
      if (typeof(token) == 'string')
        tree = this.createParseTree(token);
      else
        tree = token;

      // Obtain the operator token.
      token = tree.value;

      // Set suffix options.
      var options = {};
      options.ifEmpty = token[token.length - 1] == '=';
      if (options.ifEmpty)
        token = token.substring(0, token.length - 1);

      // Process token based on prefix.
      var prefix = token[0];
      token = token.slice(1);

      if (opt_exclude[token])
        return;

      // All possible tokens based on prefix.
      if (prefix == '$') {
        options.annotation = token;
        if (token == 'role') {
          // Non-localized role and state obtained by default.
          this.addToSpannable_(buff, node.role, options);
        } else if (token == 'value') {
          var text = node.attributes.value;
          if (text) {
            var offset = buff.getLength();
            if (node.attributes.textSelStart !== undefined) {
              options.annotation = new Output.SelectionSpan(
                  node.attributes.textSelStart,
                  node.attributes.textSelEnd);
            }
          } else if (node.role == chrome.automation.RoleType.staticText) {
            // TODO(dtseng): Remove once Blink treats staticText values as
            // names.
            text = node.attributes.name;
          }
          this.addToSpannable_(buff, text, options);
        } else if (token == 'indexInParent') {
          this.addToSpannable_(buff, node.indexInParent + 1);
        } else if (token == 'parentChildCount') {
          if (node.parent)
          this.addToSpannable_(buff, node.parent.children.length);
        } else if (token == 'state') {
          Object.getOwnPropertyNames(node.state).forEach(function(s) {
            this.addToSpannable_(buff, s, options);
          }.bind(this));
        } else if (token == 'find') {
          // Find takes two arguments: JSON query string and format string.
          if (tree.firstChild) {
            var jsonQuery = tree.firstChild.value;
            node = node.find(
                /** @type {Object}*/(JSON.parse(jsonQuery)));
            var formatString = tree.firstChild.nextSibling;
            if (node)
              this.format_(node, formatString, buff);
          }
        } else if (token == 'descendants') {
          if (AutomationPredicate.leaf(node))
            return;

          // Construct a range to the leftmost and rightmost leaves.
          var leftmost = AutomationUtil.findNodePre(
              node, Dir.FORWARD, AutomationPredicate.leaf);
          var rightmost = AutomationUtil.findNodePre(
              node, Dir.BACKWARD, AutomationPredicate.leaf);
          if (!leftmost || !rightmost)
            return;

          var subrange = new cursors.Range(
              new cursors.Cursor(leftmost, 0),
              new cursors.Cursor(rightmost, 0));
          this.range_(subrange, null, 'navigate', buff);
        } else if (node.attributes[token]) {
          this.addToSpannable_(buff, node.attributes[token], options);
        } else if (node.state[token]) {
          this.addToSpannable_(buff, token, options);
        } else if (tree.firstChild) {
          // Custom functions.
          if (token == 'if') {
            var cond = tree.firstChild;
            var attrib = cond.value.slice(1);
            if (node.attributes[attrib] || node.state[attrib])
              this.format_(node, cond.nextSibling, buff);
            else
              this.format_(node, cond.nextSibling.nextSibling, buff);
          } else if (token == 'earcon') {
            var contentBuff = new cvox.Spannable();
            if (tree.firstChild.nextSibling)
              this.format_(node, tree.firstChild.nextSibling, contentBuff);
            options.annotation = new Output.Action(function() {
              cvox.ChromeVox.earcons.playEarcon(
                  cvox.AbstractEarcons[tree.firstChild.value]);
            });
            this.addToSpannable_(buff, contentBuff, options);
          }
        }
      } else if (prefix == '@') {
        var msgId = token;
        var msgArgs = [];
        var curMsg = tree.firstChild;

        while (curMsg) {
          var arg = curMsg.value;
          if (arg[0] != '$') {
            console.error('Unexpected value: ' + arg);
            return;
          }
          var msgBuff = new cvox.Spannable();
          this.format_(node, arg, msgBuff);
          msgArgs.push(msgBuff.toString());
          curMsg = curMsg.nextSibling;
        }
          var msg = cvox.ChromeVox.msgs.getMsg(msgId, msgArgs);
        try {
          if (this.formatOptions_.braille)
            msg = cvox.ChromeVox.msgs.getMsg(msgId + '_brl', msgArgs) || msg;
        } catch(e) {}

        if (msg) {
          this.addToSpannable_(buff, msg, options);
        }
      } else if (prefix == '!') {
        this.speechProperties_[token] = true;
      }
    }.bind(this));
  },

  /**
   * @param {!cursors.Range} range
   * @param {cursors.Range} prevRange
   * @param {chrome.automation.EventType|string} type
   * @param {!cvox.Spannable} rangeBuff
   * @private
   */
  range_: function(range, prevRange, type, rangeBuff) {
    if (!prevRange)
      prevRange = cursors.Range.fromNode(range.getStart().getNode().root);

    var cursor = range.getStart();
    var prevNode = prevRange.getStart().getNode();

    var formatNodeAndAncestors = function(node, prevNode) {
      var buff = new cvox.Spannable();
      this.ancestry_(node, prevNode, type, buff);
      this.node_(node, prevNode, type, buff);
      if (this.formatOptions_.location)
        this.locations_.push(node.location);
      return buff;
    }.bind(this);

    while (cursor.getNode() != range.getEnd().getNode()) {
      var node = cursor.getNode();
      this.addToSpannable_(
          rangeBuff, formatNodeAndAncestors(node, prevNode));
      prevNode = node;
      cursor = cursor.move(cursors.Unit.NODE,
                           cursors.Movement.DIRECTIONAL,
                           Dir.FORWARD);
    }
    var lastNode = range.getEnd().getNode();
    this.addToSpannable_(rangeBuff, formatNodeAndAncestors(lastNode, prevNode));
  },

  /**
   * @param {!chrome.automation.AutomationNode} node
   * @param {!chrome.automation.AutomationNode} prevNode
   * @param {chrome.automation.EventType|string} type
   * @param {!cvox.Spannable} buff
   * @param {!Object=} opt_exclude A list of attributes to exclude from
   * processing.
   * @private
   */
  ancestry_: function(node, prevNode, type, buff, opt_exclude) {
    opt_exclude = opt_exclude || {};
    var prevUniqueAncestors =
        AutomationUtil.getUniqueAncestors(node, prevNode);
    var uniqueAncestors = AutomationUtil.getUniqueAncestors(prevNode, node);

    // First, look up the event type's format block.
    // Navigate is the default event.
    var eventBlock = Output.RULES[type] || Output.RULES['navigate'];

    for (var i = 0, formatPrevNode;
         (formatPrevNode = prevUniqueAncestors[i]);
         i++) {
      var roleBlock = eventBlock[formatPrevNode.role] || eventBlock['default'];
      if (roleBlock.leave)
        this.format_(formatPrevNode, roleBlock.leave, buff, opt_exclude);
    }

    var enterOutput = [];
    var enterRole = {};
    for (var j = uniqueAncestors.length - 2, formatNode;
         (formatNode = uniqueAncestors[j]);
         j--) {
      var roleBlock = eventBlock[formatNode.role] || eventBlock['default'];
      if (roleBlock.enter) {
        if (enterRole[formatNode.role])
          continue;
        enterRole[formatNode.role] = true;
        var tempBuff = new cvox.Spannable('');
        this.format_(formatNode, roleBlock.enter, tempBuff, opt_exclude);
        enterOutput.unshift(tempBuff);
      }
    }
    enterOutput.forEach(function(c) {
      this.addToSpannable_(buff, c);
    }.bind(this));

    if (!opt_exclude.stay) {
      var commonFormatNode = uniqueAncestors[0];
      while (commonFormatNode && commonFormatNode.parent) {
        commonFormatNode = commonFormatNode.parent;
        var roleBlock =
            eventBlock[commonFormatNode.role] || eventBlock['default'];
        if (roleBlock.stay)
          this.format_(commonFormatNode, roleBlock.stay, buff, opt_exclude);
      }
    }
  },

  /**
   * @param {!chrome.automation.AutomationNode} node
   * @param {!chrome.automation.AutomationNode} prevNode
   * @param {chrome.automation.EventType|string} type
   * @param {!cvox.Spannable} buff
   * @private
   */
  node_: function(node, prevNode, type, buff) {
    // Navigate is the default event.
    var eventBlock = Output.RULES[type] || Output.RULES['navigate'];
    var roleBlock = eventBlock[node.role] || eventBlock['default'];
    var speakFormat = roleBlock.speak || eventBlock['default'].speak;
    this.format_(node, speakFormat, buff);
  },

  /**
   * @param {!cursors.Range} range
   * @param {cursors.Range} prevRange
   * @param {chrome.automation.EventType|string} type
   * @param {!cvox.Spannable} buff
   * @private
   */
  subNode_: function(range, prevRange, type, buff) {
    if (!prevRange)
      prevRange = range;
    var dir = cursors.Range.getDirection(prevRange, range);
    var prevNode = prevRange.getBound(dir).getNode();
    this.ancestry_(
        range.getStart().getNode(), prevNode, type, buff,
        {stay: true, name: true, value: true});
    var startIndex = range.getStart().getIndex();
    var endIndex = range.getEnd().getIndex();
    if (startIndex === endIndex)
      endIndex++;
    this.addToSpannable_(
        buff, range.getStart().getText().substring(startIndex, endIndex));
  },

  /**
   * Adds to the given buffer with proper delimiters added.
   * @param {!cvox.Spannable} spannable
   * @param {string|!cvox.Spannable} value
   * @param {{ifEmpty: boolean,
   *      annotation: (string|Output.Action|undefined)}=} opt_options
   */
  addToSpannable_: function(spannable, value, opt_options) {
    opt_options = opt_options || {ifEmpty: false, annotation: undefined};
    if ((!value || value.length == 0) && !opt_options.annotation)
      return;

    var spannableToAdd = new cvox.Spannable(value, opt_options.annotation);
    if (spannable.getLength() == 0) {
      spannable.append(spannableToAdd);
      return;
    }

    if (opt_options.ifEmpty &&
        opt_options.annotation &&
        (spannable.getSpanStart(opt_options.annotation) != undefined ||
            spannable.getSpanStart(
                Output.ATTRIBUTE_ALIAS[opt_options.annotation]) != undefined))
      return;

    var prefixed = new cvox.Spannable(Output.SPACE);
    prefixed.append(spannableToAdd);
    spannable.append(prefixed);
  },

  /**
   * Parses the token containing a custom function and returns a tree.
   * @param {string} inputStr
   * @return {Object}
   */
  createParseTree: function(inputStr) {
    var root = {value: ''};
    var currentNode = root;
    var index = 0;
    var braceNesting = 0;
    while (index < inputStr.length) {
      if (inputStr[index] == '(') {
        currentNode.firstChild = {value: ''};
        currentNode.firstChild.parent = currentNode;
        currentNode = currentNode.firstChild;
      } else if (inputStr[index] == ')') {
        currentNode = currentNode.parent;
      } else if (inputStr[index] == '{') {
        braceNesting++;
        currentNode.value += inputStr[index];
      } else if (inputStr[index] == '}') {
        braceNesting--;
        currentNode.value += inputStr[index];
      } else if (inputStr[index] == ',' && braceNesting === 0) {
        currentNode.nextSibling = {value: ''};
        currentNode.nextSibling.parent = currentNode.parent;
        currentNode = currentNode.nextSibling;
      } else {
        currentNode.value += inputStr[index];
      }
      index++;
    }

    if (currentNode != root)
      throw 'Unbalanced parenthesis.';

    return root;
  }
};

});  // goog.scope
