// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview JavaScript code-coverage utility.
 * This module powers a FireFox extension that gathers code coverage results.
 * It depends on the Venkman JavaScript Debugger extension.
 * <p>
 * To enable coverage tracking, set the JS_COVERAGE environment variable to 1.
 * Then, optionally, set the JS_COVERAGE_FILE environment variable to the full
 * path where the results LCOV data should be stored. The default path is
 * "/tmp/jscov.dat".  Finally, set the JS_COVERAGE_URL_PATTERN environment
 * variable to a regex of the files you want to instrument.  The default pattern
 * is "^http://.*js".
 * <p>
 * The coverage seems to be a little flaky, in the sense that it does not
 * always seem to catch all the javascript files being loaded into Firefox.
 * From what I have observed, it seems that the files of extensions might be
 * cached, and when loaded from this cache the onScriptCreated() callback is
 * not always called.  If I save any one file in the extension, then all files
 * of that extension are caught correctly.  Still investigating.
 */


/** Namespace for everything in this extension. */
var LCOV = {};


/**
 * This helper method returns a given class based on a class name from the
 * global Components repository.
 * @param {String} className: The name of the class to return
 * @return {Object}: The requested class (not an instance)
 */
LCOV.CC = function(className) {
  return Components.classes[className];
};


/**
 * This helper method returns a given interface based on an interface name
 * from the global Components repository.
 * @param {String} interfaceName The name of the interface to return
 * @return {Object} The requested interface
 */
LCOV.CI = function(interfaceName) {
  return Components.interfaces[interfaceName];
};


/** Debugger service component class */
LCOV.debuggerService = LCOV.CC('@mozilla.org/js/jsd/debugger-service;1');


/** Environment component class */
LCOV.environment = LCOV.CC("@mozilla.org/process/environment;1");


/** Local file component class */
LCOV.localFile = LCOV.CC('@mozilla.org/file/local;1');


/** File output stream component class */
LCOV.fileOutputStream = LCOV.CC('@mozilla.org/network/file-output-stream;1');


/** Debugger service component interface */
LCOV.jsdIDebuggerService = LCOV.CI('jsdIDebuggerService');


/** Script component interface */
LCOV.jsdIScript = LCOV.CI('jsdIScript');


/** Execution hook component interface */
LCOV.jsdIExecutionHook = LCOV.CI("jsdIExecutionHook");


/** Environment component interface */
LCOV.nsIEnvironment = LCOV.CI('nsIEnvironment');


/** Local file component interface */
LCOV.nsILocalFile = LCOV.CI('nsILocalFile');


/** File output stream component interface */
LCOV.nsIFileOutputStream = LCOV.CI('nsIFileOutputStream');


/** Log an informational message to the Firefox error console. */
LCOV.logInfo = function(msg) {
  Application.console.log('LCOV: ' + msg);
};


/** Log a error message to the Firefox error console. */
LCOV.logError = function(msg) {
  Components.utils.reportError('LCOV: *** ' + msg);
};


/**
 * Useful function for dumping the properties of an object, if for some reason
 * the JSON encoding does not work.
 * @param {string} message String to prepend to dumped message.
 * @param {Object} o The object to dump.
 */
LCOV.dumpit = function(message, o) {
  var buf = 'dumpit:' + message + ': ';

  if (o && o.wrappedJSObject)
    o = o.wrappedJSObject;

  if (o) {
    for (pn in o) {
      try {
        if (o[pn] && typeof o[pn] != 'function')
          buf += '  ' + pn + '=' + o[pn];
      } catch (ex) {
        buf += '  ' + pn + ':ex=' + ex;
      }
    }
  } else {
    buf += 'object is null or not defined';
  }

  LCOV.logInfo(buf);
};


/**
 * The coverage class is responsible for creating and setting up a JavaScript
 * debugger.  It then tracks every line of javascript executed so that coverage
 * data can be reported after a test run.
 * @constructor
 */
LCOV.CoverageTracker = function() {

  /**
   * The Venkman JavaScript Debugger service
   * @type jsdIDebuggerService
   * @private
   */
  this.javascriptDebugger_ = LCOV.debuggerService.getService(
      LCOV.jsdIDebuggerService);
  if (!this.javascriptDebugger_) {
    LCOV.logError('Coult not get debugger service.');
  }

  /**
   * Whether coverage is enabled.
   * @type boolean
   * @private
   */
  this.isCoverageEnabled_ = this.getIsCoverageEnabled_();

  if (this.isCoverageEnabled() && this.javascriptDebugger_) {
    LCOV.logInfo('is enabled');

    var pattern = this.getUrlPattern_();
    LCOV.logInfo('Will instrument URLs macthing: ' + pattern);

    /**
     * Regular expression for matching page URLs to instrument.
     * @type RegExp
     * @private
     */
    this.reUrlPattern_ = new RegExp(pattern, 'i');

    this.startCoverage_();
  } else {
    LCOV.logInfo('not enabled');
  }
};


/**
 * Environment variable name for whether coverage is requested
 * @type String
 * @private
 */
LCOV.CoverageTracker.JS_COVERAGE_ENV_ = 'JS_COVERAGE';


/**
 * Environment variable name for the output file path
 * @type String
 * @private
 */
LCOV.CoverageTracker.JS_COVERAGE_FILE_ENV_ = 'JS_COVERAGE_FILE';


/**
 * Environment variable name for regular expression used for matching page
 * URLs to instrument
 * @type String
 * @private
 */
LCOV.CoverageTracker.JS_COVERAGE_URL_PATTERN_ENV_ = 'JS_COVERAGE_URL_PATTERN';


/**
 * Default output file path
 * @type String
 * @private
 */
LCOV.CoverageTracker.DEFAULT_FILE_ = '/tmp/jscov.dat';


/**
 * Interval between flushes in milliseconds
 * @type Number
 * @private
 */
LCOV.CoverageTracker.FLUSH_INTERVAL_MS_ = 2000;


/**
 * Maximum number of lines stored at a time before a flush is triggered
 * @type Number
 * @private
 */
LCOV.CoverageTracker.HITS_SIZE_THRESHOLD_ = 10000;


/**
 * Singleton instance of the coverage tracker
 * @type LCOV.CoverageTracker
 * @private
 */
LCOV.CoverageTracker.instance_ = null;


/**
 * All the recorded line hit data
 * @type Object
 * @private
 */
LCOV.CoverageTracker.prototype.hits_ = null;


/**
 * Number of lines stored in the {@code hits_} field
 * @type Number
 * @private
 */
LCOV.CoverageTracker.prototype.hitsSize_ = 0;


/**
 * Handle to an event for flushing the data regularly
 * @type Number
 * @private
 */
LCOV.CoverageTracker.prototype.flushEvent_ = null;


/**
 * Output file stream
 * @type nsIFileOutputStream
 * @private
 */
LCOV.CoverageTracker.prototype.stream_ = null;


/**
 * Gets a singleton instance of the coverage tracker. This method is
 * idempotent.
 * @return {LCOV.CoverageTracker} coverage tracker instance
 */
LCOV.CoverageTracker.getInstance = function() {
  if (!LCOV.CoverageTracker.instance_) {
    LCOV.CoverageTracker.instance_ = new LCOV.CoverageTracker();
  }
  return LCOV.CoverageTracker.instance_;
};


/**
 * Returns a cached value for whether or not coverage is enabled for this test
 * run.
 * @return {boolean} Whether or not coverage is enabled
 */
LCOV.CoverageTracker.prototype.isCoverageEnabled = function() {
  return this.isCoverageEnabled_;
};


/**
 * Set of scripts that have been been created.  This is really only used for
 * logging and debuggin purposes.
 * @private
 */
LCOV.scripts_ = {};


/**
 * Setup method that gets invoked from the extension's overlay.  This must
 * be called once from each top-level Firefox window.
 */
LCOV.CoverageTracker.prototype.setupCurrentWindow = function() {
  if (this.isCoverageEnabled()) {
    // The script hooks have to be registered in each top-level Firefox window.
    var coverageTracker = this;
    this.javascriptDebugger_.scriptHook = {
      'onScriptCreated':
          function(script) {
            // This function seems to be called once for each javascript
            // function defined in the source file.  The script will also
            // be destroyed once when each function returns.
            coverageTracker.instrumentScript_(script);
          },
      'onScriptDestroyed':
          function(script) {
            if (typeof LVOC != 'undefined') {
              var fileName = script.fileName.toString();
              LCOV.scripts_[fileName] -= 1;
              if (0 == LCOV.scripts_[fileName]) {
                //LCOV.logInfo('onScriptDestroyed ' + fileName);
                delete LCOV.scripts_[fileName];
              }
            }
            coverageTracker.flushHits_();
          }
    };
  }
};


/**
 * Disposes of the object. Note that the singleton currently lives for as long
 * as the browser is open, so there are no callers of this method.
 */
LCOV.CoverageTracker.prototype.dispose = function() {
  this.endCoverage_();
};


/**
 * Begin the coverage cycle.  First, clear out any existing coverage data.
 * Second, attach to the interrupt hook within the javascript debugger.  Please
 * note that this is a very expensive operation so as few scripts as possible
 * should be evaluated.
 * @private
 */
LCOV.CoverageTracker.prototype.startCoverage_ = function() {
  LCOV.logInfo('starting');
  this.openOutputStream_();
  this.clearCoverageData_();

  var coverageTracker = this;

  this.javascriptDebugger_.on();

  // Enum all scripts there were loaded prior to the debugger turned on.
  this.javascriptDebugger_.enumerateScripts({
    enumerateScript: function(script) {
      //var fileName = script.fileName.toString();
      //LCOV.logInfo('Script already loaded: ' + fileName);
      coverageTracker.instrumentScript_(script);
  }});

  var flags = LCOV.jsdIDebuggerService.DISABLE_OBJECT_TRACE |
      LCOV.jsdIDebuggerService.DEBUG_WHEN_SET |
      LCOV.jsdIDebuggerService.MASK_TOP_FRAME_ONLY;
  //flags |= this.javascriptDebugger_.flags;
  this.javascriptDebugger_.flags |= flags;

  this.javascriptDebugger_.interruptHook = {
    'onExecute':
        function(frame, type, val) {
          coverageTracker.recordExecution_(frame);
          return LCOV.jsdIExecutionHook.RETURN_CONTINUE;
        }
  };
  this.flushEvent_ = window.setInterval(
      function() {
        if (coverageTracker.hitsSize_ > 0)
          LCOV.logInfo('timed out, flushing');

        coverageTracker.flushHits_();
      },
      LCOV.CoverageTracker.FLUSH_INTERVAL_MS_);
};


/**
 * End the coverage cycle by killing the interrupt hook attached to the
 * javascript debugger.  Additionally, turn the javascript debugger off.
 * @private
 */
LCOV.CoverageTracker.prototype.endCoverage_ = function() {
  LCOV.logInfo('ending');
  window.clearInterval(this.flushEvent_);
  this.flushEvent_ = null;
  this.javascriptDebugger_.interruptHook = null;
  this.javascriptDebugger_.scriptHook = null;
  this.javascriptDebugger_.off();
  this.clearCoverageData_();
  this.closeOutputStream_();
};


/**
 * Determines whether or not the script should be considered for coverage, and
 * if so then begins instrumentation.  Currently, the following rules are
 * enforced:
 * <ol>
 *   <li>The script cannot be a chrome script, (i.e. a browser or FF extension
 *       script)</li>
 *   <li>The script cannot be on the local filesystem, (i.e. a FF
 *       component)</li>
 *   <li>The script cannot be inline javascript</li>
 *   <li>The script must resemble a URL, (i.e. not "XStringBundle")</li>
 * </ol>
 * @param {jsdIScript} script The script to instrument
 * @private
 */
LCOV.CoverageTracker.prototype.instrumentScript_ = function(script) {
  var fileName = script.fileName.toString();
  var doInstrumentation = this.reUrlPattern_.test(fileName) &&
      !(fileName.lastIndexOf('js-coverage.js') == fileName.length - 14);

  // Log whether the script will be instrumented or not.
  if (!LCOV.scripts_[fileName]) {
    LCOV.scripts_[fileName] = 1;
    var prefix = doInstrumentation ? 'Instrumenting ' : 'Not Instrumenting ';
    //LCOV.logInfo(prefix + fileName);
  } else {
    LCOV.scripts_[fileName] += 1;
  }

  if (doInstrumentation) {
    // Turns on debugging for this script.
    script.flags |= LCOV.jsdIScript.FLAG_DEBUG;

    // Records all instrumentable lines in the script as not yet hit.
    this.writeOutput_('SF:' + fileName + '\n');
    var lineBegin = script.baseLineNumber;
    var lineEnd = lineBegin + script.lineExtent + 1;
    for (var lineIdx = lineBegin; lineIdx < lineEnd; lineIdx++) {
      if (script.isLineExecutable(lineIdx,
          LCOV.jsdIScript.PCMAP_SOURCETEXT)) {
        this.writeOutput_('DA:' + lineIdx + ',0\n');
      }
    }
    this.writeOutput_('end_of_record\n');
    this.flushOutputStream_();
  } else {
    // Turns off debugging for this script.
    script.flags &= ~LCOV.jsdIScript.FLAG_DEBUG;
  }
};


/**
 * Clears any stored coverage data.
 * @private
 */
LCOV.CoverageTracker.prototype.clearCoverageData_ = function() {
  //LCOV.logInfo('clearing data');
  this.hits_ = {};
  this.hitsSize_ = 0;
};


/**
 * Checks to see whether or not coverage is enabled for this test run.
 * @return {Boolean} Whether or not coverage is enabled
 * @private
 */
LCOV.CoverageTracker.prototype.getIsCoverageEnabled_ = function() {
  var userEnvironment = LCOV.environment.getService(LCOV.nsIEnvironment);
  var isEnabled;
  return userEnvironment.exists(LCOV.CoverageTracker.JS_COVERAGE_ENV_) &&
      !!userEnvironment.get(LCOV.CoverageTracker.JS_COVERAGE_ENV_);
};


/**
 * Gets the regular expression used for matching page URLs to instrument.
 * Only pages that match this pattern will be instrumented.
 */
LCOV.CoverageTracker.prototype.getUrlPattern_ = function() {
  var userEnvironment = LCOV.environment.getService(LCOV.nsIEnvironment);
  var re = userEnvironment.exists(
      LCOV.CoverageTracker.JS_COVERAGE_URL_PATTERN_ENV_) &&
      userEnvironment.get(LCOV.CoverageTracker.JS_COVERAGE_URL_PATTERN_ENV_);
  if (!re)
    re = '^http://.*js$';

  return re;
};


/**
 * Gets the output file for coverage data.
 * @return {String} Full path to the file
 * @private
 */
LCOV.CoverageTracker.prototype.getCoverageFile_ = function() {
  var userEnvironment = LCOV.environment.getService(LCOV.nsIEnvironment);
  var file;
  if (userEnvironment.exists(LCOV.CoverageTracker.JS_COVERAGE_FILE_ENV_)) {
    file = userEnvironment.get(LCOV.CoverageTracker.JS_COVERAGE_FILE_ENV_);
    if (file.length == 0) {
      file = LCOV.CoverageTracker.DEFAULT_FILE_;
    }
  } else {
    file = LCOV.CoverageTracker.DEFAULT_FILE_;
  }

  LCOV.logInfo('data will be written to ' + file);
  return file;
};


/**
 * Records a line-hit entry to the current collection data set.
 * @param {jsdIStackFrame} frame A jsdIStackFrame object representing the
 *     bottom stack frame
 * @private
 */
LCOV.CoverageTracker.prototype.recordExecution_ = function(frame) {
  var lineNumber = frame.line;
  var fileName = frame.script.fileName.toString();
  var fileData;
  if (fileName in this.hits_) {
    fileData = this.hits_[fileName];
  } else {
    // Add this file to the data set.
    fileData = this.hits_[fileName] = {};
  }

  if (lineNumber in fileData) {
    // Increment the hit value at this line.
    fileData[lineNumber]++;
  } else {
    // Make sure the working set doesn't grow too large.
    if (this.hitsSize_ >= LCOV.CoverageTracker.HITS_SIZE_THRESHOLD_) {
      LCOV.logInfo('threshold reached, flushing');
      this.flushHits_();
    }
    // Since this line has not been tracked yet, set the value to 1 hit.
    fileData[lineNumber] = 1;
    this.hitsSize_++;
  }
};


/**
 * Appends to the output file in the "lcov" format which is outlined below.
 * <pre>
 *   SF:<em>source file name 1</em>
 *   DA:<em>line_no</em>,<em>hit</em>
 *   DA:<em>line_no</em>,<em>hit</em>
 *   ...
 *   end_of_record
 *   SF:<em>source file name 2</em>
 *   DA:<em>line_no</em>,<em>hit</em>
 *   DA:<em>line_no</em>,<em>hit</em>
 *   ...
 *   end_of_record
 * </pre>
 * @private
 */
LCOV.CoverageTracker.prototype.flushHits_ = function() {
  //LCOV.logInfo('flushing');
  for (var file in this.hits_) {
    var fileData = this.hits_[file];
    this.writeOutput_('SF:' + file + '\n');

    for (var lineHit in fileData) {
      var lineHitCnt = fileData[lineHit];
        this.writeOutput_('DA:' + lineHit + ',' + lineHitCnt + '\n');
    }

    this.writeOutput_('end_of_record\n');
  }
  this.clearCoverageData_();
  this.flushOutputStream_();
};


/**
 * Opens a file stream suitable for outputting coverage results.
 * @private
 */
LCOV.CoverageTracker.prototype.openOutputStream_ = function() {
  // We're outside of the sandbox so we can write directly to disk.
  var outputFile = LCOV.localFile.createInstance(LCOV.nsILocalFile);
  outputFile.initWithPath(this.getCoverageFile_());
  this.stream_ = LCOV.fileOutputStream.createInstance(LCOV.nsIFileOutputStream);
  this.stream_.init(outputFile, 0x04 | 0x08 | 0x10, 424, 0);
};


/**
 * Writes a string to the output file stream.
 * @param {String} str The string to write to the stream
 * @private
 */
LCOV.CoverageTracker.prototype.writeOutput_ = function(str) {
  this.stream_.write(str, str.length);
};


/**
 * Closes the output stream.
 * @private
 */
LCOV.CoverageTracker.prototype.closeOutputStream_ = function() {
  this.flushOutputStream_();
  this.stream_.close();
};


/**
 * Closes the output stream.
 * @private
 */
LCOV.CoverageTracker.prototype.flushOutputStream_ = function() {
  this.stream_.flush();
};

// Setup the current top-level Firefox window for coverage.
LCOV.CoverageTracker.getInstance().setupCurrentWindow();
LCOV.logInfo('add-on loaded');
