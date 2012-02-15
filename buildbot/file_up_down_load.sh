#!/bin/bash
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
#@ This script for up/downloading native client toolchains.

#set -o xtrace
set -o nounset
set -o errexit

######################################################################
# Helper
######################################################################

Banner() {
  echo "######################################################################"
  echo $*
  echo "######################################################################"
}


Usage() {
  egrep "^#@" $0 | cut --bytes=3-
}

SanityCheck() {
  Banner "Sanity Checks"
  if [[ $(basename $(pwd)) != "native_client" ]] ; then
    echo "ERROR: run this script from the native_client/ dir"
    exit -1
  fi
}

######################################################################
# Config
######################################################################

readonly GS_UTIL=${GS_UTIL:-buildbot/gsutil.sh}

# raw data store url
readonly URL_PREFIX_RAW=https://commondatastorage.googleapis.com
# appengine app that constructs a synthetic directory listing
# for the data store
readonly URL_PREFIX_UI=http://gsdview.appspot.com
readonly BASE_ARM_TOOLCHAIN=nativeclient-archive2/toolchain

readonly BASE_BETWEEN_BOTS_TRY=nativeclient-trybot/between_builders
readonly BASE_BETWEEN_BOTS=nativeclient-archive2/between_builders


######################################################################
# UTIL
######################################################################
Upload() {
  echo  "uploading: $2"
  echo  @@@STEP_LINK@download@${URL_PREFIX_UI}/$2@@@
  ${GS_UTIL} -h Cache-Control:no-cache cp -a public-read $1 gs://$2
}


UploadArmToolchain() {
  rev=$1
  name=$2
  tarball=$3

  Upload ${tarball} ${BASE_ARM_TOOLCHAIN}/${rev}/${name}
}

######################################################################
# ARM TRUSTED
######################################################################

UploadArmTrustedToolchain() {
  rev=$1
  tarball=$2

  UploadArmToolchain ${rev} naclsdk_linux_arm-trusted.tgz ${tarball}
}

DownloadArmTrustedToolchain() {
  local rev=$1
  local tarball=$2
  curl -L \
     ${URL_PREFIX_RAW}/${BASE_ARM_TOOLCHAIN}/${rev}/naclsdk_linux_arm-trusted.tgz
     -o $2
}

ShowRecentArmTrustedToolchains() {
   local url=gs://${BASE_ARM_TOOLCHAIN}/*/naclsdk_linux_arm-trusted.tgz
   local recent=$(${GS_UTIL} ls ${url} | tail -5)
   for url in ${recent} ; do
     if ${GS_UTIL} ls -L "${url}" ; then
       echo "====="
     fi
   done
}

######################################################################
# ARM UN-TRUSTED
######################################################################

#@ label should be in :
#@
#@ pnacl_linux_i686
#@ pnacl_linux_x86_64
#@ pnacl_darwin_i386
#@ pnacl_windows_i686

UploadArmUntrustedToolchains() {
  local rev=$1
  local label=$2
  local tarball=$3

  UploadArmToolchain ${rev} naclsdk_${label}.tgz ${tarball}
}

DownloadArmUntrustedToolchains() {
  local rev=$1
  local label=$2
  local tarball=$3

   curl -L \
     ${URL_PREFIX}/${BASE_ARM_TOOLCHAIN}/${rev}/naclsdk_${label}.tgz
     -o $2
}

ShowRecentArmUntrustedToolchains() {
  local label=$1
  local url="gs://${BASE_ARM_TOOLCHAIN}/*/naclsdk_${label}.tgz"

  local recent=$(${GS_UTIL} ls ${url} | tail -5)
  for url in ${recent} ; do
    if ${GS_UTIL} ls -L "${url}" ; then
      echo "====="
    fi
  done
}

######################################################################
# ARM BETWEEN BOTS
######################################################################

UploadArmBinariesForHWBots() {
  name=$1
  tarball=$2
  Upload ${tarball} ${BASE_BETWEEN_BOTS}/${name}/$(basename ${tarball})
}


DownloadArmBinariesForHWBots() {
  name=$1
  tarball=$2
  curl -L \
     ${URL_PREFIX_RAW}/${BASE_BETWEEN_BOTS}/${name}/$(basename ${tarball}) \
     -o ${tarball}
}

######################################################################
# ARM BETWEEN BOTS TRY
######################################################################

UploadArmBinariesForHWBotsTry() {
  name=$1
  tarball=$2
  Upload ${tarball} ${BASE_BETWEEN_BOTS_TRY}/${name}/$(basename ${tarball})
}


DownloadArmBinariesForHWBotsTry() {
  name=$1
  tarball=$2
  curl -L \
     ${URL_PREFIX_RAW}/${BASE_BETWEEN_BOTS_TRY}/${name}/$(basename ${tarball})\
     -o ${tarball}
}

######################################################################
# DISPATCH
######################################################################
SanityCheck

if [[ $# -eq 0 ]] ; then
  echo "you must specify a mode on the commandline:"
  echo
  Usage
  exit -1
elif [[ "$(type -t $1)" != "function" ]]; then
  echo "ERROR: unknown function '$1'." >&2
  echo "For help, try:"
  echo "    $0 help"
  exit 1
else
  "$@"
fi
