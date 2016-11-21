#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Helper to extract the output files from a grd file.
Usage: Call add_options to add the parser options (see optparser module).
Then call get_grd_outputs with the options obtained from parser.parse_args.
"""

import os
import sys


def add_options(parser):
  '''Adds options to an option parser that are required for getting grit output
  files.

  The following options are required:
    --grit_info: Path to the grit_info.py script.
    --grd_input: Path to the grd file.
    --grd_strip_path_prefix: Prefix to be removed from the output paths.

  The following options are optional:
    -D <define1> ... -D <defineN>: List of grit defines.
    -E <env1> ... -D <envN>:       List of grit build environment variables.

  Args:
    parser: Option parser (from optparse.OptionParser()).
  '''
  parser.add_option("--grit_info", dest="grit_info")
  parser.add_option("--grd_input", dest="grd_input")
  parser.add_option("--grd_strip_path_prefix", dest="grd_strip_path_prefix")
  parser.add_option("-D", action="append", dest="grit_defines", default=[])
  parser.add_option("-E", action="append", dest="grit_build_env", default=[])


def get_grd_outputs(options):
  '''Retrieves output files from a grd file. Call |add_options| before invoking
  the option parser.

  Args:
    options: Parsed options (first return value from parser.parse_args(...)).
  '''
  # Build a list of defines (parsed from -D <define> args).
  grit_defines = {}
  for define in options.grit_defines:
    grit_defines[define] = 1

  # Get the grit outputs.
  grit_path = os.path.join(os.getcwd(), os.path.dirname(options.grit_info))
  sys.path.append(grit_path)
  import grit_info
  outputs = grit_info.Outputs(options.grd_input, grit_defines,
                              'GRIT_DIR/../gritsettings/resource_ids')

  # Strip the path prefix from the filenames.
  result = []
  for item in outputs:
    assert item.startswith(options.grd_strip_path_prefix)
    result.append(item[len(options.grd_strip_path_prefix):])
  return result
