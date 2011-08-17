#!/bin/bash
#
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -u
set -e

# Change to directory in which we are located.
SCRIPTDIR=$(readlink -f "$(dirname "$0")")
cd $SCRIPTDIR

PACKAGE_NAME="gcpdriver"

# Clean up any old data completely
rm -rf debian

# Target architecture to same as build host.
if [ "$(uname -m)" = "x86_64" ]; then
  TARGETARCH="amd64"
else
  TARGETARCH="i386"
fi

# Set up the required directory structure
mkdir -p debian/usr/lib/cups/backend
mkdir -p debian/usr/share/ppd/GCP
mkdir -p debian/usr/share/doc/$PACKAGE_NAME
mkdir -p debian/DEBIAN
chmod 755 debian/DEBIAN

# Copy files to the correct location
# These files are copied in by the previous GYP target.
mv postinst debian/DEBIAN
mv postrm debian/DEBIAN
mv prerm debian/DEBIAN
mv GCP-driver.ppd debian/usr/share/ppd/GCP
echo "Architecture:${TARGETARCH}" >> debian/DEBIAN/control
cat control >> debian/DEBIAN/control
mv changelog debian/usr/share/doc/$PACKAGE_NAME/
gzip --best -f debian/usr/share/doc/$PACKAGE_NAME/changelog
mv copyright debian/usr/share/doc/$PACKAGE_NAME/
mv GCP-driver debian/usr/lib/cups/backend

# Set permissions as required to roll package
chmod 755 debian/DEBIAN/postinst
chmod 755 debian/DEBIAN/postrm
chmod 755 debian/DEBIAN/prerm
chmod -R 755 debian/usr
chmod 644 debian/usr/share/doc/$PACKAGE_NAME/*
chmod 644 debian/usr/share/ppd/GCP/GCP-driver.ppd

# Actually roll the package and rename
fakeroot dpkg-deb --build debian
mv debian.deb gcp-driver.deb

# Clean up
rm -rf debian
rm control