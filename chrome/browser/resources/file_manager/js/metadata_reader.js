// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var metadataReader = {
  verbose: false,

  parsers: [],

  registerParser : function(parser) {
    this.parsers.push(parser);
  },

  messageHandlers: {
    "init": function() {
      // Combine url filters from all parsers and build a single regexp.
      var sources = [];
      for (var i = 0; i != this.parsers.length; i++) {
        sources.push(this.parsers[i].urlFilter.source);
      }
      var regexp = new RegExp('(' + sources.join('|') + ')', 'i');
      postMessage({verb: 'filter', arguments: [regexp]});
      this.log('initialized with URL filter ' + regexp);
    },

    "request": function(fileURL) {
      this.processOneFile(fileURL, function callback(metadata) {
          postMessage({verb: 'result', arguments: [fileURL, metadata]});
      });
    }
  },

  onMessage: function(event) {
    var data = event.data;

    if (this.messageHandlers.hasOwnProperty(data.verb)) {
      this.messageHandlers[data.verb].apply(this, data.arguments);
    } else {
      this.log('Unknown message from client: ' + data.verb, data);
    }
  },

  error: function(var_args) {
    var ary = Array.apply(null, arguments);
    postMessage({verb: 'error', arguments: ary});
  },

  log: function(var_args) {
    var ary = Array.apply(null, arguments);
    postMessage({verb: 'log', arguments: ary});
  },

  vlog: function(var_args) {
    if (this.verbose)
      this.log.apply(this, arguments);
  },

  processOneFile: function(fileURL, callback) {
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
        for (var i = 0; i != self.parsers.length; i++) {
          var parser = self.parsers[i];
          if (fileURL.match(parser.urlFilter)) {
            nextStep(parser.parse);
            return;
          }
        }
        onError('unsupported format');
      },

      // Step two, turn the url into an entry.
      function getEntry(parseFunc) {
        webkitResolveLocalFileSystemURL(
            fileURL,
            function(entry) { nextStep(entry, parseFunc) },
            onError);
      },

      // Step three, turn the entry into a file.
      function getFile(entry, parseFunc) {
        entry.file(function(file) { nextStep(file, parseFunc) }, onError);
      },

      // Step four, parse the file content.
      function parseContent(file, parseFunc) {
        parseFunc(file, callback, onError, self);
      }
    ];

    nextStep();
  }
};

var onmessage = metadataReader.onMessage.bind(metadataReader);

importScripts("metadata_util.js");
importScripts("exif_reader.js");
importScripts("mpeg_reader.js");