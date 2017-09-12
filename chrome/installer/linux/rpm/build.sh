#!/bin/bash
#
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# TODO(mmoss) This currently only works with official builds, since non-official
# builds don't add the "${BUILDDIR}/installer/" files needed for packaging.

set -e
if [ "$VERBOSE" ]; then
  set -x
fi
set -u

gen_spec() {
  rm -f "${SPEC}"
  # Different channels need to install to different locations so they
  # don't conflict with each other.
  local PACKAGE_FILENAME="${PACKAGE}-${CHANNEL}"
  if [ "$CHANNEL" != "stable" ]; then
    local INSTALLDIR="${INSTALLDIR}-${CHANNEL}"
    local PACKAGE="${PACKAGE}-${CHANNEL}"
    local MENUNAME="${MENUNAME} (${CHANNEL})"
  fi
  process_template "${SCRIPTDIR}/chrome.spec.template" "${SPEC}"
}

# Setup the installation directory hierachy in the package staging area.
prep_staging_rpm() {
  prep_staging_common
  install -m 755 -d "${STAGEDIR}/etc/cron.daily"
}

# Put the package contents in the staging area.
stage_install_rpm() {
  # TODO(phajdan.jr): Deduplicate this and debian/build.sh .
  # For now duplication is going to help us avoid merge conflicts
  # as changes are frequently merged to older branches related to SxS effort.
  if [ "$CHANNEL" != "stable" ]; then
    # Avoid file collisions between channels.
    local PACKAGE="${PACKAGE}-${CHANNEL}"
    local INSTALLDIR="${INSTALLDIR}-${CHANNEL}"

    # Make it possible to distinguish between menu entries
    # for different channels.
    local MENUNAME="${MENUNAME} (${CHANNEL})"
  fi
  prep_staging_rpm
  stage_install_common
  echo "Staging RPM install files in '${STAGEDIR}'..."
  process_template "${BUILDDIR}/installer/common/rpmrepo.cron" \
    "${STAGEDIR}/etc/cron.daily/${PACKAGE}"
  chmod 755 "${STAGEDIR}/etc/cron.daily/${PACKAGE}"
}

verify_package() {
  local DEPENDS="$1"
  local ADDITIONAL_RPM_DEPENDS="/bin/sh, \
  rpmlib(CompressedFileNames) <= 3.0.4-1, \
  rpmlib(PayloadFilesHavePrefix) <= 4.0-1, \
  /usr/sbin/update-alternatives"
  if [ ${IS_OFFICIAL_BUILD} -ne 0 ]; then
    ADDITIONAL_RPM_DEPENDS="${ADDITIONAL_RPM_DEPENDS}, \
      rpmlib(PayloadIsXz) <= 5.2-1"
  fi
  echo "${DEPENDS}" "${ADDITIONAL_RPM_DEPENDS}" | sed 's/,/\n/g' | \
      sed 's/^ *//' | LANG=C sort > expected_rpm_depends
  rpm -qpR "${OUTPUTDIR}/${PKGNAME}.${ARCHITECTURE}.rpm" | LANG=C sort | uniq \
      > actual_rpm_depends
  BAD_DIFF=0
  diff -u expected_rpm_depends actual_rpm_depends || BAD_DIFF=1
  if [ $BAD_DIFF -ne 0 ] && [ -z "${IGNORE_DEPS_CHANGES:-}" ]; then
    echo
    echo "ERROR: bad rpm dependencies!"
    echo
    exit $BAD_DIFF
  fi
}

# Actually generate the package file.
do_package() {
  echo "Packaging ${ARCHITECTURE}..."
  PROVIDES="${PACKAGE}"
  local REPS="$REPLACES"
  REPLACES=""
  for rep in $REPS; do
    if [ -z "$REPLACES" ]; then
      REPLACES="$PACKAGE-$rep"
    else
      REPLACES="$REPLACES $PACKAGE-$rep"
    fi
  done

  # The symbols in libX11.so are not versioned, so when a newer version has new
  # symbols like _XGetRequest, RPM's find-requires tool does not detect it, and
  # there is no way to specify a libX11.so version number to prevent
  # installation on affected distros like OpenSUSE 12.1 and Fedora 16.
  # Thus there has to be distro-specific conflict here.
  # TODO(thestig) Remove these in the future when other requirements prevent
  # installation on affected distros.
  ADDITIONAL_CONFLICTS="xorg-x11-libX11 < 7.6_1 libX11 < 1.4.99"
  REPLACES="$REPLACES $ADDITIONAL_CONFLICTS"

  RPM_COMMON_DEPS="${BUILDDIR}/rpm_common.deps"
  DEPENDS=$(cat "${RPM_COMMON_DEPS}" | tr '\n' ',')
  gen_spec

  # Create temporary rpmbuild dirs.
  mkdir -p "$RPMBUILD_DIR/BUILD"
  mkdir -p "$RPMBUILD_DIR/RPMS"

  if [ ${IS_OFFICIAL_BUILD} -ne 0 ]; then
    local COMPRESSION_OPT="_binary_payload w9.xzdio"
  else
    local COMPRESSION_OPT="_binary_payload w0.gzdio"
  fi

  # '__os_install_post ${nil}' disables a bunch of automatic post-processing
  # (brp-compress, etc.), which by default appears to only be enabled on 32-bit,
  # and which doesn't gain us anything since we already explicitly do all the
  # compression, symbol stripping, etc. that we want.
  fakeroot rpmbuild -bb --target="$ARCHITECTURE" --rmspec \
    --define "_topdir $RPMBUILD_DIR" \
    --define "${COMPRESSION_OPT}" \
    --define "__os_install_post  %{nil}" \
    "${SPEC}"
  PKGNAME="${PACKAGE}-${CHANNEL}-${VERSION}-${PACKAGE_RELEASE}"
  mv "$RPMBUILD_DIR/RPMS/$ARCHITECTURE/${PKGNAME}.${ARCHITECTURE}.rpm" \
     "${OUTPUTDIR}"
  # Make sure the package is world-readable, otherwise it causes problems when
  # copied to share drive.
  chmod a+r "${OUTPUTDIR}/${PKGNAME}.${ARCHITECTURE}.rpm"

  verify_package "$DEPENDS"
}

# Remove temporary files and unwanted packaging output.
cleanup() {
  rm -rf "${STAGEDIR}"
  rm -rf "${TMPFILEDIR}"
  rm -rf "${RPMBUILD_DIR}"
}

usage() {
  echo "usage: $(basename $0) [-a target_arch] [-b 'dir'] -c channel"
  echo "                      -d branding [-f] [-o 'dir']"
  echo "-a arch     package architecture (ia32 or x64)"
  echo "-b dir      build input directory    [${BUILDDIR}]"
  echo "-c channel  the package channel (unstable, beta, stable)"
  echo "-d brand    either chromium or google_chrome"
  echo "-f          indicates that this is an official build"
  echo "-h          this help message"
  echo "-o dir      package output directory [${OUTPUTDIR}]"
}

# Check that the channel name is one of the allowable ones.
verify_channel() {
  case $CHANNEL in
    stable )
      CHANNEL=stable
      # TODO(phajdan.jr): Remove REPLACES completely.
      REPLACES="dummy"
      ;;
    unstable|dev|alpha )
      CHANNEL=unstable
      # TODO(phajdan.jr): Remove REPLACES completely.
      REPLACES="dummy"
      ;;
    testing|beta )
      CHANNEL=beta
      # TODO(phajdan.jr): Remove REPLACES completely.
      REPLACES="dummy"
      ;;
    * )
      echo
      echo "ERROR: '$CHANNEL' is not a valid channel type."
      echo
      exit 1
      ;;
  esac
}

process_opts() {
  while getopts ":a:b:c:d:fho:" OPTNAME
  do
    case $OPTNAME in
      a )
        TARGETARCH="$OPTARG"
        ;;
      b )
        BUILDDIR=$(readlink -f "${OPTARG}")
        ;;
      c )
        CHANNEL="$OPTARG"
        verify_channel
        ;;
      d )
        BRANDING="$OPTARG"
        ;;
      f )
        IS_OFFICIAL_BUILD=1
        ;;
      h )
        usage
        exit 0
        ;;
      o )
        OUTPUTDIR=$(readlink -f "${OPTARG}")
        mkdir -p "${OUTPUTDIR}"
        ;;
      \: )
        echo "'-$OPTARG' needs an argument."
        usage
        exit 1
        ;;
      * )
        echo "invalid command-line option: $OPTARG"
        usage
        exit 1
        ;;
    esac
  done
}

#=========
# MAIN
#=========

SCRIPTDIR=$(readlink -f "$(dirname "$0")")
OUTPUTDIR="${PWD}"
# Default target architecture to same as build host.
if [ "$(uname -m)" = "x86_64" ]; then
  TARGETARCH="x64"
else
  TARGETARCH="ia32"
fi

# call cleanup() on exit
trap cleanup 0
process_opts "$@"
BUILDDIR=${BUILDDIR:=$(readlink -f "${SCRIPTDIR}/../../../../out/Release")}
IS_OFFICIAL_BUILD=${IS_OFFICIAL_BUILD:=0}

STAGEDIR="${BUILDDIR}/rpm-staging-${CHANNEL}"
mkdir -p "${STAGEDIR}"
TMPFILEDIR="${BUILDDIR}/rpm-tmp-${CHANNEL}"
mkdir -p "${TMPFILEDIR}"
RPMBUILD_DIR="${BUILDDIR}/rpm-build-${CHANNEL}"
mkdir -p "${RPMBUILD_DIR}"
SPEC="${TMPFILEDIR}/chrome.spec"

source ${BUILDDIR}/installer/common/installer.include

get_version_info

if [ "$BRANDING" = "google_chrome" ]; then
  source "${BUILDDIR}/installer/common/google-chrome.info"
else
  source "${BUILDDIR}/installer/common/chromium-browser.info"
fi
eval $(sed -e "s/^\([^=]\+\)=\(.*\)$/export \1='\2'/" \
  "${BUILDDIR}/installer/theme/BRANDING")

REPOCONFIG="http://dl.google.com/linux/${PACKAGE#google-}/rpm/stable"
verify_channel
export USR_BIN_SYMLINK_NAME="${PACKAGE}-${CHANNEL}"

# Make everything happen in the OUTPUTDIR.
cd "${OUTPUTDIR}"

case "$TARGETARCH" in
  ia32 )
    export ARCHITECTURE="i386"
    stage_install_rpm
    ;;
  x64 )
    export ARCHITECTURE="x86_64"
    stage_install_rpm
    ;;
  * )
    echo
    echo "ERROR: Don't know how to build RPMs for '$TARGETARCH'."
    echo
    exit 1
    ;;
esac

do_package
