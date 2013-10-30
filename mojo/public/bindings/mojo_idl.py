#!/usr/bin/env python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""The frontend for the Mojo bindings system."""


import sys
from optparse import OptionParser
from parser import mojo_parser
from parser import mojo_translate
from generators import mojom_data
from generators import mojom_cpp_generator


def Main():
  parser = OptionParser(usage="usage: %prog [options] filename1 [filename2...]")
  parser.add_option("-o", "--outdir", dest="outdir", default=".",
                    help="specify output directory")
  (options, args) = parser.parse_args()

  if len(args) < 1:
    parser.print_help()
    sys.exit(1)

  for filename in args:
    # TODO(darin): There's clearly too many layers of translation here!  We can
    # at least avoid generating the serialized Mojom IR.
    tree = mojo_parser.Parse(filename)
    mojom = mojo_translate.Translate(tree)
    module = mojom_data.ModuleFromData(mojom)
    cpp = mojom_cpp_generator.CPPGenerator(module)
    cpp.GenerateFiles(options.outdir)


if __name__ == '__main__':
  Main()
