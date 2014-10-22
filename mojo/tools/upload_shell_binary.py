#!/usr/bin/env python
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import subprocess
import sys
import tempfile
import time
import zipfile

root_path = os.path.join(os.path.dirname(os.path.realpath(__file__)),
                            "..", "..")
version_path = os.path.join(root_path, "mojo", "public", "VERSION")
version_file = open(version_path)
version = version_file.read().strip()

sys.path.insert(0, os.path.join(root_path, "tools"))
# pylint: disable=F0401
import find_depot_tools


binary_path = os.path.join(root_path, "out", "Release", "mojo_shell")

dest = "gs://mojo/shell/" + version + "/linux-x64.zip"

depot_tools_path = find_depot_tools.add_depot_tools_to_path()
gsutil_exe = os.path.join(depot_tools_path, "third_party", "gsutil", "gsutil")

with tempfile.NamedTemporaryFile() as zip_file:
  with zipfile.ZipFile(zip_file, 'w') as z:
    with open(binary_path) as shell_binary:
      zipinfo = zipfile.ZipInfo("mojo_shell")
      zipinfo.external_attr = 0777 << 16L
      zipinfo.compress_type = zipfile.ZIP_DEFLATED
      zipinfo.date_time = time.gmtime(os.path.getmtime(binary_path))
      z.writestr(zipinfo, shell_binary.read())
  subprocess.check_call([gsutil_exe, "cp", zip_file.name, dest])
