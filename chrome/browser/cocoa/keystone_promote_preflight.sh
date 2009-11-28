#!/bin/bash -p

# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Called as root before Keystone ticket promotion to ensure a suitable
# environment for Keystone installation.  Ultimately, these features should be
# integrated directly into the Keystone installation.
#
# Note that this script will be invoked with the real user ID set to the
# user's ID, but the effective user ID set to 0 (root).  bash -p is used on
# the first line to prevent bash from setting the effective user ID to the
# real user ID (dropping root privileges).
#
# TODO(mark): Remove this script when able.  See http://b/2285921 and
# http://b/2289908.

set -e

# This script runs as root, so be paranoid about things like ${PATH}.
export PATH="/usr/bin:/usr/sbin:/bin:/sbin"

# Output the pid to stdout before doing anything else.  See
# chrome/browser/cocoa/authorization_util.h.
echo "${$}"

if [ ${#} -ne 0 ] ; then
  echo "usage: ${0}" >& 2
  exit 2
fi

OWNER_GROUP="root:admin"
CHMOD_MODE="a+rX,u+w,go-w"

LIB_GOOG="/Library/Google"
if [ -d "${LIB_GOOG}" ] ; then
  # Just work with the directory.  Don't do anything recursively here, so as
  # to leave other things in /Library/Google alone.
  chown -h "${OWNER_GROUP}" "${LIB_GOOG}" >& /dev/null
  chmod -h "${CHMOD_MODE}" "${LIB_GOOG}" >& /dev/null

  LIB_GOOG_GSU="${LIB_GOOG}/GoogleSoftwareUpdate"
  if [ -d "${LIB_GOOG_GSU}" ] ; then
    chown -Rh "${OWNER_GROUP}" "${LIB_GOOG_GSU}" >& /dev/null
    chmod -R "${CHMOD_MODE}" "${LIB_GOOG_GSU}" >& /dev/null

    # On the Mac, or at least on HFS+, symbolic link permissions are
    # significant, but chmod -R and -h can't be used together.  Do another
    # pass to fix the permissions on any symbolic links.
    find "${LIB_GOOG_GSU}" -type l -exec chmod -h "${CHMOD_MODE}" {} + >& \
        /dev/null

    # TODO(mark): If GoogleSoftwareUpdate.bundle is missing, dump TicketStore
    # too?
  fi
fi

exit 0
