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
  # Trunk packages need to install to a custom path so they don't conflict with
  # release channel packages.
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
  rpmlib(PayloadIsXz) <= 5.2-1, \
  /usr/sbin/update-alternatives"
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

  # If we specify a dependecy of foo.so below, we would depend on both the
  # 32 and 64-bit versions on a 64-bit machine. The current version of RPM
  # we use is too old and doesn't provide %{_isa}, so we do this manually.
  if [ "$ARCHITECTURE" = "x86_64" ] ; then
    local EMPTY_VERSION="()"
    local PKG_ARCH="(64bit)"
  elif [ "$ARCHITECTURE" = "i386" ] ; then
    local EMPTY_VERSION=""
    local PKG_ARCH=""
  fi

  # Use find-requires script to make sure the dependencies are complete
  # (especially libc versions).
  DETECTED_DEPENDS="$(echo "${BUILDDIR}/chrome" | /usr/lib/rpm/find-requires)"

  # Compare the expected dependency list to the generated list.
  #
  # In this comparison, we allow ld-linux.so's symbol version "GLIBC_2.3"
  # to be present but don't require it, because it is hard to predict
  # whether it will be generated.  Referencing a __thread
  # (thread-local/TLS) variable *sometimes* causes the compiler to generate
  # a reference to __tls_get_addr() (depending on compiler options such as
  # -fPIC vs. -fPIE).  This function has symbol version "GLIBC_2.3".  The
  # linker *sometimes* optimizes away the __tls_get_addr() call using
  # link-time code rewriting, but it might leave the symbol dependency in
  # place -- there are no guarantees.
  BAD_DIFF=0
  if [ -r "$SCRIPTDIR/expected_deps_$ARCHITECTURE" ]; then
    diff -u "$SCRIPTDIR/expected_deps_$ARCHITECTURE" \
        <(echo "${DETECTED_DEPENDS}" | \
          LANG=C sort | \
          grep -v '^ld-linux.*\(GLIBC_2\.3\)') \
        || BAD_DIFF=1
  fi
  if [ $BAD_DIFF -ne 0 ] && [ -z "${IGNORE_DEPS_CHANGES:-}" ]; then
    echo
    echo "ERROR: Shared library dependencies changed!"
    echo "If this is intentional, please update:"
    echo "chrome/installer/linux/rpm/expected_deps_x86_64"
    echo
    exit $BAD_DIFF
  fi

  # lsb implies many dependencies and on Fedora or RHEL some of these are not
  # needed at all (the most obvious one is qt3) and Chrome is usually the one
  # who pulls them to the system by requiring the whole lsb. Require only
  # lsb_release from the lsb as that's the only thing that we are using.
  #
  # nss (bundled) is optional in LSB 4.0. Also specify a more recent version
  # for security and stability updates. While we depend on libnss3.so and not
  # libssl3.so, force the dependency on libssl3 to ensure the NSS version is
  # 3.28 or later, since libssl3 should always be packaged with libnss3.
  #
  # wget is for uploading crash reports with Breakpad.
  #
  # xdg-utils is still optional in LSB 4.0.
  #
  # zlib may not need to be there. It should be included with LSB.
  # TODO(thestig): Figure out why there is an entry for zlib.
  #
  # We want to depend on the system SSL certs so wget can upload crash reports
  # securely, but there's no common capability between the distros. Bugs filed:
  # https://qa.mandriva.com/show_bug.cgi?id=55714
  # https://bugzilla.redhat.com/show_bug.cgi?id=538158
  # https://bugzilla.novell.com/show_bug.cgi?id=556248
  #
  # We want to depend on liberation-fonts as well, but there is no such package
  # for Fedora. https://bugzilla.redhat.com/show_bug.cgi?id=1252564
  # TODO(thestig): Use the liberation-fonts package once its available on all
  # supported distros.
  DEPENDS="/usr/bin/lsb_release, \
  libnss3.so(NSS_3.22)${PKG_ARCH}, \
  libssl3.so(NSS_3.28)${PKG_ARCH}, \
  wget, \
  xdg-utils, \
  zlib, \
  $(echo "${DETECTED_DEPENDS}" | tr '\n' ',')"
  gen_spec

  # Create temporary rpmbuild dirs.
  mkdir -p "$RPMBUILD_DIR/BUILD"
  mkdir -p "$RPMBUILD_DIR/RPMS"

  # '__os_install_post ${nil}' disables a bunch of automatic post-processing
  # (brp-compress, etc.), which by default appears to only be enabled on 32-bit,
  # and which doesn't gain us anything since we already explicitly do all the
  # compression, symbol stripping, etc. that we want.
  fakeroot rpmbuild -bb --target="$ARCHITECTURE" --rmspec \
    --define "_topdir $RPMBUILD_DIR" \
    --define "_binary_payload w9.xzdio" \
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
  echo "usage: $(basename $0) [-c channel] [-a target_arch] [-o 'dir']"
  echo "                      [-b 'dir'] -d branding"
  echo "-c channel the package channel (trunk, asan, unstable, beta, stable)"
  echo "-a arch    package architecture (ia32 or x64)"
  echo "-o dir     package output directory [${OUTPUTDIR}]"
  echo "-b dir     build input directory    [${BUILDDIR}]"
  echo "-d brand   either chromium or google_chrome"
  echo "-h         this help message"
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
    trunk|asan )
      # This is a special package, mostly for development testing, so don't make
      # it replace any installed release packages.
      # TODO(phajdan.jr): Remove REPLACES completely.
      REPLACES="dummy"
      # Setting this to empty will prevent it from updating any existing configs
      # from release packages.
      REPOCONFIG=""
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
  while getopts ":o:b:c:a:d:h" OPTNAME
  do
    case $OPTNAME in
      o )
        OUTPUTDIR=$(readlink -f "${OPTARG}")
        mkdir -p "${OUTPUTDIR}"
        ;;
      b )
        BUILDDIR=$(readlink -f "${OPTARG}")
        ;;
      c )
        CHANNEL="$OPTARG"
        verify_channel
        ;;
      a )
        TARGETARCH="$OPTARG"
        ;;
      d )
        BRANDING="$OPTARG"
        ;;
      h )
        usage
        exit 0
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
CHANNEL="trunk"
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
