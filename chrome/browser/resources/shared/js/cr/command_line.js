// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview CommandLine class, parses out individual options from a
 * command line string.
 *
 * This file depends on chrome.commandLineString, which is only set if your
 * Web UI explicitly sets it.  The Web UI based options dialog does this from
 * OptionsUI::RenderViewCreated, in options_ui.cc.
 */

cr.define('cr', function() {
  /**
   * Class to reperesent command line options passed to chrome.
   *
   * Instances of this class will have the following properties:
   *   executable: The name of the executable used to start chrome
   *
   *   options: An object containing the named arguments.  If the argument
   *   was assigned a value, such as --foo=bar, then options['--foo'] will be
   *   set to 'bar'.  If the argument was not assigned a value, such as
   *   --enable-foo, then options['--enable-foo'] will be set to true.
   *
   *   looseArguments: An array of arguments that were not associated with
   *   argument names.
   *
   * Note that the Chromium code that computes the command line string
   * has a bug that strips quotes from command lines, so you can't really
   * trust looseArguments or any argument that might contain spaces until
   * http://code.google.com/p/chromium/issues/detail?id=56684 is fixed.
   *
   * @param {string} commandLineString The command line string to parse.
   */
  function CommandLine(commandLineString) {
    this.commandLineString_ = commandLineString;
    this.parseOptions_(commandLineString.split(/\s+/));
  }

  /**
   * Return the command line as a single string.
   */
  CommandLine.prototype.toString = function() {
    return this.commandLineString_;
  };

  /**
   * Parse the array of command line options into this.executable, this.options,
   * and this.looseArguments.
   *
   * @param {Array} ary The list of command line arguments.  The first argument
   *     must be the executable name.  Named command line arguments must start
   *     with two dashes, and may optionally be assigned a value as in
   *     --argument-name=value.
   */
  CommandLine.prototype.parseOptions_ = function(ary) {
    this.executable = ary.shift();
    this.options = {};
    this.looseArguments = [];

    for (var i = 0; i < ary.length; i++) {
      var arg = ary[i];

      if (arg.substr(0, 2) == '--') {
        var pos = arg.indexOf('=');
        if (pos > 0) {
          // Argument has a value: --argument-name=value
          this.options[arg.substr(0, pos)] = arg.substr(pos + 1);
        } else {
          // Argument is a flag: --some-flag
          this.options[arg] = true;
        }
      } else {
        // Argument doesn't start with '--'.
        this.looseArguments.push(arg);
      }
    }
  };

  var commandLine = null;
  if (chrome && chrome.commandLineString) {
    commandLine = new CommandLine(chrome.commandLineString);
  } else {
    console.warn('chrome.commandLineString is not present.  Not initializing ' +
                 'cr.commandLine');
  }

  return {
    CommandLine: CommandLine,
    commandLine: commandLine
  };
});
