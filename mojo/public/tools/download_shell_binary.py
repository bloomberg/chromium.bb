#!/usr/bin/env python
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

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

version_path = os.path.join(current_path, "../VERSION")
with open(version_path) as version_file:
  version = version_file.read().strip()

prebuilt_file_path = os.path.join(current_path, "prebuilt")
stamp_path = os.path.join(prebuilt_file_path, "VERSION")

try:
  with open(stamp_path) as stamp_file:
    current_version = stamp_file.read().strip()
    if current_version == version:
      sys.exit(0)
except IOError:
  pass

platform = "linux-x64" # TODO: configurate
basename = platform + ".zip"

gs_path = "gs://mojo/shell/" + version + "/" + basename

depot_tools_path = find_depot_tools.add_depot_tools_to_path()
gsutil_exe = os.path.join(depot_tools_path, "third_party", "gsutil", "gsutil")

with tempfile.NamedTemporaryFile() as temp_zip_file:
  subprocess.check_call([gsutil_exe, "--bypass_prodaccess",
                         "cp", gs_path, temp_zip_file.name])
  with zipfile.ZipFile(temp_zip_file.name) as z:
    zi = z.getinfo("mojo_shell")
    mode = zi.external_attr >> 16L
    z.extract(zi, prebuilt_file_path)
    os.chmod(os.path.join(prebuilt_file_path, "mojo_shell"), mode)

with open(stamp_path, 'w') as stamp_file:
  stamp_file.write(version + "\n")
