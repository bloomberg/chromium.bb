#!/usr/bin/env python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""The frontend for the Mojo bindings system."""


import argparse
import imp
import os
import pprint
import sys

script_dir = os.path.dirname(os.path.realpath(__file__))
sys.path.insert(0, os.path.join(script_dir, 'pylib'))

from generators import mojom_data
from parse import mojo_parser
from parse import mojo_translate


def LoadGenerators(generators_string):
  if not generators_string:
    return []  # No generators.

  generators = []
  for generator_name in [s.strip() for s in generators_string.split(",")]:
    # "Built-in" generators:
    if generator_name.lower() == "c++":
      generator_module = __import__("generators.mojom_cpp_generator",
                                    fromlist=["mojom_cpp_generator"])
    elif generator_name.lower() == "javascript":
      generator_module = __import__("generators.mojom_js_generator",
                                    fromlist=["mojom_js_generator"])
    # Specified generator python module:
    elif generator_name.endswith(".py"):
      generator_module = imp.load_source(os.path.basename(generator_name)[:-3],
                                         generator_name)
    else:
      print "Unknown generator name %s" % generator_name
      sys.exit(1)
    generators.append(generator_module)
  return generators


def Main():
  parser = argparse.ArgumentParser(
      description="Generate bindings from mojom files.")
  parser.add_argument("filename", nargs="+",
                      help="mojom input file")
  parser.add_argument("-i", "--include_dir", dest="include_dir", default=".",
                      help="include path for #includes")
  parser.add_argument("-o", "--output_dir", dest="output_dir", default=".",
                      help="output directory for generated files")
  parser.add_argument("-g", "--generators", dest="generators_string",
                      metavar="GENERATORS", default="c++,javascript",
                      help="comma-separated list of generators")
  parser.add_argument("--debug_print_intermediate", action="store_true",
                      help="print the intermediate representation")
  args = parser.parse_args()

  # TODO(vtl): Load these dynamically. (Also add a command-line option to
  # specify which generators.)
  generator_modules = LoadGenerators(args.generators_string)

  if not os.path.exists(args.output_dir):
    os.makedirs(args.output_dir)

  for filename in args.filename:
    name = os.path.splitext(os.path.basename(filename))[0]
    # TODO(darin): There's clearly too many layers of translation here!  We can
    # at least avoid generating the serialized Mojom IR.
    tree = mojo_parser.Parse(filename)
    mojom = mojo_translate.Translate(tree, name)
    if args.debug_print_intermediate:
      pprint.PrettyPrinter().pprint(mojom)
    module = mojom_data.OrderedModuleFromData(mojom)
    for generator_module in generator_modules:
      generator = generator_module.Generator(module, args.include_dir,
                                             args.output_dir)
      generator.GenerateFiles()

  return 0


if __name__ == "__main__":
  sys.exit(Main())
