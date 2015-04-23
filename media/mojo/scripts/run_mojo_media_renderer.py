#!/usr/bin/env python
# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# The script launches mojo_shell to test the mojo media renderer.

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


def _BuildShellCommand(args):
  sdk_version = subprocess.check_output(["cat",
      "third_party/mojo/src/mojo/public/VERSION"], cwd=root_path)
  build_dir = os.path.join(root_path, args.build_dir)

  shell_command = [os.path.join(build_dir, "mojo_shell")]

  options = ["--enable-mojo-media-renderer"]
  if args.verbose:
    options.append("--vmodule=pipeline*=3,*renderer_impl*=3,"
                   "*mojo_demuxer*=3,mojo*service=3")

  full_command = shell_command + options + [args.url]

  if args.verbose:
    print full_command

  return full_command


def main():
  parser = argparse.ArgumentParser(
      description="View a URL with HTMLViewer with mojo media renderer. "
                  "You must have built //mojo, //components/html_viewer, "
                  "//mojo/services/network and //media/mojo/services first.")
  parser.add_argument(
      "--build-dir",
      help="Path to the dir containing the linux-x64 binaries relative to the "
           "repo root (default: %(default)s)",
      default="out/Release")
  parser.add_argument("--verbose", help="Increase output verbosity.",
                      action="store_true")
  parser.add_argument("url", help="The URL to be viewed")

  args = parser.parse_args()
  return subprocess.call(_BuildShellCommand(args))

if __name__ == '__main__':
  sys.exit(main())
