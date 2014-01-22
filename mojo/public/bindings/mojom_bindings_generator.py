#!/usr/bin/env python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""The frontend for the Mojo bindings system."""


import os
import sys
from argparse import ArgumentParser
from parse import mojo_parser
from parse import mojo_translate
from generators import mojom_data
from generators import mojom_cpp_generator
from generators import mojom_js_generator


def Main():
  parser = ArgumentParser(description="Generate bindings from mojom files.")
  parser.add_argument("filename", nargs="+",
                      help="mojom input file")
  parser.add_argument("-i", "--include_dir", dest="include_dir", default=".",
                      help="include path for #includes")
  parser.add_argument("-o", "--output_dir", dest="output_dir", default=".",
                      help="output directory for generated files")
  args = parser.parse_args()

  # TODO(vtl): Load these dynamically. (Also add a command-line option to
  # specify which generators.)
  generator_modules = [mojom_cpp_generator, mojom_js_generator]

  if not os.path.exists(args.output_dir):
    os.makedirs(args.output_dir)

  for filename in args.filename:
    name = os.path.splitext(os.path.basename(filename))[0]
    # TODO(darin): There's clearly too many layers of translation here!  We can
    # at least avoid generating the serialized Mojom IR.
    tree = mojo_parser.Parse(filename)
    mojom = mojo_translate.Translate(tree, name)
    module = mojom_data.OrderedModuleFromData(mojom)
    for generator_module in generator_modules:
      generator = generator_module.Generator(module, args.include_dir,
                                             args.output_dir)
      generator.GenerateFiles()


if __name__ == "__main__":
  Main()
