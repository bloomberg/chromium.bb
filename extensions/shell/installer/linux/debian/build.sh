#!/bin/bash
#
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# TODO(michaelpg): Dedupe common functionality with the Chrome installer.

# TODO(mmoss) This currently only works with official builds, since non-official
# builds don't add the "${BUILDDIR}/app_shell_installer/" files needed for
# packaging.

set -e
set -o pipefail
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
    "${STAGEDIR}/usr/share/doc/${USR_BIN_SYMLINK_NAME}"
}

# Put the package contents in the staging area.
stage_install_debian() {
  # Always use a different name for /usr/bin symlink depending on channel to
  # avoid file collisions.
  local USR_BIN_SYMLINK_NAME="${PACKAGE}-${CHANNEL}"

  if [ "$CHANNEL" != "stable" ]; then
    # Avoid file collisions between channels.
    local INSTALLDIR="${INSTALLDIR}-${CHANNEL}"

    local PACKAGE="${PACKAGE}-${CHANNEL}"
  fi
  prep_staging_debian
  stage_install_common
  echo "Staging Debian install files in '${STAGEDIR}'..."
  install -m 755 -d "${STAGEDIR}/${INSTALLDIR}/cron"
  process_template "${BUILDDIR}/app_shell_installer/common/repo.cron" \
      "${STAGEDIR}/${INSTALLDIR}/cron/${PACKAGE}"
  chmod 755 "${STAGEDIR}/${INSTALLDIR}/cron/${PACKAGE}"
  pushd "${STAGEDIR}/etc/cron.daily/"
  ln -snf "${INSTALLDIR}/cron/${PACKAGE}" "${PACKAGE}"
  popd
  process_template "${BUILDDIR}/app_shell_installer/debian/postinst" \
    "${STAGEDIR}/DEBIAN/postinst"
  chmod 755 "${STAGEDIR}/DEBIAN/postinst"
  process_template "${BUILDDIR}/app_shell_installer/debian/postrm" \
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
  PROVIDES=""
  gen_changelog
  process_template "${SCRIPTDIR}/control.template" "${DEB_CONTROL}"
  export DEB_HOST_ARCH="${ARCHITECTURE}"
  if [ -f "${DEB_CONTROL}" ]; then
    gen_control
  fi
  fakeroot dpkg-deb -Zxz -z9 -b "${STAGEDIR}" .
}

verify_package() {
  DEPENDS="${COMMON_DEPS}"  # This needs to match do_package() above.
  echo ${DEPENDS} | sed 's/, /\n/g' | LANG=C sort > expected_deb_depends
  dpkg -I "${PACKAGE}-${CHANNEL}_${VERSIONFULL}_${ARCHITECTURE}.deb" | \
      grep '^ Depends: ' | sed 's/^ Depends: //' | sed 's/, /\n/g' | \
      LANG=C sort > actual_deb_depends
  BAD_DIFF=0
  diff -u expected_deb_depends actual_deb_depends || BAD_DIFF=1
  if [ $BAD_DIFF -ne 0 ] && [ -z "${IGNORE_DEPS_CHANGES:-}" ]; then
    echo
    echo "ERROR: bad dpkg dependencies!"
    echo
    exit $BAD_DIFF
  fi
}

# Remove temporary files and unwanted packaging output.
cleanup() {
  echo "Cleaning..."
  rm -rf "${STAGEDIR}"
  rm -rf "${TMPFILEDIR}"
}

usage() {
  echo "usage: $(basename $0) [-c channel] [-a target_arch] [-o 'dir'] "
  echo "                      [-b 'dir'] -d branding"
  echo "-c channel the package channel (trunk, asan, unstable, beta, stable)"
  echo "-a arch    package architecture (ia32 or x64)"
  echo "-o dir     package output directory [${OUTPUTDIR}]"
  echo "-b dir     build input directory    [${BUILDDIR}]"
  echo "-d brand   either chromium or google_chrome"
  echo "-s dir     /path/to/sysroot"
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
  while getopts ":s:o:b:c:a:d:h" OPTNAME
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
      d )
        BRANDING="$OPTARG"
        ;;
      s )
        SYSROOT="$OPTARG"
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
BUILDDIR=${BUILDDIR:=$(readlink -f "${SCRIPTDIR}/../../../../out/Release")}

if [[ "$(basename ${SYSROOT})" = "debian_jessie_"*"-sysroot" ]]; then
  TARGET_DISTRO="jessie"
else
  echo "Debian package can only be built using the jessie sysroot."
  exit 1
fi

source ${BUILDDIR}/app_shell_installer/common/installer.include

get_version_info
VERSIONFULL="${VERSION}-${PACKAGE_RELEASE}"

if [ "$BRANDING" = "google_chrome" ]; then
  source "${BUILDDIR}/app_shell_installer/common/google-app-shell.info"
else
  source "${BUILDDIR}/app_shell_installer/common/chromium-app-shell.info"
fi
eval $(sed -e "s/^\([^=]\+\)=\(.*\)$/export \1='\2'/" \
  "${BUILDDIR}/app_shell_installer/theme/BRANDING")

verify_channel

# Some Debian packaging tools want these set.
export DEBFULLNAME="${MAINTNAME}"
export DEBEMAIL="${MAINTMAIL}"

# We'd like to eliminate more of these deps by relying on the 'lsb' package, but
# that brings in tons of unnecessary stuff, like an mta and rpm. Until that full
# 'lsb' package is installed by default on DEB distros, we'll have to stick with
# the LSB sub-packages, to avoid pulling in all that stuff that's not installed
# by default.

# Generate the dependencies,
# TODO(mmoss): This is a workaround for a problem where dpkg-shlibdeps was
# resolving deps using some of our build output shlibs (i.e.
# out/Release/lib.target/libfreetype.so.6), and was then failing with:
#   dpkg-shlibdeps: error: no dependency information found for ...
# It's not clear if we ever want to look in LD_LIBRARY_PATH to resolve deps,
# but it seems that we don't currently, so this is the most expediant fix.
SAVE_LDLP=${LD_LIBRARY_PATH:-}
unset LD_LIBRARY_PATH
if [ ${TARGETARCH} = "x64" ]; then
  SHLIB_ARGS="-l${SYSROOT}/usr/lib/x86_64-linux-gnu"
  SHLIB_ARGS="${SHLIB_ARGS} -l${SYSROOT}/lib/x86_64-linux-gnu"
else
  SHLIB_ARGS="-l${SYSROOT}/usr/lib/i386-linux-gnu"
  SHLIB_ARGS="${SHLIB_ARGS} -l${SYSROOT}/lib/i386-linux-gnu"
fi
SHLIB_ARGS="${SHLIB_ARGS} -l${SYSROOT}/usr/lib"
DPKG_SHLIB_DEPS=$(cd ${SYSROOT} && dpkg-shlibdeps ${SHLIB_ARGS:-} -O \
                  -e"$BUILDDIR/app_shell" | sed 's/^shlibs:Depends=//')
if [ -n "$SAVE_LDLP" ]; then
  LD_LIBRARY_PATH=$SAVE_LDLP
fi

# Format it nicely and save it for comparison.
echo "$DPKG_SHLIB_DEPS" | sed 's/, /\n/g' | LANG=C sort > actual

# Compare the expected dependency list to the generated list.
BAD_DIFF=0
diff -u "$SCRIPTDIR/expected_deps_${TARGETARCH}_${TARGET_DISTRO}" actual || \
  BAD_DIFF=1
if [ $BAD_DIFF -ne 0 ] && [ -z "${IGNORE_DEPS_CHANGES:-}" ]; then
  echo
  echo "ERROR: Shared library dependencies changed!"
  echo "If this is intentional, please update:"
  echo "extensions/shell/installer/linux/debian/expected_deps_*"
  echo
  exit $BAD_DIFF
fi

# Additional dependencies not in the dpkg-shlibdeps output.
# ca-certificates: Make sure users have SSL certificates.
# libnss3: Pull a more recent version of NSS than required by runtime linking,
#          for security and stability updates in NSS.
# lsb-release: For lsb-release.
# wget: For uploading crash reports with Breakpad.
ADDITIONAL_DEPS="ca-certificates, libnss3 (>= 3.26), lsb-release, wget"

# Fix-up libnspr dependency due to renaming in Ubuntu (the old package still
# exists, but it was moved to "universe" repository, which isn't installed by
# default).
DPKG_SHLIB_DEPS=$(sed \
    's/\(libnspr4-0d ([^)]*)\), /\1 | libnspr4 (>= 4.9.5-0ubuntu0), /g' \
    <<< $DPKG_SHLIB_DEPS)

# Remove libnss dependency so the one in $ADDITIONAL_DEPS can supercede it.
DPKG_SHLIB_DEPS=$(sed 's/\(libnss3 ([^)]*)\), //g' <<< $DPKG_SHLIB_DEPS)

COMMON_DEPS="${DPKG_SHLIB_DEPS}, ${ADDITIONAL_DEPS}"
COMMON_PREDEPS="dpkg (>= 1.14.0)"


# Make everything happen in the OUTPUTDIR.
cd "${OUTPUTDIR}"

case "$TARGETARCH" in
  ia32 )
    export ARCHITECTURE="i386"
    ;;
  x64 )
    export ARCHITECTURE="amd64"
    ;;
  * )
    echo
    echo "ERROR: Don't know how to build DEBs for '$TARGETARCH'."
    echo
    exit 1
    ;;
esac
# TODO(michaelpg): Get a working repo URL.
BASEREPOCONFIG="dl.google.com/linux/app-shell/deb/ stable main"
# Only use the default REPOCONFIG if it's unset (e.g. verify_channel might have
# set it to an empty string)
REPOCONFIG="${REPOCONFIG-deb [arch=${ARCHITECTURE}] http://${BASEREPOCONFIG}}"
# Allowed configs include optional HTTPS support and explicit multiarch
# platforms.
REPOCONFIGREGEX="deb (\\\\[arch=[^]]*\\\\b${ARCHITECTURE}\\\\b[^]]*\\\\]"
REPOCONFIGREGEX+="[[:space:]]*) https?://${BASEREPOCONFIG}"
stage_install_debian

do_package
verify_package
