// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
if (arguments.length < 3) {
  print('usage: ' +
        arguments[0] + ' path-to-testfile.js testfile.js [output.cc]');
  quit();
}
var js_file = arguments[1];
var js_file_base = arguments[2];
var outputfile = arguments[3];
var prevfuncs = {};
for (var func in this) {
  if (this[func] instanceof Function)
    prevfuncs[func] = func;
}
var js = load(js_file);
if (!('test_fixture' in this)) {
  print(js_file + ' did not define test_fixture.');
  quit(-1);
}
if (!('test_add_library' in this)) {
  this['test_add_library'] = true;
}
print('// GENERATED FILE');
print('// ' + arguments.join(' '));
print('// PLEASE DO NOT HAND EDIT!');
print();
for (var func in this) {
  if (!prevfuncs[func] && this[func] instanceof Function) {
    print('IN_PROC_BROWSER_TEST_F(' + test_fixture + ', ' + func + ') {');
    if (test_add_library)
      print('  AddLibrary(FilePath(FILE_PATH_LITERAL("' + js_file_base +
            '")));');
    print('  ASSERT_TRUE(RunJavascriptTest("' + func + '"));');
    print('}');
    print();
  }
}
