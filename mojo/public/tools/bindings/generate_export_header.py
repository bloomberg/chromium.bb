#!/usr/bin/env python
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Generates a C++ header to define a component export macro."""

import argparse
import os
import sys


_TEMPLATE = """// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef {{guard}}
#define {{guard}}

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined({{impl_macro}})
#define {{export_macro}} __declspec(dllexport)
#else
#define {{export_macro}} __declspec(dllimport)
#endif  // defined({{impl_macro}})

#else  // defined(WIN32)

#if defined({{impl_macro}})
#define {{export_macro}} __attribute__((visibility("default")))
#else
#define {{export_macro}}
#endif

#endif

#else  // defined(COMPONENT_BUILD)
#define {{export_macro}}
#endif

#endif  // {{guard}}
"""


def WriteHeader(output_file, relative_path, impl_macro, export_macro):
  # Allow inputs for impl_macro to be of the form FOO_IMPL=1. Drop the "=1".
  impl_macro = impl_macro.split("=")[0]

  substitutions = {
    "guard": relative_path.replace("/", "_").replace(".", "_").upper() + "_",
    "impl_macro": impl_macro,
    "export_macro": export_macro,
  }
  contents = _TEMPLATE
  for k, v in substitutions.iteritems():
    contents = contents.replace("{{%s}}" % k, v)
  with open(output_file, "w") as f:
    f.write(contents)


def main():
  parser = argparse.ArgumentParser(
      description=__doc__,
      formatter_class=argparse.RawDescriptionHelpFormatter)
  parser.add_argument(
      "--impl_macro", type=str, required=True,
      help=("The name of the macro to emit for component IMPL blocks."))
  parser.add_argument(
      "--export_macro", type=str, required=True,
      help=("The name of the macro to emit for component EXPORT attributes."))
  parser.add_argument(
      "--output_file", type=str, required=True,
      help=("The file path to which the generated header should be written."))
  parser.add_argument(
      "--relative_path", type=str, required=True,
      help=("The generated header's path relative to the generated output"
            " root. Used to generate header guards."))

  params, _ = parser.parse_known_args()
  WriteHeader(params.output_file, params.relative_path, params.impl_macro,
              params.export_macro)


if __name__ == "__main__":
  sys.exit(main())
