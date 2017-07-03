#!/usr/bin/env python
#
# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This script creates a "jumbo" file which merges all incoming files
for compiling.

"""

from __future__ import print_function

import argparse


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument("--outputs", nargs="+", required=True,
                      help='List of output files to split input into')
  parser.add_argument("--file-list", required=True)
  parser.add_argument("--verbose", action="store_true")
  args = parser.parse_args()

  output_count = len(args.outputs)

  lines = []
  # If written with gn |write_file| each file is on its own line.
  with open(args.file_list) as file_list_file:
    lines = [line.strip() for line in file_list_file if line.strip()]
  # If written with gn |response_file_contents| the files are space separated.
  inputs = []
  for line in lines:
    inputs.extend(line.split())
  input_count = len(inputs)

  written_inputs = 0
  for output_index, output_file in enumerate(args.outputs):
    # TODO: Check if the file is right already and then do not update it.
    with open(output_file, "w") as out:
      out.write("/* This is a Jumbo file. Don't edit. */\n\n")
      out.write("/* Generated with merge_for_jumbo.py. */\n\n")
      input_limit = (output_index + 1) * input_count / output_count
      while written_inputs < input_limit:
        filename = inputs[written_inputs]
        written_inputs += 1
        # The source list includes headers which should not be
        # compiled, and Objective C files which will be special cased
        # later since they will not compile correctly if included in a
        # C++ file. We will just skip them here for now.
        if filename.endswith((".h", ".mm")):
          continue

        out.write("#include \"%s\"\n" % filename)

  if args.verbose:
      print("Generated %s (%d files) based on %s" % (str(args.outputs),
                                                     written_inputs,
                                                     args.file_list))

if __name__ == "__main__":
    main()
