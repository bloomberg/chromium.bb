/*
 * Copyright (c) 2013 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

'use strict';

lib.rtdep('lib.f',
          'hterm');

// CSP means that we can't kick off the initialization from the html file,
// so we do it like this instead.
window.onload = function() {
  lib.init(function() {
    NaClTerm.init();
  });
};

/**
 * The hterm-powered terminal command.
 *
 * This class defines a command that can be run in an hterm.Terminal instance.
 *
 * @param {Object} argv The argument object passed in from the Terminal.
 */
function NaClTerm(argv) {
  this.argv_ = argv;
  this.io = null;
};

var embed;

/**
 * Static initialier called from index.html.
 *
 * This constructs a new Terminal instance and instructs it to run the NaClTerm
 * command.
 */
NaClTerm.init = function() {
  var profileName = lib.f.parseQuery(document.location.search)['profile'];
  var terminal = new hterm.Terminal(profileName);
  terminal.decorate(document.querySelector('#terminal'));

  // Useful for console debugging.
  window.term_ = terminal;

  // We don't properly support the hterm bell sound, so we need to disable it.
  terminal.prefs_.definePreference('audible-bell-sound', '');

  terminal.setAutoCarriageReturn(true);
  terminal.setCursorPosition(0, 0);
  terminal.setCursorVisible(true);
  terminal.runCommandClass(NaClTerm, document.location.hash.substr(1));

  return true;
};

/**
 * Handle messages sent to us from NaCl.
 *
 * @private
 */
NaClTerm.prototype.handleMessage_ = function(e) {
  if (e.data.indexOf(NaClTerm.prefix) != 0) return;
  var msg = e.data.substring(NaClTerm.prefix.length);
  if (!this.loaded) {
    this.bufferedOutput += msg;
  } else {
    term_.io.print(msg);
  }
}

/**
 * Handle load error event from NaCl.
 */
NaClTerm.prototype.handleLoadAbort_ = function(e) {
  term_.io.print("Load aborted.\n");
}

/**
 * Handle load abort event from NaCl.
 */
NaClTerm.prototype.handleLoadError_ = function(e) {
  term_.io.print(embed.lastError + '\n');
}

/**
 * Handle load end event from NaCl.
 */
NaClTerm.prototype.handleLoad_ = function(e) {
  if (typeof(this.lastUrl) != 'undefined')
    term_.io.print("\n");
  term_.io.print("Loaded.\n");

  // Now that have completed loading and displaying
  // loading messages we output any messages from the
  // NaCl module that were buffered up unto this point
  this.loaded = true;
  term_.io.print(this.bufferedOutput);
  this.bufferedOutput = ''
}

/**
 * Handle load progress event from NaCl.
 */
NaClTerm.prototype.handleProgress_ = function(e) {
  var url = e.url.substring(e.url.lastIndexOf('/') + 1);
  if (this.lastUrl != url) {
    if (url != '') {
      if (this.lastUrl)
        term_.io.print("\n");
      term_.io.print("Loading " + url + " .");
    }
  } else {
    term_.io.print(".");
  }
  if (url)
  this.lastUrl = url;
}

/**
 * Handle crash event from NaCl.
 */
NaClTerm.prototype.handleCrash_ = function(e) {
 if (embed.exitStatus == -1) {
   term_.io.print("Program crashed (exit status -1)\n")
 } else {
   term_.io.print("Program exited (status=" + embed.exitStatus + ")\n");
 }
}


NaClTerm.prototype.onTerminalResize_ = function() {
  var width = term_.io.terminal_.screenSize.width;
  var height = term_.io.terminal_.screenSize.height;
  embed.postMessage({'tty_resize': [ width, height ]});
}

NaClTerm.prototype.onVTKeystroke_ = function(str) {
  var message = {};
  message[NaClTerm.prefix] = str;
  embed.postMessage(message);
}

/*
 * This is invoked by the terminal as a result of terminal.runCommandClass().
 */
NaClTerm.prototype.run = function() {
  this.io = this.argv_.io.push();
  this.bufferedOutput = '';
  this.loaded = false;

  embed = document.createElement('object');
  embed.width = 0;
  embed.height = 0;
  embed.data = NaClTerm.nmf;
  embed.type = 'application/x-pnacl';
  embed.addEventListener('message', this.handleMessage_.bind(this));
  embed.addEventListener('progress', this.handleProgress_.bind(this));
  embed.addEventListener('load', this.handleLoad_.bind(this));
  embed.addEventListener('error', this.handleLoadError_.bind(this));
  embed.addEventListener('abort', this.handleLoadAbort_.bind(this));
  embed.addEventListener('crash', this.handleCrash_.bind(this));

  function addParam(name, value) {
    var param = document.createElement('param');
    param.name = name;
    param.value = value;
    embed.appendChild(param);
  }

  addParam('ps_tty_prefix', NaClTerm.prefix);
  addParam('ps_tty_resize', 'tty_resize');
  addParam('ps_stdin', '/dev/tty');
  addParam('ps_stdout', '/dev/tty');
  addParam('ps_stderr', '/dev/tty');
  addParam('ps_verbosity', '2');
  addParam('TERM', 'xterm-256color');

  var args = lib.f.parseQuery(document.location.search);
  var argn = 1;
  while (true) {
    var argname = 'arg' + argn;
    var arg = args[argname];
    if (typeof(arg) === 'undefined')
      break;
    addParam(argname, arg);
    argn = argn + 1;
  }

  term_.io.print("Loading NaCl module.\n")
  document.body.appendChild(embed);

  this.io.onVTKeystroke = this.onVTKeystroke_.bind(this);
  this.io.onTerminalResize = this.onTerminalResize_.bind(this);
};
