#!/usr/bin/env python
# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generates a new manifest.json for Canary, Dev builds with postfixed name and
git hash included.

This script takes a manifest.json file as input and generates a new one with
postfixed name and git hash included.
"""

import argparse
import json
import os
import subprocess
import sys


CWD = os.path.dirname(os.path.realpath(__file__))

SRC_MANIFEST = CWD + '/../src/manifest.json'
CANARY_MANIFEST = CWD + '/../build/camera-canary/manifest.json'
DEV_MANIFEST = CWD + '/../build/camera-dev/manifest.json'


def GetGitHash():
  return subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=CWD).strip()


def main(argv):
  parser = argparse.ArgumentParser()
  group = parser.add_mutually_exclusive_group(required=True)
  group.add_argument('--canary',
                     action='store_true',
                     help='Generate Canary manifest')
  group.add_argument('--dev',
                     action='store_true',
                     help='Generate Dev manifest')
  args = parser.parse_args(argv)

  with open(SRC_MANIFEST, 'r') as f:
    manifest = json.load(f)

  if args.canary:
    output_manifest = CANARY_MANIFEST
  elif args.dev:
    output_manifest = DEV_MANIFEST

  manifest['version_name'] = (manifest['version'] +
                                ' (' + GetGitHash()[:8] + ')')
  with open(output_manifest, 'w') as f:
    f.write(json.dumps(manifest, indent=2))
  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
