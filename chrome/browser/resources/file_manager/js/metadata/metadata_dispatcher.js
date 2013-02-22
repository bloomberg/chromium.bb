// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// All of these scripts could be imported with a single call to importScripts,
// but then load and compile time errors would all be reported from the same
// line.
importScripts('metadata_parser.js');
importScripts('byte_reader.js');
importScripts('../util.js');

/**
 * Dispatches metadata requests to the correct parser.
 *
 * @param {Object} port Worker port.
 * @constructor
 */
function MetadataDispatcher(port) {
  this.port_ = port;
  this.port_.onmessage = this.onMessage.bind(this);

  // Make sure to update component_extension_resources.grd
  // when adding new parsers.
  importScripts('exif_parser.js');
  importScripts('image_parsers.js');
  importScripts('mpeg_parser.js');
  importScripts('id3_parser.js');

  var patterns = [];

  this.parserInstances_ = [];
  for (var i = 0; i < MetadataDispatcher.parserClasses_.length; i++) {
    var parserClass = MetadataDispatcher.parserClasses_[i];
    var parser = new parserClass(this);
    this.parserInstances_.push(parser);
    patterns.push(parser.urlFilter.source);
  }

  this.parserRegexp_ = new RegExp('(' + patterns.join('|') + ')', 'i');

  this.messageHandlers_ = {
    init: this.init_.bind(this),
    request: this.request_.bind(this)
  };
}

/**
 * List of registered parser classes.
 * @private
 */
MetadataDispatcher.parserClasses_ = [];

/**
 * @param {function} parserClass Parser constructor function.
 */
MetadataDispatcher.registerParserClass = function(parserClass) {
  MetadataDispatcher.parserClasses_.push(parserClass);
};

/**
 * Verbose logging for the dispatcher.
 *
 * Individual parsers also take this as their default verbosity setting.
 */
MetadataDispatcher.prototype.verbose = false;

/**
 * |init| message handler.
 * @private
 */
MetadataDispatcher.prototype.init_ = function() {
  // Inform our owner that we're done initializing.
  // If we need to pass more data back, we can add it to the param array.
  this.postMessage('initialized', [this.parserRegexp_]);
  this.log('initialized with URL filter ' + this.parserRegexp_);
};

/**
 * |request| message handler.
 * @param {string} fileURL File URL.
 * @private
 */
MetadataDispatcher.prototype.request_ = function(fileURL) {
  try {
    this.processOneFile(fileURL, function callback(metadata) {
        this.postMessage('result', [fileURL, metadata]);
    }.bind(this));
  } catch (ex) {
    this.error(fileURL, ex);
  }
};

/**
 * Indicate to the caller that an operation has failed.
 *
 * No other messages relating to the failed operation should be sent.
 * @param {...Object} var_args Arguments.
 */
MetadataDispatcher.prototype.error = function(var_args) {
  var ary = Array.apply(null, arguments);
  this.postMessage('error', ary);
};

/**
 * Send a log message to the caller.
 *
 * Callers must not parse log messages for control flow.
 * @param {...Object} var_args Arguments.
 */
MetadataDispatcher.prototype.log = function(var_args) {
  var ary = Array.apply(null, arguments);
  this.postMessage('log', ary);
};

/**
 * Send a log message to the caller only if this.verbose is true.
 * @param {...Object} var_args Arguments.
 */
MetadataDispatcher.prototype.vlog = function(var_args) {
  if (this.verbose)
    this.log.apply(this, arguments);
};

/**
 * Post a properly formatted message to the caller.
 * @param {string} verb Message type descriptor.
 * @param {Array.<Object>} args Arguments array.
 */
MetadataDispatcher.prototype.postMessage = function(verb, args) {
  this.port_.postMessage({verb: verb, arguments: args});
};

/**
 * Message handler.
 * @param {Event} event Event object.
 */
MetadataDispatcher.prototype.onMessage = function(event) {
  var data = event.data;

  if (this.messageHandlers_.hasOwnProperty(data.verb)) {
    this.messageHandlers_[data.verb].apply(this, data.arguments);
  } else {
    this.log('Unknown message from client: ' + data.verb, data);
  }
};

/**
 * @param {string} fileURL File URL.
 * @param {function(Object)} callback Completion callback.
 */
MetadataDispatcher.prototype.processOneFile = function(fileURL, callback) {
  var self = this;
  var currentStep = -1;

  function nextStep(var_args) {
    self.vlog('nextStep: ' + steps[currentStep + 1].name);
    steps[++currentStep].apply(self, arguments);
  }

  var metadata;

  function onError(err, stepName) {
    self.error(fileURL, stepName || steps[currentStep].name, err.toString(),
        metadata);
  }

  var steps =
  [ // Step one, find the parser matching the url.
    function detectFormat() {
      for (var i = 0; i != self.parserInstances_.length; i++) {
        var parser = self.parserInstances_[i];
        if (fileURL.match(parser.urlFilter)) {
          // Create the metadata object as early as possible so that we can
          // pass it with the error message.
          metadata = parser.createDefaultMetadata();
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
      metadata.fileSize = file.size;
      try {
        parser.parse(file, metadata, callback, onError);
      } catch (e) {
        onError(e.stack);
      }
    }
  ];

  nextStep();
};

// Webworker spec says that the worker global object is called self.  That's
// a terrible name since we use it all over the chrome codebase to capture
// the 'this' keyword in lambdas.
var global = self;

if (global.constructor.name == 'SharedWorkerContext') {
  global.addEventListener('connect', function(e) {
    var port = e.ports[0];
    new MetadataDispatcher(port);
    port.start();
  });
} else {
  // Non-shared worker.
  new MetadataDispatcher(global);
}
