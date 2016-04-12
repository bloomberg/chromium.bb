#!/usr/bin/python
# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
 Convert the ASCII download_file_types.asciipb proto into a binary resource.
"""

import optparse
import sys

import google.protobuf.text_format as text_format

def ConvertProto(opts):

  # Find the proto code
  sys.path.append(opts.path)
  import download_file_types_pb2 as config_pb2

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
  parser.add_option('-i', '--infile',
                    help='The ASCII DownloadFileType-proto file to read.')
  parser.add_option('-o', '--outfile',
                    help='The binary file to write.')
  parser.add_option('-p', '--path',
                    help='A directory path containing the '+
                    'download_file_types_pb2.py module')
  (opts, args) = parser.parse_args()
  if opts.infile is None or opts.outfile is None or opts.path is None:
    parser.print_help()
    return 1

  try:
    ConvertProto(opts)
  except Exception as e:
    print "ERROR: %s failed to parse ASCII proto\n%s: %s\n" % (
        sys.argv[0], opts.infile, str(e))
    return 1

if __name__ == '__main__':
  sys.exit(main())
