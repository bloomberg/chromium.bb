#!/usr/bin/python
# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
 Convert the ASCII download_file_types.asciipb proto into a binary resource.
"""

import optparse
import subprocess
import sys


def ConvertProto(opts):
  # Find the proto code
  for path in opts.path:
    # Put the path to our proto libraries in front, so that we don't use system
    # protobuf.
    sys.path.insert(1, path)


  import download_file_types_pb2 as config_pb2
  import google.protobuf.text_format as text_format

  # Read the ASCII
  ifile = open(opts.infile, 'r')
  ascii_pb_str = ifile.read()
  ifile.close()

  # Parse it into a structure PB
  pb = config_pb2.DownloadFileTypeConfig()
  text_format.Merge(ascii_pb_str, pb)

  # Serialize it
  binary_pb_str = pb.SerializeToString()

  # Write it to disk
  open(opts.outfile, 'w').write(binary_pb_str)


def main():
  parser = optparse.OptionParser()
  # TODO(nparker): Remove this once the bug is fixed.
  parser.add_option('-w', '--wrap', action="store_true", default=False,
                     help='Wrap this script in another python '
                     'execution to disable site-packages.  This is a '
                     'fix for http://crbug.com/605592')
  parser.add_option('-i', '--infile',
                    help='The ASCII DownloadFileType-proto file to read.')
  parser.add_option('-o', '--outfile',
                    help='The binary file to write.')
  parser.add_option('-p', '--path', action="append",
                    help='Repeat this as needed.  Directory(s) containing ' +
                    'the download_file_types_pb2.py and ' +
                    'google.protobuf.text_format modules')
  (opts, args) = parser.parse_args()
  if opts.infile is None or opts.outfile is None:
    parser.print_help()
    return 1

  if opts.wrap:
    # Run this script again with different args to the interpreter.
    command = [sys.executable, '-S', '-s', sys.argv[0]]
    command += ['-i', opts.infile]
    command += ['-o', opts.outfile]
    for path in opts.path:
      command += ['-p', path]
    sys.exit(subprocess.call(command))

  try:
    ConvertProto(opts)
  except Exception as e:
    print "ERROR: %s failed to parse ASCII proto\n%s: %s\n" % (
        sys.argv, opts.infile, str(e))
    return 1

if __name__ == '__main__':
  sys.exit(main())
