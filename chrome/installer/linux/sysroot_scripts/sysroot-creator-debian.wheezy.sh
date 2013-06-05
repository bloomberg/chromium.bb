#!/bin/sh
#
# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
#@ This script builds a Debian Wheezy sysroot for building Google Chrome.
#@
#@  Generally this script is invoked as:
#@  sysroot-creator-debian.wheezy.sh <mode> <args>*
#@  Available modes are shown below.
#@
#@ List of modes:

######################################################################
# Config
######################################################################

set -o nounset
set -o errexit

readonly SCRIPT_DIR=$(dirname $0)

# This is where the staging sysroot is.
readonly INSTALL_ROOT_AMD64=$(pwd)/debian_wheezy_amd64_staging
readonly INSTALL_ROOT_I386=$(pwd)/debian_wheezy_i386_staging

readonly REQUIRED_TOOLS="wget"

######################################################################
# Package Config
######################################################################

# this is where we get all the debian packages from
readonly DEBIAN_REPO=http://http.us.debian.org/debian

readonly PACKAGE_LIST_AMD64="${DEBIAN_REPO}/dists/wheezy/main/binary-amd64/Packages.bz2"
readonly PACKAGE_LIST_I386="${DEBIAN_REPO}/dists/wheezy/main/binary-i386/Packages.bz2"

# TODO(thestig) Remove unused libbz2 packages in the next sysroot update.
# Sysroot packages: these are the packages needed to build chrome.
# NOTE: When DEBIAN_PACKAGES is modified, the packagelist files must be updated
# by running this script in GeneratePackageList mode.
readonly DEBIAN_PACKAGES="\
  comerr-dev \
  gcc-4.6 \
  krb5-multidev \
  libasound2 \
  libasound2-dev \
  libatk1.0-0 \
  libatk1.0-dev \
  libavahi-client3 \
  libavahi-common3 \
  libbz2-1.0 \
  libbz2-dev \
  libc6 \
  libc6-dev \
  libcairo2 \
  libcairo2-dev \
  libcairo-gobject2 \
  libcairo-script-interpreter2 \
  libcomerr2 \
  libcups2 \
  libcups2-dev \
  libdbus-1-3 \
  libdbus-1-dev \
  libdbus-glib-1-2 \
  libelf1 \
  libelf-dev \
  libexpat1 \
  libexpat1-dev \
  libffi5 \
  libfontconfig1 \
  libfontconfig1-dev \
  libfreetype6 \
  libfreetype6-dev \
  libgcc1 \
  libgcc1 \
  libgconf-2-4 \
  libgconf2-4 \
  libgconf2-dev \
  libgcrypt11 \
  libgcrypt11-dev \
  libgdk-pixbuf2.0-0 \
  libgdk-pixbuf2.0-dev \
  libglib2.0-0 \
  libglib2.0-dev \
  libgnome-keyring0 \
  libgnome-keyring-dev \
  libgnutls26 \
  libgnutls-dev \
  libgnutls-openssl27 \
  libgnutlsxx27 \
  libgomp1 \
  libgpg-error0 \
  libgpg-error-dev \
  libgssapi-krb5-2 \
  libgssrpc4 \
  libgtk2.0-0 \
  libgtk2.0-dev \
  libk5crypto3 \
  libkadm5clnt-mit8 \
  libkadm5srv-mit8 \
  libkdb5-6 \
  libkeyutils1 \
  libkrb5-3 \
  libkrb5-dev \
  libkrb5support0 \
  libnspr4 \
  libnspr4-dev \
  libnss3 \
  libnss3-dev \
  libnss-db \
  liborbit2 \
  libp11-2 \
  libp11-kit0 \
  libpam0g \
  libpam0g-dev \
  libpango1.0-0 \
  libpango1.0-dev \
  libpci3 \
  libpci-dev \
  libpcre3 \
  libpcre3-dev \
  libpcrecpp0 \
  libpixman-1-0 \
  libpixman-1-dev \
  libpng12-0 \
  libpng12-dev \
  libpulse0 \
  libpulse-dev \
  libpulse-mainloop-glib0 \
  libquadmath0 \
  libselinux1 \
  libspeechd2 \
  libspeechd-dev \
  libssl1.0.0 \
  libssl-dev \
  libstdc++6 \
  libstdc++6-4.6-dev \
  libtasn1-3 \
  libudev0 \
  libudev-dev \
  libx11-6 \
  libx11-dev \
  libxau6 \
  libxau-dev \
  libxcb1 \
  libxcb1-dev \
  libxcb-render0 \
  libxcb-render0-dev \
  libxcb-shm0 \
  libxcb-shm0-dev \
  libxcomposite1 \
  libxcomposite-dev \
  libxcursor1 \
  libxcursor-dev \
  libxdamage1 \
  libxdamage-dev \
  libxdmcp6 \
  libxext6 \
  libxext-dev \
  libxfixes3 \
  libxfixes-dev \
  libxi6 \
  libxi-dev \
  libxinerama1 \
  libxinerama-dev \
  libxrandr2 \
  libxrandr-dev \
  libxrender1 \
  libxrender-dev \
  libxss1 \
  libxss-dev \
  libxt6 \
  libxt-dev \
  libxtst6 \
  libxtst-dev \
  linux-libc-dev \
  speech-dispatcher \
  x11proto-composite-dev \
  x11proto-core-dev \
  x11proto-damage-dev \
  x11proto-fixes-dev \
  x11proto-input-dev \
  x11proto-kb-dev \
  x11proto-randr-dev \
  x11proto-record-dev \
  x11proto-render-dev \
  x11proto-scrnsaver-dev \
  x11proto-xext-dev \
  zlib1g \
  zlib1g-dev"

readonly DEBIAN_DEP_LIST_AMD64="${SCRIPT_DIR}/packagelist.debian.wheezy.amd64"
readonly DEBIAN_DEP_LIST_I386="${SCRIPT_DIR}/packagelist.debian.wheezy.i386"
readonly DEBIAN_DEP_FILES_AMD64="$(cat ${DEBIAN_DEP_LIST_AMD64})"
readonly DEBIAN_DEP_FILES_I386="$(cat ${DEBIAN_DEP_LIST_I386})"

######################################################################
# Helper
######################################################################

Banner() {
  echo "######################################################################"
  echo $*
  echo "######################################################################"
}


SubBanner() {
  echo "......................................................................"
  echo $*
  echo "......................................................................"
}


Usage() {
  egrep "^#@" "$0" | cut --bytes=3-
}


DownloadOrCopy() {
  if [ -f "$2" ] ; then
    echo "$2 already in place"
    return
  fi

  HTTP=0
  echo "$1" | grep -qs ^http:// && HTTP=1
  if [ "$HTTP" = "1" ]; then
    SubBanner "downloading from $1 -> $2"
    wget "$1" -O "$2"
  else
    SubBanner "copying from $1"
    cp "$1" "$2"
  fi
}


SetEnvironmentVariables() {
  ARCH=""
  echo $1 | grep -qs Amd64$ && ARCH=AMD64
  if [ -z "$ARCH" ]; then
    echo $1 | grep -qs I386$ && ARCH=I386
  fi
  case "$ARCH" in
    AMD64)
      INSTALL_ROOT="$INSTALL_ROOT_AMD64";
      ;;
    I386)
      INSTALL_ROOT="$INSTALL_ROOT_I386";
      ;;
    *)
      echo "ERROR: Unexpected bad architecture."
      exit 1
      ;;
  esac
}

Cleanup() {
  rm -rf "$TMP"
}

# some sanity checks to make sure this script is run from the right place
# with the right tools
SanityCheck() {
  Banner "Sanity Checks"

  if [ "$(basename $(pwd))" != "sysroot_scripts" ] ; then
    echo -n "ERROR: run this script from "
    echo "src/chrome/installer/linux/sysroot_scripts"
    exit 1
  fi

  if ! mkdir -p "${INSTALL_ROOT}" ; then
     echo "ERROR: ${INSTALL_ROOT} can't be created."
    exit 1
  fi

  TMP=$(mktemp -q -t -d debian-wheezy-XXXXXX)
  if [ -z "$TMP" ]; then
    echo "ERROR: temp dir can't be created."
    exit 1
  fi
  trap Cleanup 0

  for tool in ${REQUIRED_TOOLS} ; do
    if ! which ${tool} ; then
      echo "Required binary $tool not found."
      echo "Exiting."
      exit 1
    fi
  done
}


ChangeDirectory() {
  # Change directory to where this script is.
  cd $(dirname "$0")
}


ClearInstallDir() {
  Banner "clearing dirs in ${INSTALL_ROOT}"
  rm -rf ${INSTALL_ROOT}/*
}


CreateTarBall() {
  local tarball=$1
  Banner "creating tar ball ${tarball}"
  tar zcf ${tarball} -C ${INSTALL_ROOT} .
}

######################################################################
#
######################################################################

HacksAndPatchesAmd64() {
  Banner "Misc Hacks & Patches"
  # these are linker scripts with absolute pathnames in them
  # which we rewrite here
  lscripts="${INSTALL_ROOT}/usr/lib/x86_64-linux-gnu/libpthread.so \
            ${INSTALL_ROOT}/usr/lib/x86_64-linux-gnu/libc.so"

  #SubBanner "Rewriting Linker Scripts"
  sed -i -e 's|/usr/lib/x86_64-linux-gnu/||g'  ${lscripts}
  sed -i -e 's|/lib/x86_64-linux-gnu/||g' ${lscripts}

  # This is for chrome's ./build/linux/pkg-config-wrapper
  # which overwrites PKG_CONFIG_PATH internally
  SubBanner "Package Configs Symlink"
  mkdir -p ${INSTALL_ROOT}/usr/share
  ln -s ../lib/x86_64-linux-gnu/pkgconfig ${INSTALL_ROOT}/usr/share/pkgconfig

  SubBanner "Adding an additional ld.conf include"
  LD_SO_HACK_CONF="${INSTALL_ROOT}/etc/ld.so.conf.d/zz_hack.conf"
  echo /usr/lib/gcc/x86_64-linux-gnu/4.6 > "$LD_SO_HACK_CONF"
  echo /usr/lib >> "$LD_SO_HACK_CONF"
}


HacksAndPatchesI386() {
  Banner "Misc Hacks & Patches"
  # these are linker scripts with absolute pathnames in them
  # which we rewrite here
  lscripts="${INSTALL_ROOT}/usr/lib/i386-linux-gnu/libpthread.so \
            ${INSTALL_ROOT}/usr/lib/i386-linux-gnu/libc.so"

  #SubBanner "Rewriting Linker Scripts"
  sed -i -e 's|/usr/lib/i386-linux-gnu/||g'  ${lscripts}
  sed -i -e 's|/lib/i386-linux-gnu/||g' ${lscripts}

  # This is for chrome's ./build/linux/pkg-config-wrapper
  # which overwrites PKG_CONFIG_PATH internally
  SubBanner "Package Configs Symlink"
  mkdir -p ${INSTALL_ROOT}/usr/share
  ln -s ../lib/i386-linux-gnu/pkgconfig ${INSTALL_ROOT}/usr/share/pkgconfig

  SubBanner "Adding an additional ld.conf include"
  LD_SO_HACK_CONF="${INSTALL_ROOT}/etc/ld.so.conf.d/zz_hack.conf"
  echo /usr/lib/gcc/i486-linux-gnu/4.6 > "$LD_SO_HACK_CONF"
  echo /usr/lib >> "$LD_SO_HACK_CONF"
}


InstallIntoSysroot() {
  Banner "Install Libs And Headers Into Jail"

  mkdir -p ${TMP}/debian-packages
  mkdir -p ${INSTALL_ROOT}
  for file in $@ ; do
    local package="${TMP}/debian-packages/${file##*/}"
    Banner "installing ${file}"
    DownloadOrCopy ${DEBIAN_REPO}/pool/${file} ${package}
    SubBanner "extracting to ${INSTALL_ROOT}"
    if [ ! -s "${package}" ] ; then
      echo
      echo "ERROR: bad package ${package}"
      exit 1
    fi
    dpkg --fsys-tarfile ${package}\
      | tar -xvf - --exclude=./usr/share -C ${INSTALL_ROOT}
  done
}


CleanupJailSymlinks() {
  Banner "jail symlink cleanup"

  SAVEDPWD=$(pwd)
  cd ${INSTALL_ROOT}
  find lib lib64 usr/lib -type l -printf '%p %l\n' | while read link target; do
    # skip links with non-absolute paths
    echo "${target}" | grep -qs ^/ || continue
    echo "${link}: ${target}"
    case "${link}" in
      usr/lib/gcc/x86_64-linux-gnu/4.*/* | usr/lib/gcc/i486-linux-gnu/4.*/*)
        # Relativize the symlink.
        ln -snfv "../../../../..${target}" "${link}"
        ;;
      usr/lib/x86_64-linux-gnu/* | usr/lib/i386-linux-gnu/*)
        # Relativize the symlink.
        ln -snfv "../../..${target}" "${link}"
        ;;
      usr/lib/*)
        # Relativize the symlink.
        ln -snfv "../..${target}" "${link}"
        ;;
      lib64/* | lib/*)
        # Relativize the symlink.
        ln -snfv "..${target}" "${link}"
        ;;
    esac
  done

  find lib lib64 usr/lib -type l -printf '%p %l\n' | while read link target; do
    # Make sure we catch new bad links.
    if [ ! -r "${link}" ]; then
      echo "ERROR: FOUND BAD LINK ${link}"
      ls -l ${link}
      exit 1
    fi
  done
  cd "$SAVEDPWD"
}

#@
#@ BuildSysrootAmd64 <tarball-name>
#@
#@    Build everything and package it
BuildSysrootAmd64() {
  ClearInstallDir
  InstallIntoSysroot ${DEBIAN_DEP_FILES_AMD64}
  CleanupJailSymlinks
  HacksAndPatchesAmd64
  CreateTarBall $1
}

#@
#@ BuildSysrootI386 <tarball-name>
#@
#@    Build everything and package it
BuildSysrootI386() {
  ClearInstallDir
  InstallIntoSysroot ${DEBIAN_DEP_FILES_I386}
  CleanupJailSymlinks
  HacksAndPatchesI386
  CreateTarBall $1
}

#
# GeneratePackageList
#
#     Looks up package names in ${TMP}/Packages and write list of URLs
#     to output file.
#
GeneratePackageList() {
  local output_file=$1
  echo "Updating: ${output_file}"
  /bin/rm -f ${output_file}
  shift
  for pkg in $@ ; do
    local pkg_full=$(grep -A 1 " ${pkg}\$" ${TMP}/Packages | egrep -o "pool/.*")
    if [ -z "${pkg_full}" ]; then
        echo "ERROR: missing package: $pkg"
        exit 1
    fi
    echo $pkg_full | sed "s/^pool\///" >> $output_file
  done
  # sort -o does an in-place sort of this file
  sort $output_file -o $output_file
}

#@
#@ UpdatePackageListsAmd64
#@
#@     Regenerate the package lists such that they contain an up-to-date
#@     list of URLs within the Debian archive. (For amd64)
UpdatePackageListsAmd64() {
  local package_list="${TMP}/Packages.wheezy_amd64.bz2"
  DownloadOrCopy ${PACKAGE_LIST_AMD64} ${package_list}
  bzcat ${package_list} | egrep '^(Package:|Filename:)' > ${TMP}/Packages

  GeneratePackageList ${DEBIAN_DEP_LIST_AMD64} "${DEBIAN_PACKAGES}"
}

#@
#@ UpdatePackageListsI386
#@
#@     Regenerate the package lists such that they contain an up-to-date
#@     list of URLs within the Debian archive. (For i386)
UpdatePackageListsI386() {
  local package_list="${TMP}/Packages.wheezy_i386.bz2"
  DownloadOrCopy ${PACKAGE_LIST_I386} ${package_list}
  bzcat ${package_list} | egrep '^(Package:|Filename:)' > ${TMP}/Packages

  GeneratePackageList ${DEBIAN_DEP_LIST_I386} "${DEBIAN_PACKAGES}"
}

if [ $# -eq 0 ] ; then
  echo "ERROR: you must specify a mode on the commandline"
  echo
  Usage
  exit 1
elif [ "$(type -t $1)" != "function" ]; then
  echo "ERROR: unknown function '$1'." >&2
  echo "For help, try:"
  echo "    $0 help"
  exit 1
else
  ChangeDirectory
  SetEnvironmentVariables "$1"
  SanityCheck
  "$@"
fi
