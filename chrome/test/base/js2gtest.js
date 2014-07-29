// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Generator script for creating gtest-style JavaScript
 *     tests for extensions, WebUI and unit tests. Generates C++ gtest wrappers
 *     which will invoke the appropriate JavaScript for each test.
 * @author scr@chromium.org (Sheridan Rawlins)
 * @see WebUI testing: http://goo.gl/ZWFXF
 * @see gtest documentation: http://goo.gl/Ujj3H
 * @see chrome/chrome_tests.gypi
 * @see tools/gypv8sh.py
 */

// Arguments from rules in chrome_tests.gypi are passed in through
// python script gypv8sh.py.
if (arguments.length != 6) {
  print('usage: ' +
        arguments[0] +
        ' path-to-testfile.js testfile.js path_to_deps.js output.cc test-type');
  quit(-1);
}

/**
 * Full path to the test input file.
 * @type {string}
 */
var jsFile = arguments[1];

/**
 * Relative path to the test input file appropriate for use in the
 * C++ TestFixture's addLibrary method.
 * @type {string}
 */
var jsFileBase = arguments[2];

/**
 * The cwd, as determined by the paths of |jsFile| and |jsFileBase|.
 * This is usually relative to the root source directory and points to the
 * directory where the GYP rule processing the js file lives.
 */
var jsDirBase = jsFileBase.replace(jsFile, '');

/**
 * Path to Closure library style deps.js file.
 * @type {string?}
 */
var depsFile = arguments[3];

/**
 * Path to C++ file generation is outputting to.
 * @type {string}
 */
var outputFile = arguments[4];

/**
 * Type of this test.
 * @type {string} ('extension' | 'unit' | 'webui')
 */
var testType = arguments[5];
if (testType != 'extension' &&
    testType != 'unit' &&
    testType != 'webui') {
  print('Invalid test type: ' + testType);
  quit(-1);
}

/**
 * C++ gtest macro to use for TEST_F depending on |testType|.
 * @type {string} ('TEST_F'|'IN_PROC_BROWSER_TEST_F')
 */
var testF;

/**
 * Keeps track of whether a typedef has been generated for each test
 * fixture.
 * @type {Object.<string, string>}
 */
var typedeffedCppFixtures = {};

/**
 * Maintains a list of relative file paths to add to each gtest body
 * for inclusion at runtime before running each JavaScript test.
 * @type {Array.<string>}
 */
var genIncludes = [];

/**
 * When true, add calls to set_preload_test_(fixture|name). This is needed when
 * |testType| === 'webui' to send an injection message before the page loads,
 * but is not required or supported by any other test type.
 * @type {boolean}
 */
var addSetPreloadInfo;

/**
 * Whether cc headers need to be generated.
 * @type {boolean}
 */
var needGenHeader = true;

/**
 * Helpful hint pointing back to the source js.
 * @type {string}
 */
var argHint = '// ' + this['arguments'].join(' ');


/**
 * Generates the header of the cc file to stdout.
 * @param {string?} testFixture Name of test fixture.
 */
function maybeGenHeader(testFixture) {
  if (!needGenHeader)
    return;
  needGenHeader = false;
  print('// GENERATED FILE');
  print(argHint);
  print('// PLEASE DO NOT HAND EDIT!');
  print();

  // Output some C++ headers based upon the |testType|.
  //
  // Currently supports:
  // 'extension' - browser_tests harness, js2extension rule,
  //               ExtensionJSBrowserTest superclass.
  // 'unit' - unit_tests harness, js2unit rule, V8UnitTest superclass.
  // 'webui' - browser_tests harness, js2webui rule, WebUIBrowserTest
  // superclass.
  if (testType === 'extension') {
    print('#include "chrome/test/base/extension_js_browser_test.h"');
    testing.Test.prototype.typedefCppFixture = 'ExtensionJSBrowserTest';
    addSetPreloadInfo = false;
    testF = 'IN_PROC_BROWSER_TEST_F';
  } else if (testType === 'unit') {
    print('#include "chrome/test/base/v8_unit_test.h"');
    testing.Test.prototype.typedefCppFixture = 'V8UnitTest';
    testF = 'TEST_F';
    addSetPreloadInfo = false;
  } else {
    print('#include "chrome/test/base/web_ui_browser_test.h"');
    testing.Test.prototype.typedefCppFixture = 'WebUIBrowserTest';
    testF = 'IN_PROC_BROWSER_TEST_F';
    addSetPreloadInfo = true;
  }
  print('#include "url/gurl.h"');
  print('#include "testing/gtest/include/gtest/gtest.h"');
  if (testFixture && this[testFixture].prototype.testGenCppIncludes)
    this[testFixture].prototype.testGenCppIncludes();
  print();
}


/**
 * Convert the |includeFile| to paths appropriate for immediate
 * inclusion (path) and runtime inclusion (base).
 * @param {string} includeFile The file to include.
 * @return {{path: string, base: string}} Object describing the paths
 *     for |includeFile|. |path| is relative to cwd; |base| is relative to
 * source root.
 */
function includeFileToPaths(includeFile) {
  if (includeFile.indexOf(jsDirBase) == 0) {
    // The caller supplied a path relative to root source.
    var relPath = includeFile.replace(jsDirBase, '');
    return {
      path: relPath,
      base: jsDirBase + relPath
    };
  }

  // The caller supplied a path relative to the input js file's directory (cwd).
  return {
    path: jsFile.replace(/[^\/\\]+$/, includeFile),
    base: jsFileBase.replace(/[^\/\\]+$/, includeFile),
  };
}


/**
 * Maps object names to the path to the file that provides them.
 * Populated from the |depsFile| if any.
 * @type {Object.<string, string>}
 */
var dependencyProvidesToPaths = {};

/**
 * Maps dependency path names to object names required by the file.
 * Populated from the |depsFile| if any.
 * @type {Object.<string, Array.<string>>}
 */
var dependencyPathsToRequires = {};

if (depsFile) {
  var goog = goog || {};
  /**
   * Called by the javascript in the deps file to add modules and their
   * dependencies.
   * @param {string} path Relative path to the file.
   * @param Array.<string> provides Objects provided by this file.
   * @param Array.<string> requires Objects required by this file.
   */
  goog.addDependency = function(path, provides, requires) {
    provides.forEach(function(provide) {
      dependencyProvidesToPaths[provide] = path;
    });
    dependencyPathsToRequires[path] = requires;
  };

  // Read and eval the deps file.  It should only contain goog.addDependency
  // calls.
  eval(read(depsFile));
}

/**
 * Resolves a list of libraries to an ordered list of paths to load by the
 * generated C++.  The input should contain object names provided
 * by the deps file.  Dependencies will be resolved and included in the
 * correct order, meaning that the returned array may contain more entries
 * than the input.
 * @param {Array.<string>} deps List of dependencies.
 * @return {Array.<string>} List of paths to load.
 */
function resolveClosureModuleDeps(deps) {
  if (!depsFile && deps.length > 0) {
    print('Can\'t have closure dependencies without a deps file.');
    quit(-1);
  }
  var resultPaths = [];
  var addedPaths = {};

  function addPath(path) {
    addedPaths[path] = true;
    resultPaths.push(path);
  }

  function resolveAndAppend(path) {
    if (addedPaths[path]) {
      return;
    }
    // Set before recursing to catch cycles.
    addedPaths[path] = true;
    dependencyPathsToRequires[path].forEach(function(require) {
      var providingPath = dependencyProvidesToPaths[require];
      if (!providingPath) {
        print('Unknown object', require, 'required by', path);
        quit(-1);
      }
      resolveAndAppend(providingPath);
    });
    resultPaths.push(path);
  }

  // Always add closure library's base.js if provided by deps.
  var basePath = dependencyProvidesToPaths['goog'];
  if (basePath) {
    addPath(basePath);
  }

  deps.forEach(function(dep) {
    var providingPath = dependencyProvidesToPaths[dep];
    if (providingPath) {
      resolveAndAppend(providingPath);
    } else {
      print('Unknown dependency:', dep);
      quit(-1);
    }
  });

  return resultPaths;
}

/**
 * Output |code| verbatim.
 * @param {string} code The code to output.
 */
function GEN(code) {
  maybeGenHeader(null);
  print(code);
}

/**
 * Outputs |commentEncodedCode|, converting comment to enclosed C++ code.
 * @param {function} commentEncodedCode A function in the following format (note
 * the space in '/ *' and '* /' should be removed to form a comment delimiter):
 *    function() {/ *! my_cpp_code.DoSomething(); * /
 *    Code between / *! and * / will be extracted and written to stdout.
 */
function GEN_BLOCK(commentEncodedCode) {
  var code = commentEncodedCode.toString().
      replace(/^[^\/]+\/\*!?/, '').
      replace(/\*\/[^\/]+$/, '').
      replace(/^\n|\n$/, '').
      replace(/\s+$/, '');
  GEN(code);
}

/**
 * Generate includes for the current |jsFile| by including them
 * immediately and at runtime.
 * The paths are allowed to be:
 *   1. relative to the root src directory (i.e. similar to #include's).
 *   2. relative to the directory specified in the GYP rule for the file.
 * @param {Array.<string>} includes Paths to JavaScript files to
 *     include immediately and at runtime.
 */
function GEN_INCLUDE(includes) {
  for (var i = 0; i < includes.length; i++) {
    var includePaths = includeFileToPaths(includes[i]);
    var js = read(includePaths.path);
    ('global', eval)(js);
    genIncludes.push(includePaths.base);
  }
}

/**
 * Generate gtest-style TEST_F definitions for C++ with a body that
 * will invoke the |testBody| for |testFixture|.|testFunction|.
 * @param {string} testFixture The name of this test's fixture.
 * @param {string} testFunction The name of this test's function.
 * @param {Function} testBody The function body to execute for this test.
 */
function TEST_F(testFixture, testFunction, testBody) {
  maybeGenHeader(testFixture);
  var browsePreload = this[testFixture].prototype.browsePreload;
  var browsePrintPreload = this[testFixture].prototype.browsePrintPreload;
  var testGenPreamble = this[testFixture].prototype.testGenPreamble;
  var testGenPostamble = this[testFixture].prototype.testGenPostamble;
  var typedefCppFixture = this[testFixture].prototype.typedefCppFixture;
  var isAsyncParam = testType === 'unit' ? '' :
      this[testFixture].prototype.isAsync + ', ';
  var testShouldFail = this[testFixture].prototype.testShouldFail;
  var testPredicate = testShouldFail ? 'ASSERT_FALSE' : 'ASSERT_TRUE';
  var extraLibraries = genIncludes.concat(
      this[testFixture].prototype.extraLibraries.map(
          function(includeFile) {
            return includeFileToPaths(includeFile).base;
          }),
      resolveClosureModuleDeps(this[testFixture].prototype.closureModuleDeps));

  if (typedefCppFixture && !(testFixture in typedeffedCppFixtures)) {
    print('typedef ' + typedefCppFixture + ' ' + testFixture + ';');
    typedeffedCppFixtures[testFixture] = typedefCppFixture;
  }

  print(testF + '(' + testFixture + ', ' + testFunction + ') {');
  for (var i = 0; i < extraLibraries.length; i++) {
    print('  AddLibrary(base::FilePath(FILE_PATH_LITERAL("' +
        extraLibraries[i].replace(/\\/g, '/') + '")));');
  }
  print('  AddLibrary(base::FilePath(FILE_PATH_LITERAL("' +
      jsFileBase.replace(/\\/g, '/') + '")));');
  if (addSetPreloadInfo) {
    print('  set_preload_test_fixture("' + testFixture + '");');
    print('  set_preload_test_name("' + testFunction + '");');
  }
  if (testGenPreamble)
    testGenPreamble(testFixture, testFunction);
  if (browsePreload)
    print('  BrowsePreload(GURL("' + browsePreload + '"));');
  if (browsePrintPreload) {
    print('  BrowsePrintPreload(GURL(WebUITestDataPathToURL(\n' +
          '      FILE_PATH_LITERAL("' + browsePrintPreload + '"))));');
  }
  print('  ' + testPredicate + '(RunJavascriptTestF(' + isAsyncParam +
        '"' + testFixture + '", ' +
        '"' + testFunction + '"));');
  if (testGenPostamble)
    testGenPostamble(testFixture, testFunction);
  print('}');
  print();
}

// Now that generation functions are defined, load in |jsFile|.
var js = read(jsFile);
eval(js);
