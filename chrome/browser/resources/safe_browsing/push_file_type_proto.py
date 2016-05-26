#!/usr/bin/python
# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Push the {vers}/{plaform}/download_file_types.pb files to GCS so
# that the component update system will pick them up and push them
# to users.  See README.md before running this.

import optparse
import os
import subprocess
import sys


DEST_BUCKET = 'gs://chrome-component-file-type-policies'


def main():
  parser = optparse.OptionParser()
  parser.add_option('-d', '--dir',
                    help='The directory containing '
                    '{vers}/{platform}/download_file_types.pb files.')

  (opts, args) = parser.parse_args()
  if opts.dir is None:
    parser.print_help()
    return 1

  os.chdir(opts.dir)

  # Sanity check that we're in the right place
  assert opts.dir.endswith('/all'), '--dir should end with /all'
  dirs = os.listdir('.')
  assert (len(dirs) == 1 and dirs[0].isdigit()), (
      'Should be exactly one version directory. Please delete the contents '
      'of the target dir and regenerate the protos.')

  # Push the files with their directories, in the form
  #   {vers}/{platform}/download_file_types.pb
  # Don't overwrite existing files, incase we forgot to increment the
  # version.
  vers_dir = dirs[0]
  command = ['gsutil', 'cp', '-Rn', vers_dir, DEST_BUCKET]

  print 'Going to run the following command'
  print '   ', ' '.join(command)
  print '\nIn directory'
  print '   ', opts.dir
  print '\nWhich should push the following files'
  expected_files = [os.path.join(dp, f) for dp, dn, fn in
                    os.walk(vers_dir) for f in fn]
  for f in expected_files:
    print '   ', f

  shall = raw_input('\nAre you sure (y/N) ').lower() == 'y'
  if not shall:
    print 'aborting'
    return 1
  return subprocess.call(command)


if __name__ == '__main__':
  sys.exit(main())

