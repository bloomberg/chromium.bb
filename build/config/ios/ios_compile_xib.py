# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


import argparse
import os
import subprocess
import sys


def main():
  parser = argparse.ArgumentParser(
      description='A script to compile xib and storyboard.',
      fromfile_prefix_chars='@')
  parser.add_argument('-o', '--output', required=True,
                      help='Path to output bundle.')
  parser.add_argument('-i', '--input', required=True,
                      help='Path to input xib or storyboard.')
  parser.add_argument('-m', '--minimum-deployment-target', required=True,
                      help='Minimum deployment target.')
  args = parser.parse_args()

  subprocess.check_call([
      'xcrun', 'ibtool', '--errors', '--warnings', '--notices',
      '--auto-activate-custom-fonts', '--target-device', 'iphone',
      '--target-device', 'ipad', '--output-format', 'human-readable-text',
      '--minimum-deployment-target', args.minimum_deployment_target,
      '--compile', os.path.abspath(args.output), os.path.abspath(args.input),
  ])


if __name__ == '__main__':
  sys.exit(main())
