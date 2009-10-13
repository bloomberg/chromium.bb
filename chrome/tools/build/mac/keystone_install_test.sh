#!/bin/bash

# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Test of the Mac Chrome installer.


# Where I am
DIR=$(dirname "${0}")

# My installer to test
INSTALLER="${DIR}"/keystone_install.sh
if [ ! -f "${INSTALLER}" ]; then
  echo "Can't find scripts." >& 2
  exit 1
fi

# What I test
APPNAME="Google Chrome.app"

# Temp directory to be used as the disk image (source)
TEMPDIR=$(mktemp -d ${TMPDIR}/$(basename ${0}).XXXXXX)
PATH=$PATH:"${TEMPDIR}"

# Clean up the temp directory
function cleanup_tempdir() {
  chmod u+w "${TEMPDIR}"
  rm -rf "${TEMPDIR}"
}

# Run the installer and make sure it fails.
# If it succeeds, we fail.
# Arg0: string to print
function fail_installer() {
  echo $1
  "${INSTALLER}" "${TEMPDIR}" >/dev/null 2>&1
  RETURN=$?
  if [ $RETURN -eq 0 ]; then
    echo "Did not fail (which is a failure)" >& 2
    cleanup_tempdir
    exit 1
  else
    echo "Returns $RETURN"
  fi
}

# Make sure installer works!
# Arg0: string to print
function pass_installer() {
  echo $1
  "${INSTALLER}" "${TEMPDIR}" >/dev/null 2>&1
  RETURN=$?
  if [ $RETURN -ne 0 ]; then
    echo "FAILED; returned $RETURN but should have worked" >& 2
    cleanup_tempdir
    exit 1
  else
    echo "worked"
  fi
}

# Most of the setup for an install source and dest
function make_basic_src_and_dest() {
  chmod ugo+w "${TEMPDIR}"
  rm -rf "${TEMPDIR}"
  mkdir -p "${TEMPDIR}/${APPNAME}/Contents"
  defaults write "${TEMPDIR}/${APPNAME}/Contents/Info" \
    KSProductID "com.google.Chrome"
  defaults write "${TEMPDIR}/${APPNAME}/Contents/Info" KSVersion 1
  DEST="${TEMPDIR}"/Dest.app
  mkdir -p "${DEST}"/Contents
  defaults write "${DEST}/Contents/Info" KSVersion 0
  cat >"${TEMPDIR}"/ksadmin <<EOF
#!/bin/sh
echo "echo xc=<blah path=$DEST>"
exit 0
EOF
  chmod u+x "${TEMPDIR}"/ksadmin
}

fail_installer "No source anything"

mkdir "${TEMPDIR}"/"${APPNAME}"
fail_installer "No source bundle"

make_basic_src_and_dest
chmod ugo-w "${TEMPDIR}"
fail_installer "Writable dest directory"

make_basic_src_and_dest
fail_installer "Was no KSUpdateURL in dest after copy"

make_basic_src_and_dest
defaults write "${TEMPDIR}/${APPNAME}/Contents/Info" \
  KSUpdateURL "http://foo.bar"
pass_installer "ALL"

cleanup_tempdir
