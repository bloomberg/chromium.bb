#!/bin/bash

# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Called by the Keystone system to update the installed application with a new
# version from a disk image.

# Return values:
# 0  Happiness
# 1  Unknown failure
# 2  Basic sanity check destination failure (e.g. ticket points to nothing)
# 3  Could not prepare existing installed version to receive update
# 4  rsync failed (could not assure presence of Versions directory)
# 5  rsync failed (could not copy new versioned directory to Versions)
# 6  rsync failed (could not update outer .app bundle)
# 7  Could not get the version, update URL, or channel after update
# 8  Updated application does not have the version number from the update
# 9  ksadmin failure
# 10 Basic sanity check source failure (e.g. no app on disk image)

set -e

# Returns 0 (true) if the parameter exists, is a symbolic link, and appears
# writeable on the basis of its POSIX permissions.  This is used to determine
# writeability like test's -w primary, but -w resolves symbolic links and this
# function does not.
function is_writeable_symlink() {
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

# If SYMLINK exists and is a symbolic link, but is not writeable according to
# is_writeable_symlink, this function attempts to replace it with a new
# writeable symbolic link.  If FROM does not exist, is not a symbolic link, or
# is already writeable, this function does nothing.  This function always
# returns 0 (true).
function ensure_writeable_symlink() {
  SYMLINK=${1}
  if [ -L "${SYMLINK}" ] && ! is_writeable_symlink "${SYMLINK}" ; then
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

# The argument should be the disk image path.  Make sure it exists.
if [ $# -lt 1 ] || [ ! -d "${1}" ]; then
  exit 10
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
  exit 10
fi

# Figure out where we're going.  Determine the application version to be
# installed, use that to locate the framework, and then look inside the
# framework for the Keystone product ID.
APP_VERSION_KEY="CFBundleShortVersionString"
UPD_VERSION_APP=$(defaults read "${SRC}/Contents/Info" "${APP_VERSION_KEY}" ||
                  exit 10)
UPD_KS_PLIST="${SRC}/Contents/Versions/${UPD_VERSION_APP}/${FRAMEWORK_DIR}/Resources/Info"
KS_VERSION_KEY="KSVersion"
UPD_VERSION_KS=$(defaults read "${UPD_KS_PLIST}" "${KS_VERSION_KEY}" || exit 10)
PRODUCT_ID=$(defaults read "${UPD_KS_PLIST}" KSProductID || exit 10)
if [ -z "${UPD_VERSION_KS}" ] || [ -z "${PRODUCT_ID}" ] ; then
  exit 2
fi
DEST=$(ksadmin -pP "${PRODUCT_ID}" |
       sed -Ene \
           's%^[[:space:]]+xc=<KSPathExistenceChecker:.* path=(/.+)>$%\1%p')

# More sanity checking.
if [ -z "${DEST}" ] || [ ! -d "${DEST}" ]; then
  exit 2
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
    ensure_writeable_symlink "${DESTLINK}"
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
mkdir -p "${DEST}/Contents" || exit 3
rsync ${RSYNC_FLAGS} --exclude "*" "${SRC}/Contents/Versions/" \
                                   "${DEST}/Contents/Versions" || exit 4

# Copy the versioned directory.  The new versioned directory will have a
# different name than any existing one, so this won't harm anything already
# present in Contents/Versions, including the versioned directory being used
# by any running processes.  If this step is interrupted, there will be an
# incomplete versioned directory left behind, but it won't interfere with
# anything, and it will be replaced or removed during a future update attempt.
NEW_VERSIONED_DIR="${DEST}/Contents/Versions/${UPD_VERSION_APP}"
rsync ${RSYNC_FLAGS} --delete-before \
    "${SRC}/Contents/Versions/${UPD_VERSION_APP}/" \
    "${NEW_VERSIONED_DIR}" || exit 5

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
    "${SRC}/" "${DEST}" || exit 6

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
                  exit 7)
NEW_KS_PLIST="${DEST}/Contents/Versions/${NEW_VERSION_APP}/${FRAMEWORK_DIR}/Resources/Info"
NEW_VERSION_KS=$(defaults read "${NEW_KS_PLIST}" "${KS_VERSION_KEY}" || exit 7)
URL=$(defaults read "${NEW_KS_PLIST}" KSUpdateURL || exit 7)
# The channel ID is optional.  Suppress stderr to prevent Keystone from seeing
# possible error output.
CHANNEL_ID=$(defaults read "${NEW_KS_PLIST}" KSChannelID 2>/dev/null || true)

# Make sure that the update was successful by comparing the version found in
# the update with the version now on disk.
if [ "${NEW_VERSION_KS}" != "${UPD_VERSION_KS}" ]; then
  exit 8
fi

# Notify LaunchServices.  This is not considered a critical step, and
# lsregister's exit codes shouldn't be confused with this script's own.
/System/Library/Frameworks/CoreServices.framework/Frameworks/LaunchServices.framework/Support/lsregister "${DEST}" || true

# Notify Keystone.
KSADMIN_VERSION=$(ksadmin --ksadmin-version || true)
if [ -n "${KSADMIN_VERSION}" ] ; then
  # If ksadmin recognizes --ksadmin-version, it will recognize --tag.
  ksadmin --register \
          -P "${PRODUCT_ID}" \
          --version "${NEW_VERSION_KS}" \
          --xcpath "${DEST}" \
          --url "${URL}" \
          --tag "${CHANNEL_ID}" || exit 9
else
  # Older versions of ksadmin don't recognize --tag.  The application will
  # set the tag when it runs.
  ksadmin --register \
          -P "${PRODUCT_ID}" \
          --version "${NEW_VERSION_KS}" \
          --xcpath "${DEST}" \
          --url "${URL}" || exit 9
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
# /Applications, try to make it writeable by all admin users.  This will allow
# other admin users to update the application from their own user Keystone
# instances.
#
# If the script is not running as root and the application is not installed
# under /Applications, it might not be in a system-wide location, and it
# probably won't be something that other users on the system are running, so
# err on the side of safety and don't make it group-writeable.
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
# world-writeable.
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
