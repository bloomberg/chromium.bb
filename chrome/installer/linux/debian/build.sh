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

# Create the Debian changelog file needed by dpkg-gencontrol. This just adds a
# placeholder change, indicating it is the result of an automatic build.
# TODO(mmoss) Release packages should create something meaningful for a
# changelog, but simply grabbing the actual 'svn log' is way too verbose. Do we
# have any type of "significant/visible changes" log that we could use for this?
gen_changelog() {
  rm -f "${DEB_CHANGELOG}"
  process_template "${SCRIPTDIR}/changelog.template" "${DEB_CHANGELOG}"
  debchange -a --nomultimaint -m --changelog "${DEB_CHANGELOG}" \
    "Release Notes: ${RELEASENOTES}"
  GZLOG="${STAGEDIR}/usr/share/doc/${PACKAGE}-${CHANNEL}/changelog.gz"
  mkdir -p "$(dirname "${GZLOG}")"
  gzip -9 -c "${DEB_CHANGELOG}" > "${GZLOG}"
  chmod 644 "${GZLOG}"
}

# Create the Debian control file needed by dpkg-deb.
gen_control() {
  dpkg-gencontrol -v"${VERSIONFULL}" -c"${DEB_CONTROL}" -l"${DEB_CHANGELOG}" \
  -f"${DEB_FILES}" -p"${PACKAGE}-${CHANNEL}" -P"${STAGEDIR}" \
  -O > "${STAGEDIR}/DEBIAN/control"
  rm -f "${DEB_CONTROL}"
}

# Setup the installation directory hierachy in the package staging area.
prep_staging_debian() {
  prep_staging_common
  install -m 755 -d "${STAGEDIR}/DEBIAN" \
    "${STAGEDIR}/etc/cron.daily" \
    "${STAGEDIR}/usr/share/menu" \
    "${STAGEDIR}/usr/share/doc/${PACKAGE}"
}

# Put the package contents in the staging area.
stage_install_debian() {
  # Always use a different name for /usr/bin symlink depending on channel.
  # First, to avoid file collisions. Second, to make it possible to
  # use update-alternatives for /usr/bin/google-chrome.
  local USR_BIN_SYMLINK_NAME="${PACKAGE}-${CHANNEL}"

  if [ "$CHANNEL" != "stable" ]; then
    # This would ideally be compiled into the app, but that's a bit too
    # intrusive of a change for these limited use channels, so we'll just hack
    # it into the wrapper script. The user can still override since it seems to
    # work to specify --user-data-dir multiple times on the command line, with
    # the last occurrence winning.
    local SXS_USER_DATA_DIR="\${XDG_CONFIG_HOME:-\${HOME}/.config}/${PACKAGE}-${CHANNEL}"
    local DEFAULT_FLAGS="--user-data-dir=\"${SXS_USER_DATA_DIR}\""

    # Avoid file collisions between channels.
    local INSTALLDIR="${INSTALLDIR}-${CHANNEL}"

    local PACKAGE="${PACKAGE}-${CHANNEL}"

    # Make it possible to distinguish between menu entries
    # for different channels.
    local MENUNAME="${MENUNAME} (${CHANNEL})"
  fi
  prep_staging_debian
  stage_install_common
  echo "Staging Debian install files in '${STAGEDIR}'..."
  install -m 755 -d "${STAGEDIR}/${INSTALLDIR}/cron"
  process_template "${BUILDDIR}/installer/common/repo.cron" \
      "${STAGEDIR}/${INSTALLDIR}/cron/${PACKAGE}"
  chmod 755 "${STAGEDIR}/${INSTALLDIR}/cron/${PACKAGE}"
  pushd "${STAGEDIR}/etc/cron.daily/"
  ln -snf "${INSTALLDIR}/cron/${PACKAGE}" "${PACKAGE}"
  popd
  process_template "${BUILDDIR}/installer/debian/debian.menu" \
    "${STAGEDIR}/usr/share/menu/${PACKAGE}.menu"
  chmod 644 "${STAGEDIR}/usr/share/menu/${PACKAGE}.menu"
  process_template "${BUILDDIR}/installer/debian/postinst" \
    "${STAGEDIR}/DEBIAN/postinst"
  chmod 755 "${STAGEDIR}/DEBIAN/postinst"
  process_template "${BUILDDIR}/installer/debian/prerm" \
    "${STAGEDIR}/DEBIAN/prerm"
  chmod 755 "${STAGEDIR}/DEBIAN/prerm"
  process_template "${BUILDDIR}/installer/debian/postrm" \
    "${STAGEDIR}/DEBIAN/postrm"
  chmod 755 "${STAGEDIR}/DEBIAN/postrm"
}

# Actually generate the package file.
do_package() {
  echo "Packaging ${ARCHITECTURE}..."
  PREDEPENDS="$COMMON_PREDEPS"
  DEPENDS="${COMMON_DEPS}"
  REPLACES=""
  CONFLICTS=""
  PROVIDES="www-browser"
  gen_changelog
  process_template "${SCRIPTDIR}/control.template" "${DEB_CONTROL}"
  export DEB_HOST_ARCH="${ARCHITECTURE}"
  if [ -f "${DEB_CONTROL}" ]; then
    gen_control
  fi
  fakeroot dpkg-deb -Zlzma -b "${STAGEDIR}" .
}

# Remove temporary files and unwanted packaging output.
cleanup() {
  echo "Cleaning..."
  rm -rf "${STAGEDIR}"
  rm -rf "${TMPFILEDIR}"
}

usage() {
  echo "usage: $(basename $0) [-c channel] [-a target_arch] [-o 'dir'] "
  echo "                      [-b 'dir']"
  echo "-c channel the package channel (trunk, asan, unstable, beta, stable)"
  echo "-a arch    package architecture (ia32 or x64)"
  echo "-o dir     package output directory [${OUTPUTDIR}]"
  echo "-b dir     build input directory    [${BUILDDIR}]"
  echo "-h         this help message"
}

# Check that the channel name is one of the allowable ones.
verify_channel() {
  case $CHANNEL in
    stable )
      CHANNEL=stable
      RELEASENOTES="http://googlechromereleases.blogspot.com/search/label/Stable%20updates"
      ;;
    unstable|dev|alpha )
      CHANNEL=unstable
      RELEASENOTES="http://googlechromereleases.blogspot.com/search/label/Dev%20updates"
      ;;
    testing|beta )
      CHANNEL=beta
      RELEASENOTES="http://googlechromereleases.blogspot.com/search/label/Beta%20updates"
      ;;
    trunk|asan )
      # Setting this to empty will prevent it from updating any existing configs
      # from release packages.
      REPOCONFIG=""
      RELEASENOTES="http://googlechromereleases.blogspot.com/"
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
  while getopts ":o:b:c:a:h" OPTNAME
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
        ;;
      a )
        TARGETARCH="$OPTARG"
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
STAGEDIR=$(mktemp -d -t deb.build.XXXXXX) || exit 1
TMPFILEDIR=$(mktemp -d -t deb.tmp.XXXXXX) || exit 1
DEB_CHANGELOG="${TMPFILEDIR}/changelog"
DEB_FILES="${TMPFILEDIR}/files"
DEB_CONTROL="${TMPFILEDIR}/control"
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
if [ ! "$BUILDDIR" ]; then
  BUILDDIR=$(readlink -f "${SCRIPTDIR}/../../../../../out/Release")
fi

source ${BUILDDIR}/installer/common/installer.include

get_version_info
VERSIONFULL="${VERSION}-${PACKAGE_RELEASE}"

if [ "$CHROMIUM_BUILD" = "_google_chrome" ]; then
  source "${BUILDDIR}/installer/common/google-chrome.info"
else
  source "${BUILDDIR}/installer/common/chromium-browser.info"
fi
eval $(sed -e "s/^\([^=]\+\)=\(.*\)$/export \1='\2'/" \
  "${BUILDDIR}/installer/theme/BRANDING")

REPOCONFIG="deb http://dl.google.com/linux/chrome/deb/ stable main"
verify_channel

# Some Debian packaging tools want these set.
export DEBFULLNAME="${MAINTNAME}"
export DEBEMAIL="${MAINTMAIL}"

# We'd like to eliminate more of these deps by relying on the 'lsb' package, but
# that brings in tons of unnecessary stuff, like an mta and rpm. Until that full
# 'lsb' package is installed by default on DEB distros, we'll have to stick with
# the LSB sub-packages, to avoid pulling in all that stuff that's not installed
# by default.

# Need a dummy debian/control file for dpkg-shlibdeps.
DUMMY_STAGING_DIR="${TMPFILEDIR}/dummy_staging"
mkdir "$DUMMY_STAGING_DIR"
cd "$DUMMY_STAGING_DIR"
mkdir debian
touch debian/control

# Generate the dependencies,
# TODO(mmoss): This is a workaround for a problem where dpkg-shlibdeps was
# resolving deps using some of our build output shlibs (i.e.
# out/Release/lib.target/libfreetype.so.6), and was then failing with:
#   dpkg-shlibdeps: error: no dependency information found for ...
# It's not clear if we ever want to look in LD_LIBRARY_PATH to resolve deps,
# but it seems that we don't currently, so this is the most expediant fix.
SAVE_LDLP=${LD_LIBRARY_PATH:-}
unset LD_LIBRARY_PATH
DPKG_SHLIB_DEPS=$(dpkg-shlibdeps -O "$BUILDDIR/chrome" 2> /dev/null | \
  sed 's/^shlibs:Depends=//')
if [ -n "$SAVE_LDLP" ]; then
  LD_LIBRARY_PATH=$SAVE_LDLP
fi

# Format it nicely and save it for comparison.
# The grep -v is for a duplicate libc6 dep caused by Lucid glibc silliness.
echo "$DPKG_SHLIB_DEPS" | sed 's/, /\n/g' | \
  grep -v '^libc6 (>= 2.3.6-6~)$' > actual

# Compare the expected dependency list to the generate list.
BAD_DIFF=0
diff "$SCRIPTDIR/expected_deps_$TARGETARCH" actual || BAD_DIFF=1
if [ $BAD_DIFF -ne 0 ] && [ -z "${IGNORE_DEPS_CHANGES:-}" ]; then
  echo
  echo "ERROR: Shared library dependencies changed!"
  echo "If this is intentional, please update:"
  echo "chrome/installer/linux/debian/expected_deps_ia32"
  echo "chrome/installer/linux/debian/expected_deps_x64"
  echo
  exit $BAD_DIFF
fi
rm -rf "$DUMMY_STAGING_DIR"

# Additional dependencies not in the dpkg-shlibdeps output.
# Pull a more recent version of NSS than required by runtime linking, for
# security and stability updates in NSS.
ADDITION_DEPS="ca-certificates, libappindicator1, libcurl3, \
  libnss3 (>= 3.14.3), lsb-base (>=3.2), xdg-utils (>= 1.0.2), wget"

# Fix-up libnspr dependency due to renaming in Ubuntu (the old package still
# exists, but it was moved to "universe" repository, which isn't installed by
# default).
DPKG_SHLIB_DEPS=$(sed \
    's/\(libnspr4-0d ([^)]*)\), /\1 | libnspr4 (>= 4.9.5-0ubuntu0), /g' \
    <<< $DPKG_SHLIB_DEPS)

# Fix-up libudev dependency because Ubuntu 13.04 has libudev1 instead of
# libudev0.
DPKG_SHLIB_DEPS=$(sed 's/\(libudev0 ([^)]*)\), /\1 | libudev1 (>= 198), /g' \
                  <<< $DPKG_SHLIB_DEPS)

COMMON_DEPS="${DPKG_SHLIB_DEPS}, ${ADDITION_DEPS}"
COMMON_PREDEPS="dpkg (>= 1.14.0)"


# Make everything happen in the OUTPUTDIR.
cd "${OUTPUTDIR}"

case "$TARGETARCH" in
  ia32 )
    export ARCHITECTURE="i386"
    stage_install_debian
    ;;
  x64 )
    export ARCHITECTURE="amd64"
    stage_install_debian
    ;;
  * )
    echo
    echo "ERROR: Don't know how to build DEBs for '$TARGETARCH'."
    echo
    exit 1
    ;;
esac

do_package
