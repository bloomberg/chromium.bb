// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Webworker spec says that the worker global object is called self.  That's
// a terrible name since we use it all over the chrome codebase to capture
// the 'this' keyword in lambdas.
var global = self;

// All of these scripts could be imported with a single call to importScripts,
// but then load and compile time errors would all be reported from the same
// line.
importScripts('metadata_parser.js');
importScripts('byte_reader.js');

/**
 * Dispatches metadata requests to the correct parser.
 */
function MetadataDispatcher() {
  importScripts('exif_parser.js');
  importScripts('mpeg_parser.js');
  importScripts('id3_parser.js');

  var patterns = [];

  for (var i = 0; i < MetadataDispatcher.parserClasses_.length; i++) {
    var parserClass = MetadataDispatcher.parserClasses_[i];
    this.log('new parser: ' + parserClass.parserType);
    var parser = new parserClass(this);
    this.parserInstances_.push(parser);
    patterns.push(parser.urlFilter.source);
  }

  this.parserRegexp_ = new RegExp('(' + patterns.join('|') + ')', 'i');
}

MetadataDispatcher.parserClasses_ = [];

MetadataDispatcher.registerParserClass = function(parserClass) {
  MetadataDispatcher.parserClasses_.push(parserClass);
};

MetadataDispatcher.prototype.parserInstances_ = [];

/**
 * Verbose logging for the dispatcher.
 *
 * Individual parsers also take this as their default verbosity setting.
 */
MetadataDispatcher.prototype.verbose = false;

MetadataDispatcher.prototype.messageHandlers = {
  init: function() {
    // Inform our owner that we're done initializing.
    // If we need to pass more data back, we can add it to the param array.
    this.postMessage('initialized', [this.parserRegexp_]);
    this.log('initialized with URL filter ' + this.parserRegexp_);
  },

  request: function(fileURL) {
    var self = this;

    try {
      this.processOneFile(fileURL, function callback(metadata) {
          self.postMessage('result', [fileURL, metadata]);
      });
    } catch (ex) {
      this.error(ex);
    }
  }
};

/**
 * Indicate to the caller that an operation has failed.
 *
 * No other messages relating to the failed operation should be sent.
 */
MetadataDispatcher.prototype.error = function(var_args) {
  var ary = Array.apply(null, arguments);
  this.postMessage('error', ary);
};

/**
 * Send a log message to the caller.
 *
 * Callers must not parse log messages for control flow.
 */
MetadataDispatcher.prototype.log = function(var_args) {
  var ary = Array.apply(null, arguments);
  this.postMessage('log', ary);
};

/**
 * Send a log message to the caller only if this.verbose is true.
 */
MetadataDispatcher.prototype.vlog = function(var_args) {
  if (this.verbose)
    this.log.apply(this, arguments);
};

/**
 * Post a properly formatted message to the caller.
 */
MetadataDispatcher.prototype.postMessage = function(verb, arguments) {
  global.postMessage({verb: verb, arguments: arguments});
};

MetadataDispatcher.prototype.onMessage = function(event) {
  var data = event.data;

  if (this.messageHandlers.hasOwnProperty(data.verb)) {
    this.messageHandlers[data.verb].apply(this, data.arguments);
  } else {
    this.log('Unknown message from client: ' + data.verb, data);
  }
};

MetadataDispatcher.prototype.processOneFile = function(fileURL, callback) {
  var self = this;
  var currentStep = -1;

  function nextStep(var_args) {
    self.vlog('nextStep: ' + steps[currentStep + 1].name);
    steps[++currentStep].apply(self, arguments);
  }

  function onError(err, stepName) {
    self.error(fileURL, stepName || steps[currentStep].name, err);
  }

  var steps =
  [ // Step one, find the parser matching the url.
    function detectFormat() {
      for (var i = 0; i != self.parserInstances_.length; i++) {
        var parser = self.parserInstances_[i];
        if (fileURL.match(parser.urlFilter)) {
          nextStep(parser);
          return;
        }
      }
      onError('unsupported format');
    },

    // Step two, turn the url into an entry.
    function getEntry(parser) {
      webkitResolveLocalFileSystemURL(
          fileURL,
          function(entry) { nextStep(entry, parser) },
          onError);
    },

    // Step three, turn the entry into a file.
    function getFile(entry, parser) {
      entry.file(function(file) { nextStep(file, parser) }, onError);
    },

    // Step four, parse the file content.
    function parseContent(file, parser) {
      parser.parse(file, callback, onError);
    }
  ];

  nextStep();
};

var dispatcher = new MetadataDispatcher();
global.onmessage = dispatcher.onMessage.bind(dispatcher);
