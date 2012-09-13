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
readonly BASE_TOOLCHAIN_COMPONENT=nativeclient-archive2/toolchain

readonly BASE_BETWEEN_BOTS_TRY=nativeclient-trybot/between_builders
readonly BASE_BETWEEN_BOTS=nativeclient-archive2/between_builders


######################################################################
# UTIL
######################################################################
GetFileSizeK() {
  # Note: this is tricky to make work on win/linux/mac
  du -k $1 | egrep -o "^[0-9]+"
}


Upload() {
  local size_kb=$(GetFileSizeK $1)
  echo  "uploading: $2 (${size_kb}kB)"
  echo  "@@@STEP_LINK@download (${size_kb}kB)@${URL_PREFIX_UI}/$2@@@"
  ${GS_UTIL} cp -a public-read $1 gs://$2
}


UploadToolchainComponent() {
  local rev=$1
  local name=$2
  local tarball=$3

  Upload ${tarball} ${BASE_TOOLCHAIN_COMPONENT}/${rev}/${name}
}

ComputeSha1() {
  # on mac we do not have sha1sum so we fall back to openssl
  if which sha1sum >/dev/null ; then
    echo "$(SHA1=$(sha1sum -b $1) ; echo ${SHA1:0:40})"
  elif which openssl >/dev/null ; then
    echo "$(SHA1=$(openssl sha1 $1) ; echo ${SHA1/* /})"

  else
    echo "ERROR: do not know how to compute SHA1"
    exit 1
  fi
}

######################################################################
# ARM TRUSTED
######################################################################

UploadArmTrustedToolchain() {
  local rev=$1
  local tarball=$2

  UploadToolchainComponent ${rev} naclsdk_linux_arm-trusted.tgz ${tarball}
}

DownloadArmTrustedToolchain() {
  local rev=$1
  local tarball=$2
  curl -L \
     ${URL_PREFIX_RAW}/${BASE_TOOLCHAIN_COMPONENT}/${rev}/naclsdk_linux_arm-trusted.tgz \
     -o ${tarball}
}

ShowRecentArmTrustedToolchains() {
   local url=gs://${BASE_TOOLCHAIN_COMPONENT}/*/naclsdk_linux_arm-trusted.tgz
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
#@ pnacl_linux_x86
#@ pnacl_mac_x86
#@ pnacl_win_x86

# TODO(robertm): "arm untrusted" should be renamed to pnacl
UploadArmUntrustedToolchains() {
  local rev=$1
  local label=$2
  local tarball=$3

  ComputeSha1 ${tarball} > ${tarball}.sha1hash
  UploadToolchainComponent ${rev} naclsdk_${label}.tgz.sha1hash ${tarball}.sha1hash

  # NOTE: only the last link is shown on the waterfall so this should come last
  UploadToolchainComponent ${rev} naclsdk_${label}.tgz ${tarball}
}

DownloadArmUntrustedToolchains() {
  local rev=$1
  local label=$2
  local tarball=$3

  curl -L \
      ${URL_PREFIX}/${BASE_TOOLCHAIN_COMPONENT}/${rev}/naclsdk_${label}.tgz \
      -o ${tarball}
}

ShowRecentArmUntrustedToolchains() {
  local label=$1
  local url="gs://${BASE_TOOLCHAIN_COMPONENT}/*/naclsdk_${label}.tgz"

  local recent=$(${GS_UTIL} ls ${url} | tail -5)
  for url in ${recent} ; do
    if ${GS_UTIL} ls -L "${url}" ; then
      echo "====="
    fi
  done
}

######################################################################
# Pexes for bitcode stability testing
######################################################################

UploadArchivedPexes() {
  local rev=$1
  local label="archived_pexes_$2.tar.bz2"
  local tarball=$3

  # TODO(robertm,bradn): find another place to store this and
  #                      negotiate long term storage guarantees
  UploadToolchainComponent ${rev} ${label} ${tarball}
}

DownloadArchivedPexes() {
  local rev=$1
  local label="archived_pexes_$2.tar.bz2"
  local tarball=$3

  curl -L ${URL_PREFIX_RAW}/${BASE_TOOLCHAIN_COMPONENT}/${rev}/${label} \
      -o ${tarball}
}

UploadArchivedPexesTranslator() {
    UploadArchivedPexes $1 "translator" $2
}

DownloadArchivedPexesTranslator() {
    DownloadArchivedPexes $1 "translator" $2
}

UploadArchivedPexesSpec2k() {
    UploadArchivedPexes $1 "spec2k" $2
}

DownloadArchivedPexesSpec2k() {
    DownloadArchivedPexes $1 "spec2k" $2
}
######################################################################
# ARM BETWEEN BOTS
######################################################################

UploadArmBinariesForHWBots() {
  local name=$1
  local tarball=$2
  Upload ${tarball} ${BASE_BETWEEN_BOTS}/${name}/$(basename ${tarball})
}


DownloadArmBinariesForHWBots() {
  local name=$1
  local tarball=$2
  curl -L \
     ${URL_PREFIX_RAW}/${BASE_BETWEEN_BOTS}/${name}/$(basename ${tarball}) \
     -o ${tarball}
}

######################################################################
# ARM BETWEEN BOTS TRY
######################################################################

UploadArmBinariesForHWBotsTry() {
  local name=$1
  local tarball=$2
  Upload ${tarball} ${BASE_BETWEEN_BOTS_TRY}/${name}/$(basename ${tarball})
}


DownloadArmBinariesForHWBotsTry() {
  local name=$1
  local tarball=$2
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
