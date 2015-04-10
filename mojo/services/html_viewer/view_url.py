#!/usr/bin/env python
# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

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

  options = []
  options.append(
      "--origin=https://storage.googleapis.com/mojo/services/linux-x64/%s" %
           sdk_version)
  options.append("--url-mappings=mojo:html_viewer=file://%s/html_viewer.mojo" %
                 build_dir)
  options.append('--args-for=mojo:kiosk_wm %s' % args.url)

  app_to_run = "mojo:kiosk_wm"

  return shell_command + options + [app_to_run]

def main():
  parser = argparse.ArgumentParser(
      description="View a URL with HTMLViewer in the Kiosk window manager. "
                  "You must have built //mojo/services/html_viewer and "
                  "//mojo/services/network first. Note that this will "
                  "currently often fail spectacularly due to lack of binary "
                  "stability in Mojo.")
  parser.add_argument(
      "--build-dir",
      help="Path to the dir containing the linux-x64 binaries relative to the "
           "repo root (default: %(default)s)",
      default="out/Release")
  parser.add_argument("url",
                      help="The URL to be viewed")

  args = parser.parse_args()
  return subprocess.call(_BuildShellCommand(args))

if __name__ == '__main__':
  sys.exit(main())
