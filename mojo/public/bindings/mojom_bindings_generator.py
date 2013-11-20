#!/usr/bin/env python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""The frontend for the Mojo bindings system."""


import os
import sys
from optparse import OptionParser
from parse import mojo_parser
from parse import mojo_translate
from generators import mojom_data
from generators import mojom_js_generator
from generators import mojom_cpp_generator


def Main():
  parser = OptionParser(usage="usage: %prog [options] filename1 [filename2...]")
  parser.add_option("-i", "--include_dir", dest="include_dir", default=".",
                    help="specify directory for #includes")
  parser.add_option("-o", "--output_dir", dest="output_dir", default=".",
                    help="specify output directory")
  (options, args) = parser.parse_args()

  if len(args) < 1:
    parser.print_help()
    sys.exit(1)

  if not os.path.exists(options.output_dir):
    os.makedirs(options.output_dir)

  for filename in args:
    name = os.path.splitext(os.path.basename(filename))[0]
    # TODO(darin): There's clearly too many layers of translation here!  We can
    # at least avoid generating the serialized Mojom IR.
    tree = mojo_parser.Parse(filename)
    mojom = mojo_translate.Translate(tree, name)
    module = mojom_data.ModuleFromData(mojom)
    cpp = mojom_cpp_generator.CPPGenerator(
        module, options.include_dir, options.output_dir)
    cpp.GenerateFiles()
    js = mojom_js_generator.JSGenerator(
        module, options.include_dir, options.output_dir)
    js.GenerateFiles()


if __name__ == '__main__':
  Main()
