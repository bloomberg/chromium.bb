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

if not sys.platform.startswith("linux"):
  print "Only support linux for now"
  sys.exit(1)

root_path = os.path.realpath(
    os.path.join(
        os.path.dirname(
            os.path.realpath(__file__)),
        os.pardir,
        os.pardir,
        os.pardir))
version = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=root_path)
version = version.strip()

find_depot_tools_path = os.path.join(root_path, "tools", "find_depot_tools.py")
find_depot_tools = imp.load_source("find_depot_tools", find_depot_tools_path)

depot_tools_path = find_depot_tools.add_depot_tools_to_path()
gsutil_exe = os.path.join(depot_tools_path, "third_party", "gsutil", "gsutil")


def gsutil_cp(source, dest, dry_run):
  if dry_run:
    print "gsutil cp %s %s" % (source, dest)
  else:
    subprocess.check_call([gsutil_exe, "cp", source, dest])


def upload_mojoms(dry_run):
  absolute_mojom_directory_path = os.path.join(
      os.path.dirname(os.path.realpath(__file__)),
      "public",
      "interfaces")
  dest = "gs://mojo/network/" + version + "/" + "mojoms.zip"
  with tempfile.NamedTemporaryFile() as mojom_zip_file:
    with zipfile.ZipFile(mojom_zip_file, 'w') as z:
      for root, _, files in os.walk(absolute_mojom_directory_path):
        for filename in files:
          absolute_file_path = os.path.join(root, filename)
          relative_file_path = os.path.relpath(absolute_file_path, root)
          z.write(absolute_file_path, relative_file_path)
    gsutil_cp(mojom_zip_file.name, dest, dry_run)


def upload_binary(binary_path, platform, dry_run):
  absolute_binary_path = os.path.join(root_path, binary_path)
  binary_dest = "gs://mojo/network/" + version + "/" + platform + ".zip"
  with tempfile.NamedTemporaryFile() as binary_zip_file:
    with zipfile.ZipFile(binary_zip_file, 'w') as z:
      with open(absolute_binary_path) as service_binary:
        zipinfo = zipfile.ZipInfo("network_service.mojo")
        zipinfo.external_attr = 0o777 << 16
        zipinfo.compress_type = zipfile.ZIP_DEFLATED
        zipinfo.date_time = time.gmtime(os.path.getmtime(absolute_binary_path))
        z.writestr(zipinfo, service_binary.read())
    gsutil_cp(binary_zip_file.name, binary_dest, dry_run)


def main():
  parser = argparse.ArgumentParser(
      description="Upload network service mojoms and binaries to Google " +
                  "storage")
  parser.add_argument("-n", "--dry-run", action="store_true", help="Dry run")
  parser.add_argument(
      "--linux-x64-binary-path",
      help="Path to the linux-x64 network service binary relative to the " +
           "repo root, e.g. out/Release/network_service.mojo")
  parser.add_argument(
      "--android-arm-binary-path",
      help="Path to the android-arm network service binary relative to the " +
           "repo root, e.g. out/android_Release/network_service.mojo")

  args = parser.parse_args()
  upload_mojoms(args.dry_run)
  if args.linux_x64_binary_path:
    upload_binary(args.linux_x64_binary_path, "linux-x64", args.dry_run)
  if args.android_arm_binary_path:
    upload_binary(args.android_arm_binary_path, "android-arm", args.dry_run)

  if not args.dry_run:
    print "Uploaded artifacts for version %s" % (version, )
  else:
    print "No artifacts uploaded (dry run)"
  return 0

if __name__ == '__main__':
  sys.exit(main())
