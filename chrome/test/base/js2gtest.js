// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Generator script for creating gtest-style JavaScript
 *     tests for WebUI and unit tests. Generates C++ gtest wrappers
 *     which will invoke the appropriate JavaScript for each test.
 * @author scr@chromium.org (Sheridan Rawlins)
 * @see WebUI testing: http://goo.gl/ZWFXF
 * @see gtest documentation: http://goo.gl/Ujj3H
 * @see chrome/chrome_tests.gypi
 * @see tools/gypv8sh.py
 */

// Arguments from rules in chrome_tests.gypi are passed in through
// python script gypv8sh.py.
if (arguments.length < 4) {
  print('usage: ' +
        arguments[0] + ' path-to-testfile.js testfile.js output.cc test-type');
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
 * Path to C++ file generation is outputting to.
 * @type {string}
 */
var outputFile = arguments[3];

/**
 * Type of this test.
 * @type {string} ('unit'| 'webui')
 */
var testType = arguments[4];

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
 * |testType| === 'browser' to send an injection message before the page loads,
 * but is not required or supported for |testType| === 'unit'.
 * @type {boolean}
 */
var addSetPreloadInfo;

// Generate the file to stdout.
print('// GENERATED FILE');
print('// ' + arguments.join(' '));
print('// PLEASE DO NOT HAND EDIT!');
print();

// Output some C++ headers based upon the |testType|.
//
// Currently supports:
// 'unit' - unit_tests harness, js2unit rule, V8UnitTest superclass.
// 'webui' - browser_tests harness, js2webui rule, WebUIBrowserTest superclass.
if (testType === 'unit') {
  print('#include "chrome/test/base/v8_unit_test.h"');
  testing.Test.prototype.typedefCppFixture = 'V8UnitTest';
  testF = 'TEST_F';
  addSetPreloadInfo = false;
} else {
  print('#include "chrome/browser/ui/webui/web_ui_browsertest.h"');
  testing.Test.prototype.typedefCppFixture = 'WebUIBrowserTest';
  testF = 'IN_PROC_BROWSER_TEST_F';
  addSetPreloadInfo = true;
}
print('#include "googleurl/src/gurl.h"');
print('#include "testing/gtest/include/gtest/gtest.h"');
print();

/**
 * Convert the |includeFile| to paths appropriate for immediate
 * inclusion (path) and runtime inclusion (base).
 * @param {string} includeFile The file to include.
 * @return {{path: string, base: string}} Object describing the paths
 *     for |includeFile|.
 */
function includeFileToPaths(includeFile) {
  return {
    path: jsFile.replace(/[^\/]+$/, includeFile),
    base: jsFileBase.replace(/[^\/]+$/, includeFile),
  };
}

/**
 * Output |code| verbatim.
 * @param {string} code The code to output.
 */
function GEN(code) {
  print(code);
}

/**
 * Generate includes for the current |jsFile| by including them
 * immediately and at runtime.
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
          }));

  if (typedefCppFixture && !(testFixture in typedeffedCppFixtures)) {
    print('typedef ' + typedefCppFixture + ' ' + testFixture + ';');
    typedeffedCppFixtures[testFixture] = typedefCppFixture;
  }

  print(testF + '(' + testFixture + ', ' + testFunction + ') {');
  for (var i = 0; i < extraLibraries.length; i++) {
    print('  AddLibrary(FilePath(FILE_PATH_LITERAL("' +
        extraLibraries[i].replace(/\\/g, '/') + '")));');
  }
  print('  AddLibrary(FilePath(FILE_PATH_LITERAL("' +
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
