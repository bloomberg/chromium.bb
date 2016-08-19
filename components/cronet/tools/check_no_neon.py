#!/usr/bin/python
# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""check_no_neon.py - Check modules do not contain ARM Neon instructions."""

import argparse
import os
import sys


def main(args):
  parser = argparse.ArgumentParser(
      description='Check modules do not contain ARM Neon instructions.')
  parser.add_argument('objdump', metavar='path/to/ARM/objdump')
  parser.add_argument('objects', metavar='files/to/check/*.o')
  opts = parser.parse_args(args)
  return os.system(opts.objdump + ' -d --no-show-raw-insn ' +
      opts.objects + ' | grep -q "vld[1-9]\\|vst[1-9]"')


if __name__ == '__main__':
  sys.exit(0 if main(sys.argv[1:]) != 0 else -1)
