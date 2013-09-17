#!/usr/bin/python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script can either source a file and dump the enironment changes done by
# it, or just simply dump the current environment as JSON into a file.

import json
import optparse
import os
import pipes
import subprocess
import sys


def main():
  parser = optparse.OptionParser()
  parser.add_option('-f', '--output-json',
                    help='File to dump the environment as JSON into.')
  parser.add_option(
      '-d', '--dump-mode', action='store_true',
      help='Dump the environment to the file and exit immediately.')

  options, args = parser.parse_args()
  if not options.output_json:
    parser.error('Requires --output-json option.')

  if options.dump_mode:
    if args:
      parser.error("Cannot specify args with --dump-mode")
    with open(options.output_json, 'w') as f:
      json.dump(dict(os.environ), f)
  else:
    envsetup_cmd = ' '.join(map(pipes.quote, args))
    full_cmd = [
        'bash', '-c',
        '. %s; %s -d -f %s' % (envsetup_cmd, os.path.abspath(__file__),
                               options.output_json)
    ]
    ret = subprocess.call(full_cmd)
    if ret:
      sys.exit('Error running %s and dumping env' % envsetup_cmd)


if __name__ == '__main__':
  sys.exit(main())
