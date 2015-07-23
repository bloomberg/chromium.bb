#!/usr/bin/env python
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Writes canary version information to a specified file."""

import datetime
import sys
import optparse
import os


def WriteIfChanged(file_name, content):
  """
  Write |content| to |file_name| iff the content is different from the
  current content.
  """
  try:
    old_content = open(file_name, 'rb').read()
  except EnvironmentError:
    pass
  else:
    if content == old_content:
      return
    os.unlink(file_name)
  open(file_name, 'wb').write(content)


def main(argv=None):
  if argv is None:
    argv = sys.argv

  parser = optparse.OptionParser(usage="canary_version.py [options]")
  parser.add_option("-o", "--output", metavar="FILE",
                    help="write patch level to FILE")
  opts, args = parser.parse_args(argv[1:])
  out_file = opts.output

  if args and out_file is None:
    out_file = args.pop(0)

  if args:
    sys.stderr.write('Unexpected arguments: %r\n\n' % args)
    parser.print_help()
    sys.exit(2)

  # Number of days since January 1st, 2012.
  day_number = (datetime.date.today() - datetime.date(2012, 1, 1)).days
  content = "PATCH={}\n".format(day_number)

  if not out_file:
    sys.stdout.write(content)
  else:
    WriteIfChanged(out_file, content)

if __name__ == '__main__':
  sys.exit(main())
