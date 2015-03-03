# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import datetime
import util
import os

H_FILE_TEMPLATE = \
"""// Copyright %(year)d The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// NOTE: this file is generated from "%(source)s". Do not modify directly.

#ifndef %(include_guard)s
#define %(include_guard)s

#if defined(COMPONENT_BUILD)

#if defined(WIN32)

#if defined(%(impl_macro)s)
#define %(export_macro)s __declspec(dllexport)
#else
#define %(export_macro)s __declspec(dllimport)
#endif  // defined(%(impl_macro)s)

#else  // defined(WIN32)

#if defined(%(impl_macro)s)
#define %(export_macro)s __attribute__((visibility("default")))
#else
#define %(export_macro)s
#endif  // defined(%(impl_macro)s)

#endif  // defined(WIN32)

#else  // defined(COMPONENT_BUILD)

#define %(export_macro)s

#endif

#endif  // %(include_guard)s
"""

def GenHFile(declaration):
  subs = {}
  subs['year'] = datetime.date.today().year
  subs['source'] = declaration.path
  subs['include_guard'] = util.PathToIncludeGuard(
                              declaration.export_h_include_path)
  subs['export_macro'] = declaration.component_export_macro
  subs['impl_macro'] = declaration.component_impl_macro
  return H_FILE_TEMPLATE % subs

def ListOutputs(declaration, destination):
  dirname = os.path.join(destination, os.path.dirname(declaration.path))
  h_file_path = os.path.join(dirname, declaration.export_h_name)
  return [h_file_path]

def Gen(declaration, destination):
  h_file_path = ListOutputs(declaration, destination)[0]
  util.CreateDirIfNotExists(os.path.dirname(h_file_path))
  open(h_file_path, 'w').write(GenHFile(declaration))
