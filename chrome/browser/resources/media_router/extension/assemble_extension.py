# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This script generates manifest.json from manifest.yaml.

In the future it may copy other non-JS files into the target
directory, hence the name of the script.
"""

import argparse
import re


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument("--manifest_in")
  parser.add_argument("--output_dir")
  args = parser.parse_args()

  # Copy the manifest file, stripping YAML-style comment lines to
  # produce valid JSON.
  with open(args.output_dir + "/manifest.json", "w") as manifest_out:
    with open(args.manifest_in) as manifest_in:
      for line in manifest_in:
        if not re.match("^ *#", line):
          manifest_out.write(line)


main()
