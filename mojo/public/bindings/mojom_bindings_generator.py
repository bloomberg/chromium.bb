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
sys.path.insert(0, os.path.join(script_dir, "pylib"))

from generate import mojom_data
from parse import mojo_parser
from parse import mojo_translate


def LoadGenerators(generators_string):
  if not generators_string:
    return []  # No generators.

  generators = []
  for generator_name in [s.strip() for s in generators_string.split(",")]:
    # "Built-in" generators:
    if generator_name.lower() == "c++":
      generator_name = os.path.join(script_dir, "generators",
                                    "mojom_cpp_generator.py")
    elif generator_name.lower() == "javascript":
      generator_name = os.path.join(script_dir, "generators",
                                    "mojom_js_generator.py")
    # Specified generator python module:
    elif generator_name.endswith(".py"):
      pass
    else:
      print "Unknown generator name %s" % generator_name
      sys.exit(1)
    generator_module = imp.load_source(os.path.basename(generator_name)[:-3],
                                       generator_name)
    generators.append(generator_module)
  return generators


def ProcessFile(args, generator_modules, filename, processed_files):
  # Ensure we only visit each file once.
  if filename in processed_files:
    if processed_files[filename] is None:
      raise Exception("Circular dependency: " + filename)
    return processed_files[filename]
  processed_files[filename] = None

  dirname, name = os.path.split(filename)
  # TODO(darin): There's clearly too many layers of translation here!  We can
  # at least avoid generating the serialized Mojom IR.
  tree = mojo_parser.Parse(filename)
  mojom = mojo_translate.Translate(tree, name)
  if args.debug_print_intermediate:
    pprint.PrettyPrinter().pprint(mojom)

  # Process all our imports first and collect the module object for each.
  # We use these to generate proper type info.
  for import_data in mojom['imports']:
    import_filename = os.path.join(dirname, import_data['filename'])
    import_data['module'] = ProcessFile(
        args, generator_modules, import_filename, processed_files)

  module = mojom_data.OrderedModuleFromData(mojom)

  # Set the path as relative to the source root.
  module.path = os.path.relpath(os.path.abspath(filename),
                                os.path.abspath(args.depth))

  # Normalize to unix-style path here to keep the generators simpler.
  module.path = module.path.replace('\\', '/')

  for generator_module in generator_modules:
    generator = generator_module.Generator(module, args.output_dir)
    generator.GenerateFiles()

  processed_files[filename] = module
  return module


def Main():
  parser = argparse.ArgumentParser(
      description="Generate bindings from mojom files.")
  parser.add_argument("filename", nargs="+",
                      help="mojom input file")
  parser.add_argument("-d", "--depth", dest="depth", default=".",
                      help="depth from source root")
  parser.add_argument("-o", "--output_dir", dest="output_dir", default=".",
                      help="output directory for generated files")
  parser.add_argument("-g", "--generators", dest="generators_string",
                      metavar="GENERATORS", default="c++,javascript",
                      help="comma-separated list of generators")
  parser.add_argument("--debug_print_intermediate", action="store_true",
                      help="print the intermediate representation")
  args = parser.parse_args()

  generator_modules = LoadGenerators(args.generators_string)

  if not os.path.exists(args.output_dir):
    os.makedirs(args.output_dir)

  for filename in args.filename:
    try:
      ProcessFile(args, generator_modules, filename, {})
    except mojo_parser.ParseError, e:
      print e
      return 1

  return 0


if __name__ == "__main__":
  sys.exit(Main())
