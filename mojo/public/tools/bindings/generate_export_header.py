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


def WriteHeader(output_file, relative_path, prefix):
  substitutions = {
    "guard": relative_path.replace("/", "_").replace(".", "_").upper() + "_",
    "impl_macro": prefix + "_IMPL",
    "export_macro": prefix + "_EXPORT",
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
      "--macro_prefix", type=str, required=True,
      help=("A prefix used to generate _IMPL and _EXPORT macro names."))
  parser.add_argument(
      "--output_file", type=str, required=True,
      help=("The file path to which the generated header should be written."))
  parser.add_argument(
      "--relative_path", type=str, required=True,
      help=("The generated header's path relative to the generated output"
            " root. Used to generate header guards."))

  params, _ = parser.parse_known_args()
  WriteHeader(params.output_file, params.relative_path, params.macro_prefix)


if __name__ == "__main__":
  sys.exit(main())
