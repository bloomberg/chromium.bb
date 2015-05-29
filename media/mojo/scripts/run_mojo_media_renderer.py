#!/usr/bin/env python
# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# The script launches mandoline to test the mojo media renderer.

import argparse
import os
import subprocess
import sys


root_path = os.path.realpath(
    os.path.join(
        os.path.dirname(
            os.path.realpath(__file__)),
        os.pardir,
        os.pardir,
        os.pardir))


def _BuildCommand(args):
  build_dir = os.path.join(root_path, args.build_dir)

  runner = [os.path.join(build_dir, "mandoline")]

  options = ["--enable-mojo-media-renderer"]
  if args.verbose:
    options.append("--vmodule=pipeline*=3,*renderer_impl*=3,"
                   "*mojo_demuxer*=3,mojo*service=3")

  full_command = runner + options + [args.url]

  if args.verbose:
    print full_command

  return full_command


def main():
  parser = argparse.ArgumentParser(
      description="View a URL with Mandoline with mojo media renderer. "
                  "You must have built //mandoline, //components/html_viewer, "
                  "//mojo, //mojo/services/network and //media/mojo/services "
                  "first.")
  parser.add_argument(
      "--build-dir",
      help="Path to the dir containing the linux-x64 binaries relative to the "
           "repo root (default: %(default)s)",
      default="out/Release")
  parser.add_argument("--verbose", help="Increase output verbosity.",
                      action="store_true")
  parser.add_argument("url", help="The URL to be viewed")

  args = parser.parse_args()
  return subprocess.call(_BuildCommand(args))

if __name__ == '__main__':
  sys.exit(main())
