#!/usr/bin/env python
#
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generates a manifest with reference attributes removed."""

import argparse
import re


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument(
      '--input', required=True, help='Path to the input manifest.')
  parser.add_argument(
      '--output', required=True, help='Path to the output manifest.')
  args = parser.parse_args()

  with open(args.input) as manifest_file:
    stub_manifest = manifest_file.read()

  stub_manifest = re.sub(
      r'<meta-data[^>]*android:resource[^>]*/>',
      '',
      stub_manifest,
      flags=re.MULTILINE)
  stub_manifest = re.sub(r'android:[^=]*=\s*"@[^"]+"', '', stub_manifest)

  with open(args.output, 'w') as out_file:
    out_file.write(stub_manifest)


if __name__ == '__main__':
  main()
