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
goog.require('cvox.Spannable');

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
 * @param {!cursors.Range} range
 * @param {cursors.Range} prevRange
 * @param {chrome.automation.EventType|Output.EventType} type
 * @constructor
 */
Output = function(range, prevRange, type) {
  // TODO(dtseng): Include brailleBuffer once we add rules for braille.
  /** @type {!cvox.Spannable} */
  this.buffer_ = new cvox.Spannable();
  this.locations_ = [];

  this.render_(range, prevRange, type);
  this.handleSpeech();
  this.handleBraille();
  this.handleDisplay();
};

/**
 * Delimiter to use between output values.
 * @type {string}
 */
Output.SPACE = ' ';

/**
 * Rules specifying format of AutomationNodes for output.
 * @type {!Object.<string, Object.<string, Object.<string, string>>>}
 */
Output.RULES = {
  navigate: {
    'default': {
      speak: '$name $role $value',
      braille: ''
    },
    button: {
      speak: '$name $earcon(BUTTON, @tag_button)'
    },
    checkBox: {
      speak: '$or($checked, @describe_checkbox_checked($name), ' +
          '@describe_checkbox_unchecked($name)) ' +
          '$or($checked, ' +
              '$earcon(CHECK_ON, @input_type_checkbox), ' +
              '$earcon(CHECK_OFF, @input_type_checkbox))'
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
    paragraph: {
      speak: '$value'
    },
    staticText: {
      speak: '$value'
    },
    window: {
      enter: '$name $role= $earcon(OBJECT_OPEN)'
      // TODO(dtseng): A leave event is not reliable because views does not fire
      // focus events after closing windows.
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
  }
};

/**
 * Alias equivalent attributes.
 * @type {!Object.<string, string>}
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
   * Handle output to speech.
   */
  handleSpeech: function() {
    var buff = this.buffer_;
    cvox.ChromeVox.tts.speak(buff.toString(), cvox.QueueMode.FLUSH);
    var actions = buff.getSpansInstanceOf(
        /** @type {function()} */(new Output.Action(
            function() {}).constructor));

    if (actions) {
      actions.forEach(function(a) {
        a.run();
      });
    }
  },

  /**
   * Handles output to braille.
   */
  handleBraille: function() {
    cvox.ChromeVox.braille.write(
        cvox.NavBraille.fromText(this.buffer_.toString()));
  },

  /**
   * Handles output to visual display.
   */
  handleDisplay: function() {
    chrome.accessibilityPrivate.setFocusRing(this.locations_);
  },

  /**
   * Renders the given range using optional context previous range and event
   * type.
   * @param {!cursors.Range} range
   * @param {cursors.Range} prevRange
   * @param {chrome.automation.EventType|string} type
   * @private
   */
  render_: function(range, prevRange, type) {
    var buff = new cvox.Spannable();
    if (range.isSubNode())
      this.subNode_(range, prevRange, type, buff);
    else
      this.range_(range, prevRange, type, buff);

    this.buffer_ = buff;
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
      format = format.replace(/,\W/g, ',');
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
        } else if (token == 'state') {
          Object.getOwnPropertyNames(node.state).forEach(function(s) {
            this.addToSpannable_(buff, s, options);
          }.bind(this));
        } else if (node.attributes[token]) {
          this.addToSpannable_(buff, node.attributes[token], options);
        } else if (node.state[token]) {
          this.addToSpannable_(buff, token, options);
        } else if (tree.firstChild) {
          // Custom functions.
          if (token == 'or') {
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
          arg = arg.slice(1);
          if (!node.attributes[arg])
            msgArgs.push('');
          else
            msgArgs.push(node.attributes[arg]);
          curMsg = curMsg.nextSibling;
        }

        var msg = cvox.ChromeVox.msgs.getMsg(msgId, msgArgs);
        if (msg) {
          this.addToSpannable_(buff, msg, options);
        }
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
      prevRange = range;

    var cursor = range.getStart();
    var prevNode = prevRange.getStart().getNode();

    var formatNodeAndAncestors = function(node, prevNode) {
      var buff = new cvox.Spannable();
      this.ancestry_(node, prevNode, type, buff);
      this.node_(node, prevNode, type, buff);
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

    for (var j = 0, formatNode;
         (formatNode = uniqueAncestors[j]) && formatNode !== node;
         j++) {
      var roleBlock = eventBlock[formatNode.role] || eventBlock['default'];
      if (roleBlock.enter)
        this.format_(formatNode, roleBlock.enter, buff, opt_exclude);
    }

    if (!opt_exclude.stay) {
      var commonFormatNode = uniqueAncestors[0];
      while (commonFormatNode && commonFormatNode.parent()) {
        commonFormatNode = commonFormatNode.parent();
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
    if (value.length == 0 && !opt_options.annotation)
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
    while (index < inputStr.length) {
      if (inputStr[index] == '(') {
        currentNode.firstChild = {value: ''};
        currentNode.firstChild.parent = currentNode;
        currentNode = currentNode.firstChild;
      } else if (inputStr[index] == ')') {
        currentNode = currentNode.parent;
      } else if (inputStr[index] == ',') {
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
