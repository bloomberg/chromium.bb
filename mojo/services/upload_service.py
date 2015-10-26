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

SERVICES = ["html_viewer", "network_service", "network_service_apptests"]

# A service does not need to expose interfaces. Those that do expose interfaces
# have their mojoms located in the directories listed below, in paths relative
# to the directory of this script.
MOJOMS_IN_DIR = {
    "network_service": os.path.join("network", "public", "interfaces")
}

# The network service is downloaded out-of-band rather than dynamically by the
# shell and thus can be stored zipped in the cloud. Other services are intended
# to be downloaded dynamically by the shell, which doesn't currently understand
# zipped binaries.
SERVICES_WITH_ZIPPED_BINARIES = ["network_service", "network_service_apptests"]

if not sys.platform.startswith("linux"):
  print "Only support linux for now"
  sys.exit(1)

root_path = os.path.realpath(
    os.path.join(
        os.path.dirname(
            os.path.realpath(__file__)),
        os.pardir,
        os.pardir))

find_depot_tools_path = os.path.join(root_path, "build", "find_depot_tools.py")
find_depot_tools = imp.load_source("find_depot_tools", find_depot_tools_path)

depot_tools_path = find_depot_tools.add_depot_tools_to_path()
gsutil_exe = os.path.join(depot_tools_path, "third_party", "gsutil", "gsutil")

def get_version_name(custom_build):
  if custom_build:
    branch = subprocess.check_output(
        ["git", "rev-parse", "--abbrev-ref", "HEAD"], cwd=root_path).strip()
    try:
      base = subprocess.check_output(
          ["git", "config", "--get", "branch." + branch + ".base"],
          cwd=root_path).strip()
      issue = subprocess.check_output(
          ["git", "config", "--get", "branch." + branch + ".rietveldissue"],
          cwd=root_path).strip()
      patchset = subprocess.check_output(
          ["git", "config", "--get", "branch." + branch + ".rietveldpatchset"],
          cwd=root_path).strip()
    except subprocess.CalledProcessError:
      return None

    if not base or not issue or not patchset:
      return None
    else:
      return "custom_build_base_%s_issue_%s_patchset_%s" % (base, issue,
                                                            patchset)
  else:
    return subprocess.check_output(["git", "rev-parse", "HEAD"],
                                   cwd=root_path).strip()

def gsutil_cp(source, dest, dry_run):
  if dry_run:
    print "gsutil cp %s %s" % (source, dest)
  else:
    subprocess.check_call([gsutil_exe, "cp", source, dest])


def upload_mojoms(version_name, service, absolute_mojom_directory_path,
                  dry_run):
  dest = "gs://mojo/" + service + "/" + version_name + "/" + "mojoms.zip"

  with tempfile.NamedTemporaryFile() as mojom_zip_file:
    with zipfile.ZipFile(mojom_zip_file, 'w') as z:
      for root, _, files in os.walk(absolute_mojom_directory_path):
        for filename in files:
          absolute_file_path = os.path.join(root, filename)
          relative_file_path = os.path.relpath(absolute_file_path, root)
          z.write(absolute_file_path, relative_file_path)
    gsutil_cp(mojom_zip_file.name, dest, dry_run)


def upload_binary(version_name, service, binary_dir, platform, dry_run):
  dest_dir = "gs://mojo/" + service + "/" + version_name + "/" + platform + "/"
  should_zip = service in SERVICES_WITH_ZIPPED_BINARIES
  binary_name = service + ".mojo"
  absolute_binary_path = os.path.join(root_path, binary_dir, binary_name)

  if not should_zip:
    # Upload the binary.
    dest = dest_dir + binary_name
    gsutil_cp(absolute_binary_path, dest, dry_run)

    # Update the pointer to the service's location to point to the
    # newly-uploaded binary.
    service_location = dest.replace("gs://", "https://storage.googleapis.com/")
    location_file = ("gs://mojo/services/" + platform + "/" + service +
                     "_location")
    with tempfile.NamedTemporaryFile() as tmp:
      tmp.write(service_location)
      tmp.flush()
      gsutil_cp(tmp.name, location_file, dry_run)
    return

  # Zip the binary before uploading it to the cloud.
  dest = dest_dir + binary_name + ".zip"
  with tempfile.NamedTemporaryFile() as binary_zip_file:
    with zipfile.ZipFile(binary_zip_file, 'w') as z:
      with open(absolute_binary_path) as service_binary:
        zipinfo = zipfile.ZipInfo(binary_name)
        zipinfo.external_attr = 0o777 << 16
        zipinfo.compress_type = zipfile.ZIP_DEFLATED
        zipinfo.date_time = time.gmtime(os.path.getmtime(absolute_binary_path))
        z.writestr(zipinfo, service_binary.read())
    gsutil_cp(binary_zip_file.name, dest, dry_run)


def main():
  parser = argparse.ArgumentParser(
      description="Upload service mojoms and binaries to Google storage")
  parser.add_argument("-n", "--dry-run", action="store_true", help="Dry run")
  parser.add_argument(
      "--linux-x64-binary-dir",
      help="Path to the dir containing the linux-x64 service binary relative "
           "to the repo root, e.g. out/Release")
  parser.add_argument(
      "--android-arm-binary-dir",
      help="Path to the dir containing the android-arm service binary relative "
           "to the repo root, e.g. out/android_Release")
  parser.add_argument("service",
                      help="The service to be uploaded (one of %s)" % SERVICES)
  parser.add_argument(
      "--custom-build", action="store_true",
      help="Indicates that this is a build with change that is not committed. "
           "The change must be uploaded to Rietveld. The script needs to be "
           "run from the branch associated with the change.")
  args = parser.parse_args()

  if args.service not in SERVICES:
    print args.service + " is not one of the recognized services:"
    print SERVICES
    return 1

  version_name = get_version_name(args.custom_build)
  if args.custom_build and not version_name:
    print ("When uploading a custom build, the corresponding change to source "
           "code must be uploaded to Rietveld. Besides, this script needs to "
           "be run from the branch associated with the change.")
    return 1

  if args.service in MOJOMS_IN_DIR:
    script_dir = os.path.dirname(os.path.realpath(__file__))
    absolute_mojom_directory_path = os.path.join(script_dir,
                                                 MOJOMS_IN_DIR[args.service])
    upload_mojoms(version_name, args.service, absolute_mojom_directory_path,
                  args.dry_run)

  if args.linux_x64_binary_dir:
    upload_binary(version_name, args.service, args.linux_x64_binary_dir,
                  "linux-x64", args.dry_run)

  if args.android_arm_binary_dir:
    upload_binary(version_name, args.service, args.android_arm_binary_dir,
                  "android-arm", args.dry_run)

  if not args.dry_run:
    print "Uploaded artifacts for version %s" % (version_name, )
  else:
    print "No artifacts uploaded (dry run)"
  return 0

if __name__ == '__main__':
  sys.exit(main())
