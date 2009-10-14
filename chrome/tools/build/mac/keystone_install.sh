#!/bin/bash

# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Called by the Keystone system to update the installed application with a new
# version from a disk image.

# Return values:
# 0  Happiness
# 1  Unknown failure
# 2  Basic sanity check destination failure (e.g. ticket points to nothing)
# 3  Cannot get version of currently installed Chrome
# 4  No permission to write in destination directory
# 5  rsync failed
# 6  Cannot get version or update URL of newly installed Chrome
# 7  Post-install Chrome has same version as pre-install Chrome
# 8  ksadmin failure
# 10 Basic sanity check source failure (e.g. no app on disk image)

set -e

# The argument should be the disk image path.  Make sure it exists.
if [ $# -lt 1 ] || [ ! -d "${1}" ]; then
  exit 10
fi

# Who we are.
PRODUCT_NAME="Google Chrome"
APP_NAME="${PRODUCT_NAME}.app"
FRAMEWORK_NAME="${PRODUCT_NAME} Framework.framework"
SRC="${1}/${APP_NAME}"

# Sanity, make sure that there's something to copy from.
if [ -z "${SRC}" ] || [ ! -d "${SRC}" ]; then
  exit 10
fi

# Figure out where we're going.  Determine the application version to be
# installed, use that to locate the framework, and then look inside the
# framework for the Keystone product ID.
APP_VERSION_KEY="CFBundleShortVersionString"
UPD_VERSION_APP=$(defaults read "${SRC}/Contents/Info" "${APP_VERSION_KEY}" ||
                  exit 10)
UPD_KS_PLIST="${SRC}/Contents/Versions/${UPD_VERSION_APP}/${FRAMEWORK_NAME}/Resources/Info"
PRODUCT_ID=$(defaults read "${UPD_KS_PLIST}" KSProductID || exit 10)
DEST=$(ksadmin -pP "${PRODUCT_ID}" | grep xc= | sed -E 's/.+path=(.+)>$/\1/g')

# More sanity checking.
if [ -z "${DEST}" ] || [ ! -d "$(dirname "${DEST}")" ]; then
  exit 2
fi

# Read old version to help confirm install happiness.  Older versions kept
# the KSVersion key in the application's Info.plist.  Newer versions keep it
# in the versioned framework's Info.plist.
KS_VERSION_KEY="KSVersion"
OLD_VERSION_APP=$(defaults read "${DEST}/Contents/Info" "${APP_VERSION_KEY}" ||
                  defaults read "${DEST}/Contents/Info" "${KS_VERSION_KEY}" ||
                  exit 3)
OLD_KS_PLIST="${DEST}/Contents/Versions/${OLD_VERSION_APP}/${FRAMEWORK_NAME}/Resources/Info"
if [ ! -e "${OLD_KS_PLIST}.plist" ] ; then
  OLD_KS_PLIST="${DEST}/Contents/Info"
fi
OLD_VERSION_KS=$(defaults read "${OLD_KS_PLIST}" "${KS_VERSION_KEY}" || exit 3)

# Make sure we have permission to write the destination
DEST_DIRECTORY="$(dirname "${DEST}")"
if [ ! -w "${DEST_DIRECTORY}" ]; then
  exit 4
fi

# This usage will preserve any changes the user made to the application name.
# TODO(jrg): this may choke a running Chrome.app; be smarter.
# Note:  If the rsync fails we do not update the ticket version.
rsync -ac --delete "${SRC}/" "${DEST}/" || exit 5

# Read the new values (e.g. version).  Get the installed application version
# to get the path to the framework, where the Keystone keys are stored.
NEW_VERSION_APP=$(defaults read "${DEST}/Contents/Info" "${APP_VERSION_KEY}" ||
                  exit 6)
NEW_KS_PLIST="${DEST}/Contents/Versions/${NEW_VERSION_APP}/${FRAMEWORK_NAME}/Resources/Info"
NEW_VERSION_KS=$(defaults read "${NEW_KS_PLIST}" "${KS_VERSION_KEY}" || exit 6)
URL=$(defaults read "${NEW_KS_PLIST}" KSUpdateURL || exit 6)
# The channel ID is optional.  Suppress stderr to prevent Keystone from seeing
# possible error output.
CHANNEL_ID=$(defaults read "${NEW_KS_PLIST}" KSChannelID 2>/dev/null || true)

# Compare old and new versions.  If they are equal we failed somewhere.
if [ "${OLD_VERSION_KS}" = "${NEW_VERSION_KS}" ]; then
  exit 7
fi

# Notify LaunchServices.
/System/Library/Frameworks/CoreServices.framework/Frameworks/LaunchServices.framework/Support/lsregister "${DEST}"

# Notify Keystone.  Older versions of Keystone don't recognize --tag.  If the
# command with --tag fails, retry without it.  In that case, Chrome will set
# the tag when it runs.
# TODO: The version of Keystone picking up --tag will also include support for
# --ksdamin-version.  At that point, we can check to see if ksadmin honors the
# version check; if not, no --tag, if yes, do a case...esac on the version
# patterns for any support checks we need.
ksadmin --register \
        -P "${PRODUCT_ID}" \
        --version "${NEW_VERSION_KS}" \
        --xcpath "${DEST}" \
        --url "${URL}" \
        --tag "${CHANNEL_ID}" || \
  ksadmin --register \
          -P "${PRODUCT_ID}" \
          --version "${NEW_VERSION_KS}" \
          --xcpath "${DEST}" \
          --url "${URL}" || exit 8

# Great success!
exit 0
