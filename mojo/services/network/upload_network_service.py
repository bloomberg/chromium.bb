#!/usr/bin/env python
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import imp
import os
import subprocess
import sys
import tempfile
import time
import zipfile

root_path = os.path.realpath(
    os.path.join(
        os.path.dirname(
            os.path.realpath(__file__)),
        os.pardir,
        os.pardir,
        os.pardir))
version = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=root_path)
version = version.strip()

if not sys.platform.startswith("linux"):
  print "Only support linux for now"
  sys.exit(1)

platform = "linux-x64"  # TODO: Parameterize
binary_dest = "gs://mojo/network/" + version + "/" + platform + ".zip"

find_depot_tools_path = os.path.join(root_path, "tools", "find_depot_tools.py")
find_depot_tools = imp.load_source("find_depot_tools", find_depot_tools_path)

depot_tools_path = find_depot_tools.add_depot_tools_to_path()
gsutil_exe = os.path.join(depot_tools_path, "third_party", "gsutil", "gsutil")


def gsutil_cp(source, dest, dry_run):
  if dry_run:
    print "gsutil cp %s %s" % (source, dest)
  else:
    subprocess.check_call([gsutil_exe, "cp", source, dest])


def upload_binary(binary_path, dry_run):
  absolute_binary_path = os.path.join(root_path, binary_path)
  with tempfile.NamedTemporaryFile() as binary_zip_file:
    with zipfile.ZipFile(binary_zip_file, 'w') as z:
      with open(absolute_binary_path) as service_binary:
        zipinfo = zipfile.ZipInfo("libnetwork_service.so")
        zipinfo.external_attr = 0o777 << 16
        zipinfo.compress_type = zipfile.ZIP_DEFLATED
        zipinfo.date_time = time.gmtime(os.path.getmtime(absolute_binary_path))
        z.writestr(zipinfo, service_binary.read())
  gsutil_cp(binary_zip_file.name, binary_dest, dry_run)


def main():
  parser = argparse.ArgumentParser(
      description="Upload network service binary to Google storage")
  parser.add_argument("-n", "--dry-run", action="store_true", help="Dry run")
  parser.add_argument(
      "binary_path",
      help="Path to network service binary relative to the repo root, e.g. " +
      "out/Release/libnetwork_service.so")
  args = parser.parse_args()
  upload_binary(args.binary_path, args.dry_run)
  print "Uploaded artifacts for %s %s" % (version, "linux-x64")
  return 0

if __name__ == '__main__':
  sys.exit(main())
