// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a placeholder for compiling content_java.  The content java library
// is compiled with no array defined, and then the build system creates a
// version of the file with the real list of libraries required (which changes
// based upon which .apk is being built).

// The result of including this template in the NativeLibraries.template has to
// compile. Since LIBRARIES is final in NativeLibraries.template we have to
// initalize it with something to allow the resulting Java file to compile.
// By including this file we will get:
//
// public static final String[] LIBRARIES
// ={}
// ;
//
// Which does compile.
={}
