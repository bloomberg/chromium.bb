#!/usr/bin/env python
# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


import argparse
import json
import sys

from util import build_utils


def ParseArgs():
  parser = argparse.ArgumentParser()
  parser.add_argument('--configuration-name', required=True,
                      help='The build CONFIGURATION_NAME.')
  parser.add_argument('--enabled-configurations', default=[],
                      help='The configuration(s) for which multidex should be '
                           'enabled. If not specified and --enable-multidex is '
                           'passed, multidex will be enabled for all '
                            'configurations.')
  parser.add_argument('--multidex-configuration-path', required=True,
                      help='The path to which the multidex configuration JSON '
                           'should be saved.')

  args = parser.parse_args()

  if args.enabled_configurations:
    args.enabled_configurations = build_utils.ParseGypList(
        args.enabled_configurations)

  return args


def main():
  args = ParseArgs()

  multidex_enabled = (
      (not args.enabled_configurations
       or args.configuration_name in args.enabled_configurations))

  config = {
    'enabled': multidex_enabled,
  }

  with open(args.multidex_configuration_path, 'w') as f:
    f.write(json.dumps(config))

  return 0


if __name__ == '__main__':
  sys.exit(main())

