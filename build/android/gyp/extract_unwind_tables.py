#!/usr/bin/env python
# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Extracts the unwind tables in ARM EHABI format.

The ARM EHABI format requires 2 sections for unwinding, ARM.exidx and ARM.extab.
This script copies these sections from unstripped binary into an output file.

Usage:
  extract_unwind_tables.py --input_path [root path to unstripped chrome.so]
      --output_path [output path]

"""

import argparse
import struct
import subprocess
import sys


def _GetArmSectionsFromObjdump(input_path):
  """Parses the objdump of the binary and returns the relevant sections."""
  objdump = subprocess.check_output(['objdump', '-h', input_path]).splitlines()
  sections = {}
  for line in objdump:
    if '.ARM' not in line:
      continue
    parts = line.split()
    section = {}
    section['size'] = int(parts[2], 16)
    section['offset'] = int(parts[5], 16)
    sections[parts[1]] = section
  return sections


def _Write4Bytes(output_file, val):
  """Writes a 32 bit unsigned integer to the given output file."""
  output_file.write(struct.pack('<L', val));


def _AddSectionToOutput(input_path, output_file, section):
  """Copies the specified section from input file to output."""
  _Write4Bytes(output_file, section['size'])
  _Write4Bytes(output_file, section['offset'])

  with open(input_path, 'rb') as f:
    f.seek(section['offset'])
    data = f.read(section['size'])
    output_file.write(data)


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument(
      '--input_path', required=True,
      help='The input path of the unstripped binary')
  parser.add_argument(
      '--output_path', required=True,
      help='The path of the output file')
  args = parser.parse_args()

  sections = _GetArmSectionsFromObjdump(args.input_path)
  exidx = sections.get('.ARM.exidx')
  extab = sections.get('.ARM.extab')
  if not exidx or not extab:
    raise Exception('No arm32 exception section found.')
  with open(args.output_path, 'wb') as outputFile:
    _AddSectionToOutput(args.input_path, outputFile, exidx)
    _AddSectionToOutput(args.input_path, outputFile, extab)

  return 0

if __name__ == '__main__':
  sys.exit(main())
