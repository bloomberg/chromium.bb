#!/usr/bin/env python
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import os
import subprocess
import sys
import tempfile
import zipfile

current_path = os.path.dirname(os.path.realpath(__file__))
sys.path.insert(0, os.path.join(current_path, "..", "..", "..", "tools"))
# pylint: disable=F0401
import find_depot_tools

if not sys.platform.startswith("linux"):
  print "Not supported for your platform"
  sys.exit(0)

prebuilt_file_path = os.path.join(current_path, "prebuilt")
stamp_path = os.path.join(prebuilt_file_path, "VERSION")

depot_tools_path = find_depot_tools.add_depot_tools_to_path()
gsutil_exe = os.path.join(depot_tools_path, "third_party", "gsutil", "gsutil")

def download():
  version_path = os.path.join(current_path, "../VERSION")
  with open(version_path) as version_file:
    version = version_file.read().strip()

  try:
    with open(stamp_path) as stamp_file:
      current_version = stamp_file.read().strip()
      if current_version == version:
        return 0  # Already have the right version.
  except IOError:
    pass  # If the stamp file does not exist we need to download a new binary.

  platform = "linux-x64" # TODO: configurate
  basename = platform + ".zip"

  gs_path = "gs://mojo/shell/" + version + "/" + basename

  with tempfile.NamedTemporaryFile() as temp_zip_file:
    subprocess.check_call([gsutil_exe, "--bypass_prodaccess",
                           "cp", gs_path, temp_zip_file.name])
    with zipfile.ZipFile(temp_zip_file.name) as z:
      zi = z.getinfo("mojo_shell")
      mode = zi.external_attr >> 16L
      z.extract(zi, prebuilt_file_path)
      os.chmod(os.path.join(prebuilt_file_path, "mojo_shell"), mode)

  with open(stamp_path, 'w') as stamp_file:
    stamp_file.write(version)
  return 0

def main():
  parser = argparse.ArgumentParser(description="Download mojo_shell binary "
      "from google storage")
  parser.parse_args()
  return download()

if __name__ == "__main__":
  sys.exit(main())
