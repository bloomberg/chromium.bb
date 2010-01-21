// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Swig Interface for PyAuto.
// PyAuto makes the Automation Proxy interface available in Python
//
// Running swig as:
//   swig -python -c++ chrome/test/pyautolib/pyautolib.i
// would generate pyautolib.py, pyautolib_wrap.cxx

%module pyautolib

%include "std_string.i"
%include "std_wstring.i"

%{
#include "chrome/test/pyautolib/pyautolib.h"
%}

// Make functions using (int argc, char** argv) usable as (sys.argv) from python
%typemap(in) (int argc, char **argv) {
  int i;
  if (!PyList_Check($input)) {
    PyErr_SetString(PyExc_ValueError, "Expecting a list");
    return NULL;
  }
  $1 = PyList_Size($input);
  $2 = (char **) malloc(($1+1)*sizeof(char *));
  for (i = 0; i < $1; i++) {
    PyObject *s = PyList_GetItem($input,i);
    if (!PyString_Check(s)) {
        free($2);
        PyErr_SetString(PyExc_ValueError, "List items must be strings");
        return NULL;
    }
    $2[i] = PyString_AsString(s);
  }
  $2[i] = 0;
}

%typemap(freearg) (int argc, char **argv) {
   if ($2) free($2);
}


// TODO(nirnimesh): Fill in the autodoc constructs - crbug.com/32287
class PyUITestSuite {
 public:
  PyUITestSuite(int argc, char** argv);

  virtual void SetUp();
  virtual void TearDown();

  void NavigateToURL(const char* url_string);
  std::wstring GetActiveTabTitle();

  // BrowserProxy methods
  void SetShelfVisible(bool is_visible);
  bool IsShelfVisible();
  void OpenFindInPage();
  bool IsFindInPageVisible();
};
