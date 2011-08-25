// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
if (arguments.length < 3) {
  print('usage: ' +
        arguments[0] + ' path-to-testfile.js testfile.js [output.cc]');
  quit(-1);
}
var jsFile = arguments[1];
var jsFileBase = arguments[2];
var outputFile = arguments[3];

// Generate the file to stdout.
print('// GENERATED FILE');
print('// ' + arguments.join(' '));
print('// PLEASE DO NOT HAND EDIT!');
print();
print('#include "chrome/browser/ui/webui/web_ui_browsertest.h"');
print('#include "googleurl/src/gurl.h"');
print('#include "testing/gtest/include/gtest/gtest.h"');
print();

function GEN(code) {
  print(code);
}

var typedeffedCppFixtures = {};

function TEST_F(testFixture, testFunction, testBody) {
  var browsePreload = this[testFixture].prototype.browsePreload;
  var browsePrintPreload = this[testFixture].prototype.browsePrintPreload;
  var testGenPreamble = this[testFixture].prototype.testGenPreamble;
  var testGenPostamble = this[testFixture].prototype.testGenPostamble;
  var typedefCppFixture = this[testFixture].prototype.typedefCppFixture;
  var isAsync = this[testFixture].prototype.isAsync;

  if (typedefCppFixture && !(testFixture in typedeffedCppFixtures)) {
    print('typedef ' + typedefCppFixture + ' ' + testFixture + ';');
    typedeffedCppFixtures[testFixture] = typedefCppFixture;
  }

  print('IN_PROC_BROWSER_TEST_F(' + testFixture + ', ' + testFunction + ') {');
  if (testGenPreamble)
    testGenPreamble(testFixture, testFunction);
  print('  AddLibrary(FilePath(FILE_PATH_LITERAL("' + jsFileBase + '")));');
  if (browsePreload) {
    print('  BrowsePreload(GURL("' + browsePreload + '"), "' + testFixture +
          '", "' + testFunction + '");');
    }
  if (browsePrintPreload) {
    print('  BrowsePrintPreload(GURL(WebUITestDataPathToURL(\n' +
          '      FILE_PATH_LITERAL("' + browsePrintPreload + '"))),\n' +
          '      "' + testFixture + '", "' + testFunction + '");');
  }
  print('  ASSERT_TRUE(RunJavascriptTestF(' + isAsync + ', ' +
        '"' + testFixture + '", ' +
        '"' + testFunction + '"));');
  if (testGenPostamble)
    testGenPostamble(testFixture, testFunction);
  print('}');
  print();
}

var js = read(jsFile);
eval(js);
