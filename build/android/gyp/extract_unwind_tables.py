#!/usr/bin/env python
# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Extracts the unwind tables in from breakpad symbol files

Runs dump_syms on the given binary file and extracts the CFI data into the
given output file.
The output file is a binary file containing CFI rows ordered based on function
address. The output file only contains rows that match the most popular rule
type in CFI table, to reduce the output size and specify data in compact format.
See doc https://github.com/google/breakpad/blob/master/docs/symbol_files.md.
1. The CFA rules should be of postfix form "SP <val> +".
2. The RA rules should be of postfix form "CFA <val> + ^".
Note: breakpad represents dereferencing address with '^' operator.

The output rows are all 64 bits. We have 2 types of rows, FUNCTION and CFI.
Each function with CFI info has a single FUNCTION row, followed by one or more
CFI rows. All the addresses of the CFI rows will be within the function.
1. FUNCTION. Bits in order of high to low represent:
    31 bits: specifies function address, without the last bit (always 0).
     1 bit : always 1. Specifies the row type is FUNCTION.
    32 bits: length of the current function.

2. CFI. Bits in order of high to low represent:
    31 bits: instruction address in the current function.
     1 bit : always 0. Specifies teh row type is CFI.
    30 bits: CFA offset / 4.
     2 bits: RA offset / 4.

Usage:
  extract_unwind_tables.py --input_path [root path to unstripped chrome.so]
      --output_path [output path] --dump_syms_path [path to dump_syms binary]
"""

import argparse
import re
import struct
import subprocess
import sys
import tempfile


_CFA_REG = '.cfa'
_RA_REG = '.ra'

_ADDR_ENTRY = 0
_LENGTH_ENTRY = 1


def _Write4Bytes(output_file, val):
  """Writes a 32 bit unsigned integer to the given output file."""
  output_file.write(struct.pack('<L', val));


def _FindRuleForRegister(cfi_row, reg):
  """Returns the postfix expression as string for a given register.

  Breakpad CFI row format specifies rules for unwinding each register in postfix
  expression form separated by space. Each rule starts with register name and a
  colon. Eg: "CFI R1: <rule> R2: <rule>".
  """
  out = []
  found_register = False
  for part in cfi_row:
    if found_register:
      if part[-1] == ':':
        break
      out.append(part)
    elif part == reg + ':':
      found_register = True
  return ' '.join(out)


def _GetCfaAndRaOffset(cfi_row):
  """Returns a tuple with 2 numbers (cfa_offset, ra_offset).

  Returns right values if rule matches the predefined criteria. Returns (0, 0)
  otherwise. The criteria for CFA rule is postfix form "SP <val> +" and RA rule
  is postfix form "CFA -<val> + ^".
  """
  cfa_offset = 0
  ra_offset = 0
  cfa_rule = _FindRuleForRegister(cfi_row, _CFA_REG)
  ra_rule = _FindRuleForRegister(cfi_row, _RA_REG)
  if cfa_rule and re.match(r'sp [0-9]+ \+', cfa_rule):
    cfa_offset = int(cfa_rule.split()[1], 10)
  if ra_rule:
    if not re.match(r'.cfa -[0-9]+ \+ \^', ra_rule):
      return (0, 0)
    ra_offset = -1 * int(ra_rule.split()[1], 10)
  return (cfa_offset, ra_offset)


def _GetAllCfiRows(symbol_file):
  """Returns parsed CFI data from given symbol_file.

  Each entry in the cfi data dictionary returned is a map from function start
  address to array of function rows, starting with FUNCTION type, followed by
  one or more CFI rows.
  """
  cfi_data = {}
  current_func = []
  for line in symbol_file:
    if 'STACK CFI' not in line:
      continue

    parts = line.split()
    data = {}
    if parts[2] == 'INIT':
      # Add the previous function to the output
      if len(current_func) > 1:
        cfi_data[current_func[0][_ADDR_ENTRY]] = current_func
      current_func = []

      # The function line is of format "STACK CFI INIT <addr> <length> ..."
      data[_ADDR_ENTRY] = int(parts[3], 16)
      data[_LENGTH_ENTRY] = int(parts[4], 16)
      if data[_LENGTH_ENTRY] == 0:
        continue  # Skip the current function.
    else:
      # The current function is skipped.
      if len(current_func) == 0:
        continue

      # The CFI row is of format "STACK CFI <addr> .cfa: <expr> .ra: <expr> ..."
      data[_ADDR_ENTRY] = int(parts[2], 16)
      (data[_CFA_REG], data[_RA_REG]) = _GetCfaAndRaOffset(parts)
      if (data[_CFA_REG]) == 0 or data[_RA_REG] >= 16:
        current_func = []
        continue
      assert data[_CFA_REG] % 4 == 0

    if data[_ADDR_ENTRY] == 0:
      # Skip current function, delete all previous entries.
      current_func = []
      continue
    assert data[_ADDR_ENTRY] % 2 == 0
    current_func.append(data)

  if len(current_func) > 1:
    cfi_data[current_func[0][_ADDR_ENTRY]] = current_func
  return cfi_data


def _WriteCfiData(cfi_data, out_file):
  """Writes the CFI data in defined format to out_file."""
  for addr, function in sorted(cfi_data.iteritems()):
    assert len(function) > 1
    _Write4Bytes(out_file, addr | 1)
    _Write4Bytes(out_file, function[0][_LENGTH_ENTRY])
    for row in function[1:]:
      _Write4Bytes(out_file, row[_ADDR_ENTRY])
      _Write4Bytes(out_file, (row[_CFA_REG]) | (row[_RA_REG] / 4))


def _ParseCfiData(sym_file, output_path):
  with open(sym_file, 'r') as f:
    cfi_data =  _GetAllCfiRows(f)

  with open(output_path, 'wb') as out_file:
    _WriteCfiData(cfi_data, out_file)


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument(
      '--input_path', required=True,
      help='The input path of the unstripped binary')
  parser.add_argument(
      '--output_path', required=True,
      help='The path of the output file')
  parser.add_argument(
      '--dump_syms_path', required=True,
      help='The path of the dump_syms binary')

  args = parser.parse_args()

  sym_file = tempfile.NamedTemporaryFile()
  with tempfile.NamedTemporaryFile() as sym_file:
    out = subprocess.call(
        ['./' +args.dump_syms_path, args.input_path], stdout=sym_file)
    assert not out
    sym_file.flush()
    _ParseCfiData(sym_file.name, args.output_path)
  return 0

if __name__ == '__main__':
  sys.exit(main())
