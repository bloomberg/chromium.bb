#!/bin/bash
#
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -u
set -e

# Change to directory in which we are located.
SCRIPTDIR="$( cd "$( dirname "$0" )" && pwd )"
cd $SCRIPTDIR

#### INSTALLER #####

# Set up the directory structure for the package
mkdir -p Resources
mkdir -p package_root/usr/libexec/cups/backend

# Move out files to the directory structure
# These files are copied in by the GYP file, so
# we move them.
mv GCP-driver package_root/usr/libexec/cups/backend
mv GCP-driver.ppd Resources
mv GCP-install Resources
mv install.sh Resources

# Actually build our package
/Developer/usr/bin/packagemaker \
  -d GCP-Virtual-Driver.pmdoc -t GCP-virtual-driver



##### UNINSTALLER #####

#Clean up any old uninstaller
rm -rf uninstall_resources
mkdir uninstall_resources
mv GCP-uninstall uninstall_resources
mv uninstall.sh uninstall_resources


##### CLEANUP #####
rm -rf Resources
rm -rf package_root
rm -rf GCP-Virtual-Driver.pmdoc
rm build_mac.sh