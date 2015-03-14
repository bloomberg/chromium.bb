#!/usr/bin/env python
# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# The script follows mojo/services/html_viewer/view_url.py and modified it for
# test the mojo media renderer. The page will be rendered in a headless mode.
#
# TODO(xhwang): Explore the possibility of running this with the Kiosk window
# manager. See http://crbug.com/467176

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
  options.append("--url-mappings=mojo:html_viewer=file://%s/html_viewer.mojo,"
                 "mojo:media=file://%s/media.mojo" % (build_dir, build_dir))

  args_for_html_viewer = "--enable-mojo-media-renderer --is-headless "
  if args.verbose:
    args_for_html_viewer += \
        "--vmodule=pipeline*=3,*renderer_impl*=3,*mojo_demuxer*=3"
  options.append("--args-for=mojo:html_viewer %s" % args_for_html_viewer)

  if args.verbose:
    args_for_media = "--vmodule=mojo*service=3"
    options.append("--args-for=mojo:media %s" % args_for_media)

  full_command = shell_command + options + [args.url]

  if args.verbose:
    print full_command

  return full_command

def main():
  parser = argparse.ArgumentParser(
      description="View a URL with HTMLViewer with mojo media renderer. "
                  "You must have built //mojo/services/html_viewer, "
                  "//mojo/services/network and //media/mojo/services first. "
                  " Note that this will currently often fail spectacularly due "
                  " to lack of binary stability in Mojo.")
  parser.add_argument(
      "--build-dir",
      help="Path to the dir containing the linux-x64 binaries relative to the "
           "repo root (default: %(default)s)",
      default="out/Release")
  parser.add_argument("--verbose", help="Increase output verbosity.",
                      action="store_true")
  parser.add_argument("url",
                      help="The URL to be viewed")

  args = parser.parse_args()
  return subprocess.call(_BuildShellCommand(args))

if __name__ == '__main__':
  sys.exit(main())
