#!/bin/bash

# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Called by the Keystone system to update the installed application with a new
# version from a disk image.

# Return values:
# 0  Happiness
# 1  Unknown failure
# 2  Basic sanity check source failure (e.g. no app on disk image)
# 3  Basic sanity check destination failure (e.g. ticket points to nothing)
# 4  Update driven by user ticket when a system ticket is also present
# 5  Could not prepare existing installed version to receive update
# 6  rsync failed (could not assure presence of Versions directory)
# 7  rsync failed (could not copy new versioned directory to Versions)
# 8  rsync failed (could not update outer .app bundle)
# 9  Could not get the version, update URL, or channel after update
# 10 Updated application does not have the version number from the update
# 11 ksadmin failure

set -e

# http://b/2290916: Keystone runs the installation with a restrictive PATH
# that only includes the directory containing ksadmin, /bin, and /usr/bin.  It
# does not include /sbin or /usr/sbin.  This script uses lsof, which is in
# /usr/sbin, and it's conceivable that it might want to use other tools in an
# sbin directory.  Adjust the path accordingly.
export PATH="${PATH}:/sbin:/usr/sbin"

# Returns 0 (true) if the parameter exists, is a symbolic link, and appears
# writable on the basis of its POSIX permissions.  This is used to determine
# writeability like test's -w primary, but -w resolves symbolic links and this
# function does not.
function is_writable_symlink() {
  SYMLINK=${1}
  LINKMODE=$(stat -f %Sp "${SYMLINK}" 2> /dev/null || true)
  if [ -z "${LINKMODE}" ] || [ "${LINKMODE:0:1}" != "l" ] ; then
    return 1
  fi
  LINKUSER=$(stat -f %u "${SYMLINK}" 2> /dev/null || true)
  LINKGROUP=$(stat -f %g "${SYMLINK}" 2> /dev/null || true)
  if [ -z "${LINKUSER}" ] || [ -z "${LINKGROUP}" ] ; then
    return 1
  fi

  # If the users match, check the owner-write bit.
  if [ ${EUID} -eq ${LINKUSER} ] ; then
    if [ "${LINKMODE:2:1}" = "w" ] ; then
      return 0
    fi
    return 1
  fi

  # If the file's group matches any of the groups that this process is a
  # member of, check the group-write bit.
  GROUPMATCH=
  for group in ${GROUPS[@]} ; do
    if [ ${group} -eq ${LINKGROUP} ] ; then
      GROUPMATCH=1
      break
    fi
  done
  if [ -n "${GROUPMATCH}" ] ; then
    if [ "${LINKMODE:5:1}" = "w" ] ; then
      return 0
    fi
    return 1
  fi

  # Check the other-write bit.
  if [ "${LINKMODE:8:1}" = "w" ] ; then
    return 0
  fi
  return 1
}

# If SYMLINK exists and is a symbolic link, but is not writable according to
# is_writable_symlink, this function attempts to replace it with a new
# writable symbolic link.  If FROM does not exist, is not a symbolic link, or
# is already writable, this function does nothing.  This function always
# returns 0 (true).
function ensure_writable_symlink() {
  SYMLINK=${1}
  if [ -L "${SYMLINK}" ] && ! is_writable_symlink "${SYMLINK}" ; then
    # If ${SYMLINK} refers to a directory, doing this naively might result in
    # the new link being placed in that directory, instead of replacing the
    # existing link.  ln -fhs is supposed to handle this case, but it does so
    # by unlinking (removing) the existing symbolic link before creating a new
    # one.  That leaves a small window during which the symbolic link is not
    # present on disk at all.
    #
    # To avoid that possibility, a new symbolic link is created in a temporary
    # location and then swapped into place with mv.  An extra temporary
    # directory is used to convince mv to replace the symbolic link: again, if
    # the existing link refers to a directory, "mv newlink oldlink" will
    # actually leave oldlink alone and place newlink into the directory.
    # "mv newlink dirname(oldlink)" works as expected, but in order to replace
    # oldlink, newlink must have the same basename, hence the temporary
    # directory.

    TARGET=$(readlink "${SYMLINK}" 2> /dev/null || true)
    if [ -z "${TARGET}" ] ; then
      return 0
    fi

    SYMLINKDIR=$(dirname "${SYMLINK}")
    TEMPLINKDIR="${SYMLINKDIR}/.symlink_temp.${$}.${RANDOM}"
    TEMPLINK="${TEMPLINKDIR}/$(basename "${SYMLINK}")"

    # Don't bail out here if this fails.  Something else will probably fail.
    # Let it, it'll probably be easier to understand that failure than this
    # one.
    (mkdir "${TEMPLINKDIR}" && \
        ln -fhs "${TARGET}" "${TEMPLINK}" && \
        chmod -h 755 "${TEMPLINK}" && \
        mv -f "${TEMPLINK}" "${SYMLINKDIR}") || true
    rm -rf "${TEMPLINKDIR}"
  fi

  return 0
}

# Prints the version of ksadmin, as reported by ksadmin --ksadmin-version, to
# stdout.  This function operates with "static" variables: it will only check
# the ksadmin version once per script run.  If ksadmin is old enough to not
# support --ksadmin-version, or another error occurs, this function prints an
# empty string.
G_CHECKED_KSADMIN_VERSION=
G_KSADMIN_VERSION=
function ksadmin_version() {
  if [ -z "${G_CHECKED_KSADMIN_VERSION}" ] ; then
    G_CHECKED_KSADMIN_VERSION=1
    G_KSADMIN_VERSION=$(ksadmin --ksadmin-version || true)
  fi
  echo "${G_KSADMIN_VERSION}"
  return 0
}

# Compares the installed ksadmin version against a supplied version number,
# and returns 0 (true) if the number to check is the same as or newer than the
# installed Keystone.  Returns 1 (false) if the installed Keystone version
# number cannot be determined or if the number to check is less than the
# installed Keystone.  The check argument should be a string of the form
# "major.minor.micro.build".
function is_ksadmin_version_ge() {
  CHECK_VERSION=${1}
  KSADMIN_VERSION=$(ksadmin_version)
  if [ -n "${KSADMIN_VERSION}" ] ; then
    VER_RE='^([0-9]+)\.([0-9]+)\.([0-9]+)\.([0-9]+)$'

    KSADMIN_VERSION_MAJOR=$(sed -Ene "s/${VER_RE}/\1/p" <<< ${KSADMIN_VERSION})
    KSADMIN_VERSION_MINOR=$(sed -Ene "s/${VER_RE}/\2/p" <<< ${KSADMIN_VERSION})
    KSADMIN_VERSION_MICRO=$(sed -Ene "s/${VER_RE}/\3/p" <<< ${KSADMIN_VERSION})
    KSADMIN_VERSION_BUILD=$(sed -Ene "s/${VER_RE}/\4/p" <<< ${KSADMIN_VERSION})

    CHECK_VERSION_MAJOR=$(sed -Ene "s/${VER_RE}/\1/p" <<< ${CHECK_VERSION})
    CHECK_VERSION_MINOR=$(sed -Ene "s/${VER_RE}/\2/p" <<< ${CHECK_VERSION})
    CHECK_VERSION_MICRO=$(sed -Ene "s/${VER_RE}/\3/p" <<< ${CHECK_VERSION})
    CHECK_VERSION_BUILD=$(sed -Ene "s/${VER_RE}/\4/p" <<< ${CHECK_VERSION})

    if [ ${KSADMIN_VERSION_MAJOR} -gt ${CHECK_VERSION_MAJOR} ] ||
       ([ ${KSADMIN_VERSION_MAJOR} -eq ${CHECK_VERSION_MAJOR} ] && (
           [ ${KSADMIN_VERSION_MINOR} -gt ${CHECK_VERSION_MINOR} ] ||
           ([ ${KSADMIN_VERSION_MINOR} -eq ${CHECK_VERSION_MINOR} ] && (
               [ ${KSADMIN_VERSION_MICRO} -gt ${CHECK_VERSION_MICRO} ] ||
               ([ ${KSADMIN_VERSION_MICRO} -eq ${CHECK_VERSION_MICRO} ] &&
                [ ${KSADMIN_VERSION_BUILD} -ge ${CHECK_VERSION_BUILD} ])
           ))
       )) ; then
      return 0
    fi
  fi

  return 1
}

# Returns 0 (true) if ksadmin supports --tag.
function ksadmin_supports_tag() {
  KSADMIN_VERSION=$(ksadmin_version)
  if [ -n "${KSADMIN_VERSION}" ] ; then
    # A ksadmin that recognizes --ksadmin-version and provides a version
    # number is new enough to recognize --tag.
    return 0
  fi
  return 1
}

# Returns 0 (true) if ksadmin supports --tag-path and --tag-key.
function ksadmin_supports_tagpath_tagkey() {
  # --tag-path and --tag-key were introduced in Keystone 1.0.7.1306.
  is_ksadmin_version_ge 1.0.7.1306
  # The return value of is_ksadmin_version_ge is used as this function's
  # return value.
}

# The argument should be the disk image path.  Make sure it exists.
if [ $# -lt 1 ] || [ ! -d "${1}" ]; then
  exit 2
fi

# Who we are.
PRODUCT_NAME="Google Chrome"
APP_DIR="${PRODUCT_NAME}.app"
FRAMEWORK_NAME="${PRODUCT_NAME} Framework"
FRAMEWORK_DIR="${FRAMEWORK_NAME}.framework"
SRC="${1}/${APP_DIR}"

# Make sure that there's something to copy from, and that it's an absolute
# path.
if [ -z "${SRC}" ] || [ "${SRC:0:1}" != "/" ] || [ ! -d "${SRC}" ] ; then
  exit 2
fi

# Figure out where we're going.  Determine the application version to be
# installed, use that to locate the framework, and then look inside the
# framework for the Keystone product ID.
APP_VERSION_KEY="CFBundleShortVersionString"
UPD_VERSION_APP=$(defaults read "${SRC}/Contents/Info" "${APP_VERSION_KEY}" ||
                  exit 2)
UPD_KS_PLIST="${SRC}/Contents/Versions/${UPD_VERSION_APP}/${FRAMEWORK_DIR}/Resources/Info"
KS_VERSION_KEY="KSVersion"
UPD_VERSION_KS=$(defaults read "${UPD_KS_PLIST}" "${KS_VERSION_KEY}" || exit 2)
PRODUCT_ID=$(defaults read "${UPD_KS_PLIST}" KSProductID || exit 2)
if [ -z "${UPD_VERSION_KS}" ] || [ -z "${PRODUCT_ID}" ] ; then
  exit 3
fi
DEST=$(ksadmin -pP "${PRODUCT_ID}" |
       sed -Ene \
           's%^[[:space:]]+xc=<KSPathExistenceChecker:.* path=(/.+)>$%\1%p')

# More sanity checking.
if [ -z "${DEST}" ] || [ ! -d "${DEST}" ]; then
  exit 3
fi

# If this script is not running as root, it's being driven by a user ticket.
# If a system ticket is also present, there's a potential for the two to
# collide.  Both ticket types might be present if another user on the system
# promoted the ticket to system: the other user could not have removed this
# user's user ticket.  Handle that case here by deleting the user ticket and
# exiting early with a discrete exit status.
#
# Current versions of ksadmin will exit 1 (false) when asked to print tickets
# and given a specific product ID to print.  Older versions of ksadmin would
# exit 0 (true), but those same versions did not support -S (meaning to check
# the system ticket store) and would exit 1 (false) with this invocation due
# to not understanding the question.  Therefore, the usage here will only
# delete the existing user ticket when running as non-root with access to a
# sufficiently recent ksadmin.  Older ksadmins are tolerated: the update will
# likely fail for another reason and the user ticket will hang around until
# something is eventually able to remove it.
if [ ${EUID} -ne 0 ] &&
   ksadmin -S --print-tickets -P "${PRODUCT_ID}" >& /dev/null ; then
  ksadmin --delete -P "${PRODUCT_ID}" || true
  exit 4
fi

# Figure out what the existing version is using for its versioned directory.
# This will be used later, to avoid removing the currently-installed version's
# versioned directory in case anything is still using it.
OLD_VERSION_APP=$(defaults read "${DEST}/Contents/Info" "${APP_VERSION_KEY}" ||
                  true)
OLD_VERSIONED_DIR="${DEST}/Contents/Versions/${OLD_VERSION_APP}"

# See if the timestamp of what's currently on disk is newer than the update's
# outer .app's timestamp.  rsync will copy the update's timestamp over, but
# if that timestamp isn't as recent as what's already on disk, the .app will
# need to be touched.
NEEDS_TOUCH=
if [ "${DEST}" -nt "${SRC}" ] ; then
  NEEDS_TOUCH=1
fi

# In some very weird and rare cases, it is possible to wind up with a user
# installation that contains symbolic links that the user does not have write
# permission over.  More on how that might happen later.
#
# If a weird and rare case like this is observed, rsync will exit with an
# error when attempting to update the times on these symbolic links.  rsync
# may not be intelligent enough to try creating a new symbolic link in these
# cases, but this script can be.
#
# This fix-up is not necessary when running as root, because root will always
# be able to write everything needed.
#
# The problem occurs when an administrative user first drag-installs the
# application to /Applications, resulting in the program's user being set to
# the user's own ID.  If, subsequently, a .pkg package is installed over that,
# the existing directory ownership will be preserved, but file ownership will
# be changed to whateer is specified by the package, typically root.  This
# applies to symbolic links as well.  On a subsequent update, rsync will
# be able to copy the new files into place, because the user still has
# permission to write to the directories.  If the symbolic link targets are
# not changing, though, rsync will not replace them, and they will remain
# owned by root.  The user will not have permission to update the time on
# the symbolic links, resulting in an rsync error.
if [ ${EUID} -ne 0 ] ; then
  # This step isn't critical.
  set +e

  # Reset ${IFS} to deal with spaces in the for loop by not breaking the
  # list up when they're encountered.
  IFS_OLD="${IFS}"
  IFS=$(printf '\n\t')

  # Only consider symbolic links in ${SRC}.  If there are any other links in
  # ${DEST} not present in ${SRC}, rsync will delete them as needed later.
  LINKS=$(cd "${SRC}" && find . -type l)

  for link in ${LINKS} ; do
    # ${link} is relative to ${SRC}.  Prepending ${DEST} looks for the same
    # link already on disk.
    DESTLINK="${DEST}/${link}"
    ensure_writable_symlink "${DESTLINK}"
  done

  # Go back to how things were.
  IFS="${IFS_OLD}"
  set -e
fi

# Don't use rsync -a, because -a expands to -rlptgoD.  -g and -o copy owners
# and groups, respectively, from the source, and that is undesirable in this
# case.  -D copies devices and special files; copying devices only works
# when running as root, so for consistency between privileged and unprivileged
# operation, this option is omitted as well.
#  -I, --ignore-times          don't skip files that match in size and mod-time
#  -l, --links                 copy symlinks as symlinks
#  -r, --recursive             recurse into directories
#  -p, --perms                 preserve permissions
#  -t, --times                 preserve times
RSYNC_FLAGS="-Ilprt"

# By copying to ${DEST}, the existing application name will be preserved, even
# if the user has renamed the application on disk.  Respecting the user's
# changes is friendly.

# Make sure that the Versions directory exists, so that it can receive the
# versioned directory.  It may not exist if updating from an older version
# that did not use the versioned layout on disk.  An rsync that excludes all
# contents is used to bring the permissions over from the update's Versions
# directory, otherwise, this directory would be the only one in the entire
# update exempt from getting its permissions copied over.  A simple mkdir
# wouldn't copy mode bits.  This is done even if ${DEST}/Contents/Versions
# already does exist to ensure that the mode bits come from the update.
#
# ${DEST} is guaranteed to exist at this point, but ${DEST}/Contents may not
# if things are severely broken or if this update is actually an initial
# installation from a Keystone skeleton bootstrap.  The mkdir creates
# ${DEST}/Contents if it doesn't exist; its mode bits will be fixed up in a
# subsequent rsync.
mkdir -p "${DEST}/Contents" || exit 5
rsync ${RSYNC_FLAGS} --exclude "*" "${SRC}/Contents/Versions/" \
                                   "${DEST}/Contents/Versions" || exit 6

# Copy the versioned directory.  The new versioned directory will have a
# different name than any existing one, so this won't harm anything already
# present in Contents/Versions, including the versioned directory being used
# by any running processes.  If this step is interrupted, there will be an
# incomplete versioned directory left behind, but it won't interfere with
# anything, and it will be replaced or removed during a future update attempt.
NEW_VERSIONED_DIR="${DEST}/Contents/Versions/${UPD_VERSION_APP}"
rsync ${RSYNC_FLAGS} --delete-before \
    "${SRC}/Contents/Versions/${UPD_VERSION_APP}/" \
    "${NEW_VERSIONED_DIR}" || exit 7

# Copy the unversioned files into place, leaving everything in
# Contents/Versions alone.  If this step is interrupted, the application will
# at least remain in a usable state, although it may not pass signature
# validation.  Depending on when this step is interrupted, the application
# will either launch the old or the new version.  The critical point is when
# the main executable is replaced.  There isn't very much to copy in this step,
# because most of the application is in the versioned directory.  This step
# only accounts for around 50 files, most of which are small localized
# InfoPlist.strings files.
rsync ${RSYNC_FLAGS} --delete-after --exclude /Contents/Versions \
    "${SRC}/" "${DEST}" || exit 8

# If necessary, touch the outermost .app so that it appears to the outside
# world that something was done to the bundle.  This will cause LaunchServices
# to invalidate the information it has cached about the bundle even if
# lsregister does not run.  This is not done if rsync already updated the
# timestamp to something newer than what had been on disk.  This is not
# considered a critical step, and if it fails, this script will not exit.
if [ -n "${NEEDS_TOUCH}" ] ; then
  touch -cf "${DEST}" || true
fi

# Read the new values (e.g. version).  Get the installed application version
# to get the path to the framework, where the Keystone keys are stored.
NEW_VERSION_APP=$(defaults read "${DEST}/Contents/Info" "${APP_VERSION_KEY}" ||
                  exit 9)
NEW_KS_PLIST="${DEST}/Contents/Versions/${NEW_VERSION_APP}/${FRAMEWORK_DIR}/Resources/Info"
NEW_VERSION_KS=$(defaults read "${NEW_KS_PLIST}" "${KS_VERSION_KEY}" || exit 9)
URL=$(defaults read "${NEW_KS_PLIST}" KSUpdateURL || exit 9)
# The channel ID is optional.  Suppress stderr to prevent Keystone from seeing
# possible error output.
CHANNEL_ID_KEY=KSChannelID
CHANNEL_ID=$(defaults read "${NEW_KS_PLIST}" "${CHANNEL_ID_KEY}" 2>/dev/null ||
             true)

# Make sure that the update was successful by comparing the version found in
# the update with the version now on disk.
if [ "${NEW_VERSION_KS}" != "${UPD_VERSION_KS}" ]; then
  exit 10
fi

# Notify LaunchServices.  This is not considered a critical step, and
# lsregister's exit codes shouldn't be confused with this script's own.
/System/Library/Frameworks/CoreServices.framework/Frameworks/LaunchServices.framework/Support/lsregister "${DEST}" || true

# Call ksadmin_version once to prime the global state.  This is needed because
# subsequent calls to ksadmin_version that occur in $(...) expansions will not
# affect the global state (although they can read from the already-initialized
# global state) and thus will cause a new ksadmin --ksadmin-version process to
# run for each check unless the globals have been properly initialized
# beforehand.
ksadmin_version >& /dev/null || true

# Notify Keystone.
if ksadmin_supports_tagpath_tagkey ; then
  ksadmin --register \
          -P "${PRODUCT_ID}" \
          --version "${NEW_VERSION_KS}" \
          --xcpath "${DEST}" \
          --url "${URL}" \
          --tag "${CHANNEL_ID}" \
          --tag-path "${DEST}/Contents/Info.plist" \
          --tag-key "${CHANNEL_ID_KEY}" || exit 11
elif ksadmin_supports_tag ; then
  ksadmin --register \
          -P "${PRODUCT_ID}" \
          --version "${NEW_VERSION_KS}" \
          --xcpath "${DEST}" \
          --url "${URL}" \
          --tag "${CHANNEL_ID}" || exit 11
else
  ksadmin --register \
          -P "${PRODUCT_ID}" \
          --version "${NEW_VERSION_KS}" \
          --xcpath "${DEST}" \
          --url "${URL}" || exit 11
fi

# The remaining steps are not considered critical.
set +e

# Try to clean up old versions that are not in use.  The strategy is to keep
# the versioned directory corresponding to the update just applied
# (obviously) and the version that was just replaced, and to use ps and lsof
# to see if it looks like any processes are currently using any other old
# directories.  Directories not in use are removed.  Old versioned directories
# that are in use are left alone so as to not interfere with running
# processes.  These directories can be cleaned up by this script on future
# updates.
#
# To determine which directories are in use, both ps and lsof are used.  Each
# approach has limitations.
#
# The ps check looks for processes within the verisoned directory.  Only
# helper processes, such as renderers, are within the versioned directory.
# Browser processes are not, so the ps check will not find them, and will
# assume that a versioned directory is not in use if a browser is open without
# any windows.  The ps mechanism can also only detect processes running on the
# system that is performing the update.  If network shares are involved, all
# bets are off.
#
# The lsof check looks to see what processes have the framework dylib open.
# Browser processes will have their versioned framework dylib open, so this
# check is able to catch browsers even if there are no associated helper
# processes.  Like the ps check, the lsof check is limited to processes on
# the system that is performing the update.  Finally, unless running as root,
# the lsof check can only find processes running as the effective user
# performing the update.
#
# These limitations are motiviations to additionally preserve the versioned
# directory corresponding to the version that was just replaced.

# Set the nullglob option.  This causes a glob pattern that doesn't match
# any files to expand to an empty string, instead of expanding to the glob
# pattern itself.  This means that if /path/* doesn't match anything, it will
# expand to "" instead of, literally, "/path/*".  The glob used in the loop
# below is not expected to expand to nothing, but nullglob will prevent the
# loop from trying to remove nonexistent directories by weird names with
# funny characters in them.
shopt -s nullglob

for versioned_dir in "${DEST}/Contents/Versions/"* ; do
  if [ "${versioned_dir}" = "${NEW_VERSIONED_DIR}" ] || \
     [ "${versioned_dir}" = "${OLD_VERSIONED_DIR}" ] ; then
    # This is the versioned directory corresponding to the update that was
    # just applied or the version that was previously in use.  Leave it alone.
    continue
  fi

  # Look for any processes whose executables are within this versioned
  # directory.  They'll be helper processes, such as renderers.  Their
  # existence indicates that this versioned directory is currently in use.
  PS_STRING="${versioned_dir}/"

  # Look for any processes using the framework dylib.  This will catch
  # browser processes where the ps check will not, but it is limited to
  # processes running as the effective user.
  LSOF_FILE="${versioned_dir}/${FRAMEWORK_DIR}/${FRAMEWORK_NAME}"

  # ps -e displays all users' processes, -ww causes ps to not truncate lines,
  # -o comm instructs it to only print the command name, and the = tells it to
  # not print a header line.
  # The cut invocation filters the ps output to only have at most the number
  # of characters in ${PS_STRING}.  This is done so that grep can look for an
  # exact match.
  # grep -F tells grep to look for lines that are exact matches (not regular
  # expressions), -q tells it to not print any output and just indicate
  # matches by exit status, and -x tells it that the entire line must match
  # ${PS_STRING} exactly, as opposed to matching a substring.  A match
  # causes grep to exit zero (true).
  #
  # lsof will exit nonzero if ${LSOF_FILE} does not exist or is open by any
  # process.  If the file exists and is open, it will exit zero (true).
  if (! ps -ewwo comm= | \
        cut -c "1-${#PS_STRING}" | \
        grep -Fqx "${PS_STRING}") &&
     (! lsof "${LSOF_FILE}" >& /dev/null) ; then
    # It doesn't look like anything is using this versioned directory.  Get rid
    # of it.
    rm -rf "${versioned_dir}"
  fi
done

# If this script is not running as root (indicating an update driven by a user
# Keystone ticket) and the application is installed somewhere under
# /Applications, try to make it writable by all admin users.  This will allow
# other admin users to update the application from their own user Keystone
# instances.
#
# If the script is not running as root and the application is not installed
# under /Applications, it might not be in a system-wide location, and it
# probably won't be something that other users on the system are running, so
# err on the side of safety and don't make it group-writable.
#
# If this script is running as root, it's driven by a system Keystone ticket,
# and future updates can be expected to be applied the same way, so
# admin-writeability is not a concern.  Set the entire thing to be owned by
# root in that case, regardless of where it's installed, and drop any group
# and other write permission.
#
# If this script is running as a user that is not a member of the admin group,
# the chgrp operation will not succeed.  Tolerate that case, because it's
# better than the alternative, which is to make the application
# world-writable.
CHMOD_MODE="a+rX,u+w,go-w"
if [ ${EUID} -ne 0 ] ; then
  if [ "${DEST:0:14}" = "/Applications/" ] &&
     chgrp -Rh admin "${DEST}" >& /dev/null ; then
    CHMOD_MODE="a+rX,ug+w,o-w"
  fi
else
  chown -Rh root:wheel "${DEST}" >& /dev/null
fi

chmod -R "${CHMOD_MODE}" "${DEST}" >& /dev/null

# On the Mac, or at least on HFS+, symbolic link permissions are significant,
# but chmod -R and -h can't be used together.  Do another pass to fix the
# permissions on any symbolic links.
find "${DEST}" -type l -exec chmod -h "${CHMOD_MODE}" {} + >& /dev/null

# Host OS version check, to be able to take advantage of features on newer
# systems and fall back to slow ways of doing things on older systems.
OS_VERSION=$(sw_vers -productVersion)
OS_MAJOR=$(sed -Ene 's/^([0-9]+).*/\1/p' <<< ${OS_VERSION})
OS_MINOR=$(sed -Ene 's/^([0-9]+)\.([0-9]+).*/\2/p' <<< ${OS_VERSION})

# If an update is triggered from within the application itself, the update
# process inherits the quarantine bit (LSFileQuarantineEnabled).  Any files or
# directories created during the update will be quarantined in that case,
# which may cause Launch Services to display quarantine UI.  That's bad,
# especially if it happens when the outer .app launches a quarantined inner
# helper.  If the application is already on the system and is being updated,
# then it can be assumed that it should not be quarantined.  Use xattr to drop
# the quarantine attribute.
#
# TODO(mark): Instead of letting the quarantine attribute be set and then
# dropping it here, figure out a way to get the update process to run without
# LSFileQuarantineEnabled even when triggering an update from within the
# application.
QUARANTINE_ATTR=com.apple.quarantine
if [ ${OS_MAJOR} -gt 10 ] ||
   ([ ${OS_MAJOR} -eq 10 ] && [ ${OS_MINOR} -ge 6 ]) ; then
  # On 10.6, xattr supports -r for recursive operation.
  xattr -d -r "${QUARANTINE_ATTR}" "${DEST}" >& /dev/null
else
  # On earlier systems, xattr doesn't support -r, so run xattr via find.
  find "${DEST}" -exec xattr -d "${QUARANTINE_ATTR}" {} + >& /dev/null
fi

# Great success!
exit 0
