# -*- coding: utf-8 -*-
# Copyright 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Library for validating ebuild license information, and generating credits.

Documentation on this script is also available here:
  https://dev.chromium.org/chromium-os/licensing
"""

from __future__ import print_function

import cgi
import codecs
import os
import re

from chromite.lib import constants
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import osutils
from chromite.lib import portage_util
from chromite.cbuildbot import manifest_version

# We are imported by src/repohooks/pre-upload.py in a non chroot environment
# where yaml may not be there, so we don't error on that since it's not needed
# in that case.
try:
  # pylint: disable=wrong-import-order
  import yaml
except ImportError:
  yaml = None

# See https://crbug.com/207004 for discussion.
PER_PKG_LICENSE_DIR = portage_util.VDB_PATH

STOCK_LICENSE_DIRS = [
    'src/third_party/portage-stable/licenses',
]

# There are licenses for custom software we got and isn't part of
# upstream gentoo.
CUSTOM_LICENSE_DIRS = [
    'src/third_party/chromiumos-overlay/licenses',
    'src/private-overlays/chromeos-overlay/licenses',
]

COPYRIGHT_ATTRIBUTION_DIR = (
    os.path.join(
        constants.SOURCE_ROOT,
        'src/third_party/chromiumos-overlay/licenses/copyright-attribution'))

# Virtual packages don't need to have a license and often don't, so we skip them
# chromeos-base contains google platform packages that are covered by the
# general license at top of tree, so we skip those too.
SKIPPED_CATEGORIES = [
    'virtual',
]

SKIPPED_LICENSES = [
    # Some of our packages contain binary blobs for which we have special
    # negotiated licenses, and no need to display anything publicly. Strongly
    # consider using Google-TOS instead, if possible.
    'Proprietary-Binary',

    # If you have an early repo for which license terms have yet to be decided
    # use this. It will cause licensing for the package to be mostly ignored.
    # Official should error for any package with this license.
    'TAINTED', # TODO(dgarrett): Error on official builds with this license.
]

LICENSE_NAMES_REGEX = [
    r'^copyright$',
    r'^copyright[.]txt$',
    r'^copyright[.]regex$',                        # llvm
    r'^copying.*$',
    r'^licen[cs]e.*$',
    r'^licensing.*$',                              # libatomic_ops
    r'^ipa_font_license_agreement_v1[.]0[.]txt$',  # ja-ipafonts
    r'^PKG-INFO$',                                 # copyright assignment for
                                                   # some python packages
                                                   # (netifaces, unittest2)
]

# These are _temporary_ license mappings for packages that do not have a valid
# shared/custom license, or LICENSE file we can use.
# Once this script runs earlier (during the package build process), it will
# block new source without a LICENSE file if the ebuild contains a license
# that requires copyright assignment (BSD and friends).
# At that point, new packages will get fixed to include LICENSE instead of
# adding workaround mappings like those below.
# The way you now fix copyright attribution cases create a custom file with the
# right license directly in COPYRIGHT_ATTRIBUTION_DIR.
PACKAGE_LICENSES = {
    # TODO: replace the naive license parsing code in this script with a hook
    # into portage's license parsing. See https://crbug.com/348779

    # Chrome (the browser) is complicated, it has a morphing license that is
    # either BSD-Google, or BSD-Google,Google-TOS depending on how it was
    # built. We bypass this problem for now by hardcoding the Google-TOS bit as
    # per ChromeOS with non free bits
    'chromeos-base/chromeos-chrome': ['BSD-Google', 'Google-TOS'],

    # Currently the code cannot parse LGPL-3 || ( LGPL-2.1 MPL-1.1 )
    # Currently the code cannot parse BSD-2 BSD || ( Artistic GPL-2 LGPL-2 )
    'dev-python/pycairo': ['LGPL-3', 'LGPL-2.1'],
    'dev-lang/yasm': ['BSD-2', 'GPL-2', 'LGPL-2'],

    # Currently the code cannot parse the license for mit-krb5
    # "openafs-krb5-a BSD MIT OPENLDAP BSD-2 HPND BSD-4 ISC RSA CC-BY-SA-3.0 ||
    #      ( BSD-2 GPL-2+ )"
    'app-crypt/mit-krb5': [
        'openafs-krb5-a',
        'BSD',
        'MIT',
        'OPENLDAP',
        'BSD-2',
        'HPND',
        'BSD-4',
        'ISC',
        'RSA',
        'CC-BY-SA-3.0',
        'GPL-2+',
    ],
}

# Any license listed list here found in the ebuild will make the code look for
# license files inside the package source code in order to get copyright
# attribution from them.
COPYRIGHT_ATTRIBUTION_LICENSES = [
    'BSD',    # requires distribution of copyright notice
    'BSD-2',  # so does BSD-2 http://opensource.org/licenses/BSD-2-Clause
    'BSD-3',  # and BSD-3? http://opensource.org/licenses/BSD-3-Clause
    'BSD-4',  # and 4?
    'BSD-with-attribution',
    'MIT',
    'MIT-with-advertising',
    'Old-MIT',
]

# The following licenses are not invalid or to show as a less helpful stock
# license, but it's better to look in the source code for a more specific
# license if there is one, but not an error if no better one is found.
# Note that you don't want to set just anything here since any license here
# will be included once in stock form and a second time in custom form if
# found (there is no good way to know that a license we found on disk is the
# better version of the stock version, so we show both).
LOOK_IN_SOURCE_LICENSES = [
    'as-is',  # The stock license is very vague, source always has more details.
    'PSF-2',  # The custom license in python is more complete than the template.

    # As far as I know, we have no requirement to do copyright attribution for
    # these licenses, but the license included in the code has slightly better
    # information than the stock Gentoo one (including copyright attribution).
    'BZIP2',     # Single use license, do copyright attribution.
    'OFL',       # Almost single use license, do copyright attribution.
    'OFL-1.1',   # Almost single use license, do copyright attribution.
    'UoI-NCSA',  # Only used by NSCA, might as well show their custom copyright.
]

# This used to provide overrides. I can't find a valid reason to add any more
# here, though.
PACKAGE_HOMEPAGES = {
    # Example:
    # 'x11-proto/glproto': ['http://www.x.org/'],
}

# These are tokens found in LICENSE= in an ebuild that aren't licenses we
# can actually read from disk.
# You should not use this to blacklist real licenses.
LICENCES_IGNORE = [
    ')',              # Ignore OR tokens from LICENSE="|| ( LGPL-2.1 MPL-1.1 )"
    '(',
    '||',
]

# The full names of packages which we want to generate license information for
# even though they have an empty installation size.
SIZE_EXEMPT_PACKAGES = [
    'net-print/konica-minolta-printing-license',
    'net-print/xerox-printing-license',
]

# Find the directory of this script.
SCRIPT_DIR = os.path.dirname(os.path.realpath(__file__))

# The template files we depend on for generating HTML.
TMPL = os.path.join(SCRIPT_DIR, 'about_credits.tmpl')
ENTRY_TMPL = os.path.join(SCRIPT_DIR, 'about_credits_entry.tmpl')
SHARED_LICENSE_TMPL = os.path.join(
    SCRIPT_DIR, 'about_credits_shared_license_entry.tmpl')

# dir_set constants for _GetLicenseDirectories.
_CUSTOM_DIRS = 'custom'
_STOCK_DIRS = 'stock'
_BOTH_DIRS = 'both'


# This is called directly by src/repohooks/pre-upload.py
def GetLicenseTypesFromEbuild(ebuild_contents, overlay_path,
                              buildroot=constants.SOURCE_ROOT):
  """Returns a list of license types from an ebuild.

  This function does not always return the correct list, but it is
  faster than using portageq for not having to access chroot. It is
  intended to be used for tasks such as presubmission checks.

  Args:
    ebuild_contents: contents of ebuild.
    overlay_path: path of the overlay containing the ebuild
    buildroot: base directory, usually constants.SOURCE_ROOT, useful for tests

  Returns:
    list of licenses read from ebuild.

  Raises:
    ValueError: ebuild errors.
  """
  ebuild_env_tmpl = """
has() { [[ " ${*:2} " == *" $1 "* ]]; }
inherit() {
  local overlay_list="%(overlay_list)s"
  local eclass overlay f
  for eclass; do
    has ${eclass} ${_INHERITED_} && continue
    _INHERITED_+=" ${eclass}"
    for overlay in %(overlay_list)s; do
      f="${overlay}/eclass/${eclass}.eclass"
      if [[ -e ${f} ]]; then
        source "${f}"
        break
      fi
     done
  done
}
%(ebuild)s"""

  repo_name = portage_util.GetOverlayName(overlay_path)
  overlays = portage_util.FindOverlays(constants.BOTH_OVERLAYS, repo_name,
                                       buildroot=buildroot)
  tmpl_env = {
      'ebuild': ebuild_contents,
      'overlay_list': ' '.join(overlays),
  }

  with cros_build_lib.UnbufferedNamedTemporaryFile() as f:
    osutils.WriteFile(f.name, ebuild_env_tmpl % tmpl_env)
    env = osutils.SourceEnvironment(
        f.name, whitelist=['LICENSE'], ifs=' ', multiline=True)

  if not env.get('LICENSE'):
    raise ValueError('No LICENSE found in the ebuild.')
  if re.search(r'[,;]', env['LICENSE']):
    raise ValueError(
        'LICENSE field in the ebuild should be whitespace-limited.')

  return env['LICENSE'].split()


class PackageLicenseError(Exception):
  """Thrown if something fails while getting license information for a package.

  This will cause the processing to error in the end.
  """


class PackageInfo(object):
  """Package specific information, mostly about licenses."""

  def __init__(self, board, fullnamerev):
    """Package info initializer.

    Args:
      board: The board this package was built for.
      fullnamerev: package name of the form 'x11-base/X.Org-1.9.3-r23'
    """

    self.board = board  # This field may be None, based on entry path.

    #
    # Populate these fields from fullnamerev:
    #   category, name, version, revision
    #
    try:
      cpv = portage_util.SplitCPV(fullnamerev)
    except ValueError:
      # A bad package can either raise a TypeError exception or return None.
      raise ValueError(
          "portage couldn't find %s, missing version number?" % fullnamerev)

    #
    # These define the package uniquely.
    #

    self.category, self.name, self.version, self.revision = (
        cpv.category, cpv.package, cpv.version_no_rev, cpv.rev)

    if self.revision is not None:
      self.revision = str(self.revision).lstrip('r')

    #
    # These fields hold license information used to generate the credits page.
    #

    # This contains licenses names for this package.
    self.license_names = set()

    # Full Text of discovered license information.
    self.license_text_scanned = []

    self.homepages = []

    #
    # These fields show the results of processing.
    #

    # After reading basic package information, we can mark the package as
    # one to skip in licensing.
    self.skip = False

    # Intellegently populate initial skip information.
    self.LookForSkip()

  @property
  def fullnamerev(self):
    """e.g. libnl/libnl-3.2.24-r12"""
    s = '%s-%s' % (self.fullname, self.version)
    if self.revision:
      s += '-r%s' % self.revision
    return s

  @property
  def fullname(self):
    """e.g. libnl/libnl-3.2.24"""
    return '%s/%s' % (self.category, self.name)

  @property
  def license_dump_path(self):
    """e.g. /build/x86-alex/var/db/pkg/sys-apps/dtc-1.4.0/license.yaml.

    Only valid for packages that have already been emerged.
    """
    return os.path.join(cros_build_lib.GetSysroot(self.board),
                        PER_PKG_LICENSE_DIR, self.fullnamerev, 'license.yaml')

  def _RunEbuildPhases(self, ebuild_path, phases):
    """Run a list of ebuild phases on an ebuild.

    Args:
      ebuild_path: exact path of the ebuild file.
      phases: list of phases like ['clean', 'fetch'] or ['unpack'].

    Returns:
      ebuild command output
    """
    ebuild_cmd = cros_build_lib.GetSysrootToolPath(
        cros_build_lib.GetSysroot(self.board), 'ebuild')
    return cros_build_lib.run(
        [ebuild_cmd, ebuild_path] + phases,
        stdout=True, encoding='utf-8')

  def _GetOverrideLicense(self):
    """Look in COPYRIGHT_ATTRIBUTION_DIR for license with copyright attribution.

    For dev-util/bsdiff-4.3-r5, the code will look for
    dev-util/bsdiff-4.3-r5
    dev-util/bsdiff-4.3
    dev-util/bsdiff

    It is ok to have more than one bsdiff license file, and an empty file acts
    as a rubout (i.e. an empty dev-util/bsdiff-4.4 will shadow dev-util/bsdiff
    and tell the licensing code to look in the package source for a license
    instead of using dev-util/bsdiff as an override).

    Returns:
      False (no license found) or a multiline license string.
    """
    license_read = None
    # dev-util/bsdiff-4.3-r5 -> bsdiff-4.3-r5
    filename = os.path.basename(self.fullnamerev)
    license_path = os.path.join(COPYRIGHT_ATTRIBUTION_DIR,
                                os.path.dirname(self.fullnamerev))
    pv = portage_util.SplitPV(filename)
    pv_no_rev = '%s-%s' % (pv.package, pv.version_no_rev)
    for filename in (pv.pv, pv_no_rev, pv.package):
      file_path = os.path.join(license_path, filename)
      logging.debug('Looking for override copyright attribution license in %s',
                    file_path)
      if os.path.exists(file_path):
        # Turn
        # /../merlin/trunk/src/third_party/chromiumos-overlay/../dev-util/bsdiff
        # into
        # chromiumos-overlay/../dev-util/bsdiff
        short_dir_path = os.path.join(*file_path.rsplit(os.path.sep, 5)[1:])
        license_read = 'Copyright Attribution License %s:\n\n' % short_dir_path
        license_read += ReadUnknownEncodedFile(
            file_path, 'read copyright attribution license')
        break

    return license_read

  def _ExtractLicenses(self, src_dir, need_copyright_attribution):
    """Scrounge for text licenses in the source of package we'll unpack.

    This is only called if we couldn't get usable licenses from the ebuild,
    or one of them is BSD/MIT like which forces us to look for a file with
    copyright attribution in the source code itself.

    First, we have a shortcut where we scan COPYRIGHT_ATTRIBUTION_DIR to see if
    we find a license for this package. If so, we use that.
    Typically it'll be used if the unpacked source does not have the license
    that we're required to display for copyright attribution (in some cases it's
    plain absent, in other cases, it could be in a filename we don't look for).

    Otherwise, we scan the unpacked source code for what looks like license
    files as defined in LICENSE_NAMES_REGEX.

    Raises:
      AssertionError: on runtime errors
      PackageLicenseError: couldn't find copyright attribution file.
    """
    license_override = self._GetOverrideLicense()
    if license_override:
      self.license_text_scanned = [license_override]
      return

    if not src_dir:
      ebuild_path = self._FindEbuildPath()
      self._RunEbuildPhases(ebuild_path, ['clean', 'fetch'])
      raw_output = self._RunEbuildPhases(ebuild_path, ['unpack'])
      output = raw_output.output.splitlines()
      # Output is spammy, it looks like this:
      #  * gc-7.2d.tar.gz RMD160 SHA1 SHA256 size ;-) ...                 [ ok ]
      #  * checking gc-7.2d.tar.gz ;-) ...                                [ ok ]
      #  * Running stacked hooks for pre_pkg_setup
      #  *    sysroot_build_bin_dir ...
      #  [ ok ]
      #  * Running stacked hooks for pre_src_unpack
      #  *    python_multilib_setup ...
      #  [ ok ]
      # >>> Unpacking source...
      # >>> Unpacking gc-7.2d.tar.gz to /build/x86-alex/tmp/po/[...]ps-7.2d/work
      # >>> Source unpacked in /build/x86-alex/tmp/portage/[...]ops-7.2d/work
      # So we only keep the last 2 lines, the others we don't care about.
      output = [line for line in output if line[0:3] == '>>>' and
                line != '>>> Unpacking source...']
      for line in output:
        logging.info(line)

      tmpdir = portage_util.PortageqEnvvar('PORTAGE_TMPDIR', board=self.board)
      # tmpdir gets something like /build/daisy/tmp/
      src_dir = os.path.join(tmpdir, 'portage', self.fullnamerev, 'work')

      if not os.path.exists(src_dir):
        raise AssertionError(
            "Unpack of %s didn't create %s. Version mismatch" %
            (self.fullnamerev, src_dir))

    # You may wonder how deep should we go?
    # In case of packages with sub-packages, it could be deep.
    # Let's just be safe and get everything we can find.
    # In the case of libatomic_ops, it's actually required to look deep
    # to find the MIT license:
    # dev-libs/libatomic_ops-7.2d/work/gc-7.2/libatomic_ops/doc/LICENSING.txt
    args = ['find', src_dir, '-type', 'f']
    result = cros_build_lib.run(args, stdout=True, encoding='utf-8')
    # Truncate results to look like this: swig-2.0.4/COPYRIGHT
    files = [x[len(src_dir):].lstrip('/') for x in result.stdout.splitlines()]
    license_files = []
    for name in files:
      # When we scan a source tree managed by git, this can contain license
      # files that are not part of the source. Exclude those.
      # (e.g. .git/refs/heads/licensing)
      if '.git/' in name:
        continue
      basename = os.path.basename(name)
      # Looking for license.* brings up things like license.gpl, and we
      # never want a GPL license when looking for copyright attribution,
      # so we skip them here. We also skip regexes that can return
      # license.py (seen in some code).
      if re.search(r'.*GPL.*', basename) or re.search(r'\.py$', basename):
        continue
      for regex in LICENSE_NAMES_REGEX:
        if re.search(regex, basename, re.IGNORECASE):
          license_files.append(name)
          break

    if not license_files:
      if need_copyright_attribution:
        logging.error("""
%s: unable to find usable license.
Typically this will happen because the ebuild says it's MIT or BSD, but there
was no license file that this script could find to include along with a
copyright attribution (required for BSD/MIT).

If this is Google source, please change
LICENSE="BSD"
to
LICENSE="BSD-Google"

If not, go investigate the unpacked source in %s,
and find which license to assign.  Once you found it, you should copy that
license to a file under %s
(or you can modify LICENSE_NAMES_REGEX to pickup a license file that isn't
being scraped currently).""",
                      self.fullnamerev, src_dir, COPYRIGHT_ATTRIBUTION_DIR)
        raise PackageLicenseError()
      else:
        # We can get called for a license like as-is where it's preferable
        # to find a better one in the source, but not fatal if we didn't.
        logging.info('Was not able to find a better license for %s '
                     'in %s to replace the more generic one from ebuild',
                     self.fullnamerev, src_dir)

    # Examples of multiple license matches:
    # dev-lang/swig-2.0.4-r1: swig-2.0.4/COPYRIGHT swig-2.0.4/LICENSE
    # dev-libs/glib-2.32.4-r1: glib-2.32.4/COPYING pkg-config-0.26/COPYING
    # dev-libs/libnl-3.2.14: libnl-doc-3.2.14/COPYING libnl-3.2.14/COPYING
    # dev-libs/libpcre-8.30-r2: pcre-8.30/LICENCE pcre-8.30/COPYING
    # dev-libs/libusb-0.1.12-r6: libusb-0.1.12/COPYING libusb-0.1.12/LICENSE
    # dev-libs/pyzy-0.1.0-r1: db/COPYING pyzy-0.1.0/COPYING
    # net-misc/strongswan-5.0.2-r4: strongswan-5.0.2/COPYING
    #                               strongswan-5.0.2/LICENSE
    # sys-process/procps-3.2.8_p11: debian/copyright procps-3.2.8/COPYING
    logging.info('License(s) for %s: %s', self.fullnamerev,
                 ' '.join(license_files))
    for license_file in sorted(license_files):
      # Joy and pink ponies. Some license_files are encoded as latin1 while
      # others are utf-8 and of course you can't know but only guess.
      license_path = os.path.join(src_dir, license_file)
      license_txt = ReadUnknownEncodedFile(license_path, 'Adding License')

      self.license_text_scanned += [
          'Scanned Source License %s:\n\n%s' % (license_file, license_txt)]

    # We used to clean up here, but there have been many instances where
    # looking at unpacked source to see where the licenses were, was useful
    # so let's disable this for now
    # self._RunEbuildPhases(['clean'])

  def LookForSkip(self):
    """Look for a reason to skip over this package.

    Sets self.skip to True if a reason was found.

    Returns:
      True if a reason was found.
    """
    if self.category in SKIPPED_CATEGORIES:
      logging.info('%s in SKIPPED_CATEGORIES, skip package', self.fullname)
      self.skip = True

    # TODO(dgarrett): There are additional reasons that should be handled here.

    return self.skip

  def _FindEbuildPath(self):
    """Discover the path to a package's associated ebuild.

    This method is not valid during the emerge hook process.

    Returns:
      full path file name of the ebuild file for this package.

    Raises:
      AssertionError if it can't be discovered for some reason.
    """
    equery_cmd = cros_build_lib.GetSysrootToolPath(
        cros_build_lib.GetSysroot(self.board), 'equery')
    args = [equery_cmd, '-q', '-C', 'which', self.fullnamerev]
    try:
      path = cros_build_lib.run(args, print_cmd=True, encoding='utf-8',
                                stdout=True).output.strip()
    except cros_build_lib.RunCommandError:
      path = None

    # Path can be false because of an exception, or a command result.
    if not path:
      raise AssertionError('_FindEbuildPath for %s failed.\n'
                           'Is your tree clean? Try a rebuild?' %
                           self.fullnamerev)

    logging.debug('%s -> %s', ' '.join(args), path)

    if not os.access(path, os.F_OK):
      raise AssertionError("Can't access %s" % (path,))

    return path

  def GetLicenses(self, build_info_dir, src_dir):
    """Populate the license related fields.

    Fields populated:
      license_names, license_text_scanned, homepages, skip

    Some packages have static license mappings applied to them that get
    retrieved from the ebuild.

    For others, we figure out whether the package source should be scanned to
    add licenses found there.

    Args:
      build_info_dir: Path to the build_info for the ebuild. This can be from
        the working directory during the emerge hook, or in the portage pkg db.
      src_dir: Directory to the expanded source code for this package. If None,
        the source will be expanded, if needed (slow).

    Raises:
      AssertionError: on runtime errors
      PackageLicenseError: couldn't find license in ebuild and source.
    """
    # If the total size installed is zero, we installed no content to license.
    if _BuildInfo(build_info_dir, 'SIZE').strip() == '0':
      # Allow for license generation for the whitelisted empty packages.
      if self.fullname not in SIZE_EXEMPT_PACKAGES:
        logging.debug('Build directory is empty')
        self.skip = True
        return

    self.homepages = _BuildInfo(build_info_dir, 'HOMEPAGE').split()
    ebuild_license_names = _BuildInfo(build_info_dir, 'LICENSE').split()

    # If this ebuild only uses skipped licenses, skip it.
    if (ebuild_license_names and
        all(l in SKIPPED_LICENSES for l in ebuild_license_names)):
      self.skip = True

    if self.skip:
      return

    if self.fullname in PACKAGE_HOMEPAGES:
      self.homepages = PACKAGE_HOMEPAGES[self.fullname]

    # Packages with missing licenses or licenses that need mapping (like
    # BSD/MIT) are hardcoded here:
    if self.fullname in PACKAGE_LICENSES:
      ebuild_license_names = PACKAGE_LICENSES[self.fullname]
      logging.info('Static license mapping for %s: %s', self.fullnamerev,
                   ','.join(ebuild_license_names))
    else:
      logging.info('Read licenses for %s: %s', self.fullnamerev,
                   ','.join(ebuild_license_names))

    # Lots of packages in chromeos-base have their license set to BSD instead
    # of BSD-Google:
    new_license_names = []
    for license_name in ebuild_license_names:
      # TODO: temp workaround for http;//crbug.com/348750 , remove when the bug
      # is fixed.
      if (license_name == 'BSD' and
          self.fullnamerev.startswith('chromeos-base/') and
          'BSD-Google' not in ebuild_license_names):
        license_name = 'BSD-Google'
        logging.warning(
            "Fixed BSD->BSD-Google for %s because it's in chromeos-base. "
            'Please fix the LICENSE field in the ebuild', self.fullnamerev)
      new_license_names.append(license_name)
    ebuild_license_names = new_license_names

    # The ebuild license field can look like:
    # LICENSE="GPL-3 LGPL-3 Apache-2.0" (this means AND, as in all 3)
    # for third_party/portage-stable/app-admin/rsyslog/rsyslog-5.8.11.ebuild
    # LICENSE="|| ( LGPL-2.1 MPL-1.1 )"
    # for third_party/portage-stable/x11-libs/cairo/cairo-1.8.8.ebuild

    # The parser isn't very smart and only has basic support for the
    # || ( X Y ) OR logic to do the following:
    # In order to save time needlessly unpacking packages and looking or a
    # cleartext license (which is really a crapshoot), if we have a license
    # like BSD that requires looking for copyright attribution, but we can
    # chose another license like GPL, we do that.

    if not self.skip and not ebuild_license_names:
      logging.error('%s: no license found in ebuild. FIXME!', self.fullnamerev)
      # In a bind, you could comment this out. I'm making the output fail to
      # get your attention since this error really should be fixed, but if you
      # comment out the next line, the script will try to find a license inside
      # the source.
      raise PackageLicenseError()

    # This is not invalid, but the parser can't deal with it, so if it ever
    # happens, error out to tell the programmer to do something.
    # dev-python/pycairo-1.10.0-r4: LGPL-3 || ( LGPL-2.1 MPL-1.1 )
    if '||' in ebuild_license_names[1:]:
      logging.error("%s: Can't parse || in the middle of a license: %s",
                    self.fullnamerev, ' '.join(ebuild_license_names))
      raise PackageLicenseError()

    or_licenses_and_one_is_no_attribution = False
    # We do a quick early pass first so that the longer pass below can
    # run accordingly.
    for license_name in [x for x in ebuild_license_names
                         if x not in LICENCES_IGNORE]:
      # Here we have an OR case, and one license that we can use stock, so
      # we remember that in order to be able to skip license attributions if
      # any were in the OR.
      if (ebuild_license_names[0] == '||' and
          license_name not in COPYRIGHT_ATTRIBUTION_LICENSES):
        or_licenses_and_one_is_no_attribution = True

    need_copyright_attribution = False
    scan_source_for_licenses = False

    for license_name in [x for x in ebuild_license_names
                         if x not in LICENCES_IGNORE]:
      # Licenses like BSD or MIT can't be used as is because they do not contain
      # copyright self. They have to be replaced by copyright file given in the
      # source code, or manually mapped by us in PACKAGE_LICENSES
      if license_name in COPYRIGHT_ATTRIBUTION_LICENSES:
        # To limit needless efforts, if a package is BSD or GPL, we ignore BSD
        # and use GPL to avoid scanning the package, but we can only do this if
        # or_licenses_and_one_is_no_attribution has been set above.
        # This ensures that if we have License: || (BSD3 BSD4), we will
        # look in the source.
        if or_licenses_and_one_is_no_attribution:
          logging.info('%s: ignore license %s because ebuild LICENSES had %s',
                       self.fullnamerev, license_name,
                       ' '.join(ebuild_license_names))
        else:
          logging.info("%s: can't use %s, will scan source code for copyright",
                       self.fullnamerev, license_name)
          need_copyright_attribution = True
          scan_source_for_licenses = True
      else:
        self.license_names.add(license_name)
        # We can't display just 2+ because it only contains text that says to
        # read v2 or v3.
        if license_name == 'GPL-2+':
          self.license_names.add('GPL-2')
        if license_name == 'LGPL-2+':
          self.license_names.add('LGPL-2')

      if license_name in LOOK_IN_SOURCE_LICENSES:
        logging.info('%s: Got %s, will try to find better license in source...',
                     self.fullnamerev, license_name)
        scan_source_for_licenses = True

    if self.license_names:
      logging.info('%s: using stock|cust license(s) %s',
                   self.fullnamerev, ','.join(self.license_names))

    # If the license(s) could not be found, or one requires copyright
    # attribution, dig in the source code for license files:
    # For instance:
    # Read licenses from ebuild for net-dialup/ppp-2.4.5-r3: BSD,GPL-2
    # We need get the substitution file for BSD and add it to GPL.
    if scan_source_for_licenses:
      self._ExtractLicenses(src_dir, need_copyright_attribution)

    # This shouldn't run, but leaving as sanity check.
    if not self.license_names and not self.license_text_scanned:
      raise AssertionError("Didn't find usable licenses for %s" %
                           self.fullnamerev)

  def SaveLicenseDump(self, save_file):
    """Save PackageInfo contents to a YAML file.

    This is used to cache license results between the emerge hook phase and
    credits page generation.

    Args:
      save_file: File to save the yaml contents into.
    """
    logging.debug('Saving license to %s', save_file)
    yaml_dump = list(self.__dict__.items())
    osutils.WriteFile(save_file, yaml.dump(yaml_dump), makedirs=True)


def _GetLicenseDirectories(board=None, dir_set=_BOTH_DIRS,
                           buildroot=constants.SOURCE_ROOT):
  """Get the "licenses" directories for all matching overlays.

  Args:
    board: str|None - Which board to use to search a hierarchy.
    dir_set: str - Whether to fetch stock, custom, or both sets of directories.
        See the _(STOCK|CUSTOM|BOTH)_DIRS constants
    buildroot: str - (typically) the root chromiumos path

  Returns:
    list - all matching "licenses" directories
  """
  stock = [os.path.join(buildroot, d) for d in STOCK_LICENSE_DIRS]
  if board is None:
    custom = [os.path.join(buildroot, d) for d in CUSTOM_LICENSE_DIRS]
  else:
    tmp = portage_util.FindOverlays(constants.BOTH_OVERLAYS, board, buildroot)
    custom = [os.path.join(d, 'licenses') for d in tmp]

  if dir_set == _STOCK_DIRS:
    return stock
  elif dir_set == _CUSTOM_DIRS:
    return custom
  else:
    return stock + custom


class Licensing(object):
  """Do the actual work of extracting licensing info and outputting html."""

  def __init__(self, board, package_fullnames, gen_licenses):
    # eg x86-alex
    self.board = board
    # List of stock and custom licenses referenced in ebuilds. Used to
    # print a report. Dict value says which packages use that license.
    self.licenses = {}

    # Licenses are supposed to be generated at package build time and be
    # ready for us, but in case they're not, they can be generated.
    self.gen_licenses = gen_licenses

    self.package_text = {}
    self.entry_template = None

    # We need to have a dict for the list of packages objects, index by package
    # fullnamerev, so that when we scan our licenses at the end, and find out
    # some shared licenses are only used by one package, we can access that
    # package object by name, and add the license directly in that object.
    self.packages = {}
    self._package_fullnames = package_fullnames

  @property
  def sorted_licenses(self):
    return sorted(self.licenses, key=lambda x: x.lower())

  def _LoadLicenseDump(self, pkg):
    save_file = pkg.license_dump_path
    logging.debug('Getting license from %s for %s', save_file, pkg.name)
    yaml_dump = yaml.load(osutils.ReadFile(save_file))
    for key, value in yaml_dump:
      pkg.__dict__[key] = value

  def LicensedPackages(self, license_name):
    """Return list of packages using a given license."""
    return self.licenses[license_name]

  def LoadPackageInfo(self):
    """Populate basic package info for all packages from their ebuild."""
    for package_name in self._package_fullnames:
      pkg = PackageInfo(self.board, package_name)
      self.packages[package_name] = pkg

  def ProcessPackageLicenses(self):
    """Iterate through all packages provided and gather their licenses.

    GetLicenses will scrape licenses from the code and/or gather stock license
    names. We gather the list of stock and custom ones for later processing.

    Do not call this after adding virtual packages with AddExtraPkg.
    """
    for package_name in self.packages:
      pkg = self.packages[package_name]

      if pkg.skip:
        logging.debug('Package %s is in skip list', package_name)
        continue

      # Other skipped packages get dumped with incomplete info and the skip flag
      if not os.path.exists(pkg.license_dump_path):
        if not self.gen_licenses:
          raise PackageLicenseError('License for %s is missing' % package_name)

        logging.error('>>> License for %s is missing, creating now <<<',
                      package_name)
        build_info_path = os.path.join(
            cros_build_lib.GetSysroot(pkg.board),
            PER_PKG_LICENSE_DIR, pkg.fullnamerev)
        pkg.GetLicenses(build_info_path, None)

        # We dump packages where licensing failed too.
        pkg.SaveLicenseDump(pkg.license_dump_path)

      # Load the pre-cached version, if the in-memory version is incomplete.
      if not pkg.license_names:
        logging.debug('loading dump for %s', pkg.fullnamerev)
        self._LoadLicenseDump(pkg)

  def AddExtraPkg(self, fullnamerev, homepages, license_names, license_texts):
    """Allow adding pre-created virtual packages.

    GetLicenses will not work on them, so add them after having run
    ProcessPackages.

    Args:
      fullnamerev: package name of the form x11-base/X.Org-1.9.3-r23
      homepages: list of url strings.
      license_names: list of license name strings.
      license_texts: custom license text to use, mostly for attribution.
    """
    # TODO(crbug.com/401332): Remove this entirely.
    # We allow a few packages for now so new packages will stop showing up.
    if 'Proprietary-Binary' in license_names:
      # Note: DO NOT ADD ANY MORE PACKAGES HERE.
      LEGACY_PKGS = (
          'chromeos-base/infineon-firmware',
          'chromeos-base/pepper-flash',
          'sys-boot/coreboot-private-files-',
          'sys-boot/exynos-pre-boot',
          'sys-boot/nhlt-blobs',
          'sys-boot/mma-blobs',
      )
      if not any(fullnamerev.startswith(x) for x in LEGACY_PKGS):
        raise AssertionError('Proprietary-Binary is not a valid license.')

    # TODO(crbug.com/1019728): Remove this entirely.
    # We allow a few packages for now so new packages will stop showing up.
    if 'Google-TOS' in license_names:
      # Note: DO NOT ADD ANY MORE PACKAGES HERE.
      LEGACY_PKGS = {
          'chromeos-base/android-flashstation-app',
          'chromeos-base/app-shell-apps-rialto',
          'chromeos-base/chromeos-board-default-apps-atlas',
          'chromeos-base/chromeos-board-default-apps-blaze',
          'chromeos-base/chromeos-board-default-apps-butterfly',
          'chromeos-base/chromeos-board-default-apps-candy',
          'chromeos-base/chromeos-board-default-apps-cave',
          'chromeos-base/chromeos-board-default-apps-chell',
          'chromeos-base/chromeos-board-default-apps-falco',
          'chromeos-base/chromeos-board-default-apps-kip',
          'chromeos-base/chromeos-board-default-apps-link',
          'chromeos-base/chromeos-board-default-apps-mickey',
          'chromeos-base/chromeos-board-default-apps-minnie',
          'chromeos-base/chromeos-board-default-apps-nocturne',
          'chromeos-base/chromeos-board-default-apps-paine',
          'chromeos-base/chromeos-board-default-apps-pi',
          'chromeos-base/chromeos-board-default-apps-pit',
          'chromeos-base/chromeos-board-default-apps-quawks',
          'chromeos-base/chromeos-board-default-apps-setzer',
          'chromeos-base/chromeos-board-default-apps-skate',
          'chromeos-base/chromeos-board-default-apps-snappy',
          'chromeos-base/chromeos-board-default-apps-soraka',
          'chromeos-base/chromeos-board-default-apps-speedy',
          'chromeos-base/chromeos-board-default-apps-squawks',
          'chromeos-base/chromeos-board-default-apps-terra',
          'chromeos-base/chromeos-board-default-apps-winky',
          'chromeos-base/chromeos-board-default-apps-yuna',
          'chromeos-base/chromeos-board-default-apps-zako',
          'chromeos-base/chromeos-chrome',
          'chromeos-base/chromeos-default-apps',
          'chromeos-base/chromeos-disk-firmware-enguarde',
          'chromeos-base/chromeos-disk-firmware-falco',
          'chromeos-base/chromeos-disk-firmware-gnawty',
          'chromeos-base/chromeos-disk-firmware-samus',
          'chromeos-base/chromeos-disk-firmware-squawks',
          'chromeos-base/chromeos-disk-firmware-test-enguarde',
          'chromeos-base/chromeos-disk-firmware-test-gnawty',
          'chromeos-base/chromeos-disk-firmware-test-samus',
          'chromeos-base/chromeos-disk-firmware-test-squawks',
          'chromeos-base/chromeos-firmware-anx7688',
          'chromeos-base/fibocom-firmware',
          'chromeos-base/google-sans-fonts',
          'chromeos-base/houdini',
          'chromeos-base/houdini-pi',
          'chromeos-base/houdini-qt',
          'chromeos-base/intel-hdcp',
          'chromeos-base/monotype-fonts',
          'chromeos-base/rialto-cellular-autoconnect',
          'chromeos-base/rialto-modem-watchdog',
          'chromeos-base/rialto-override-apn',
          'chromeos-base/rialto-override-scanresponse',
          'chromeos-base/rialto-services',
          'chromeos-base/widevine-cdm',
          'dev-embedded/meta-embedded-toolkit',
          'dev-util/PVRPerfServer',
          'dev-util/PVRTrace',
          'media-libs/a630-fw',
          'media-libs/adreno-drivers',
          'media-libs/apl-hotword-support',
          'media-libs/arc-img-ddk',
          'media-libs/arc-mali-drivers',
          'media-libs/arc-mali-drivers-bifrost',
          'media-libs/dlm',
          'media-libs/glk-hotword-support',
          'media-libs/go2001-fw',
          'media-libs/img-ddk',
          'media-libs/img-ddk-bin',
          'media-libs/kbl-hotword-support',
          'media-libs/kbl-rt5514-hotword-support',
          'media-libs/mali-drivers',
          'media-libs/mali-drivers-bifrost',
          'media-libs/mali-drivers-bifrost-bin',
          'media-libs/mali-drivers-bin',
          'media-libs/mfc-fw',
          'media-libs/mfc-fw-v7',
          'media-libs/mfc-fw-v8',
          'media-libs/rk3399-hotword-support',
          'media-libs/skl-hotword-support',
          'sys-apps/accelerator-bootstrap',
          'sys-apps/eid',
          'sys-apps/loonix-hydrogen',
          'sys-boot/chromeos-firmware-ps8751',
          'sys-boot/chromeos-vendor-strings-wilco',
          'sys-boot/coreboot-private-files-amenia',
          'sys-boot/coreboot-private-files-aplrvp',
          'sys-boot/coreboot-private-files-atlas',
          'sys-boot/coreboot-private-files-baseboard-auron',
          'sys-boot/coreboot-private-files-baseboard-coral',
          'sys-boot/coreboot-private-files-baseboard-fizz',
          'sys-boot/coreboot-private-files-baseboard-glados',
          'sys-boot/coreboot-private-files-baseboard-jecht',
          'sys-boot/coreboot-private-files-baseboard-kalista',
          'sys-boot/coreboot-private-files-baseboard-kblrvp',
          'sys-boot/coreboot-private-files-baseboard-kunimitsu',
          'sys-boot/coreboot-private-files-baseboard-nami',
          'sys-boot/coreboot-private-files-baseboard-octopus',
          'sys-boot/coreboot-private-files-baseboard-poppy',
          'sys-boot/coreboot-private-files-baseboard-rammus',
          'sys-boot/coreboot-private-files-baseboard-reef',
          'sys-boot/coreboot-private-files-baseboard-strago',
          'sys-boot/coreboot-private-files-bolt',
          'sys-boot/coreboot-private-files-chipset-picasso',
          'sys-boot/coreboot-private-files-chipset-stnyridge',
          'sys-boot/coreboot-private-files-cnlrvp',
          'sys-boot/coreboot-private-files-drallion',
          'sys-boot/coreboot-private-files-eve',
          'sys-boot/coreboot-private-files-falco',
          'sys-boot/coreboot-private-files-fizz',
          'sys-boot/coreboot-private-files-glkrvp',
          'sys-boot/coreboot-private-files-grunt',
          'sys-boot/coreboot-private-files-hatch',
          'sys-boot/coreboot-private-files-kalista',
          'sys-boot/coreboot-private-files-mistral',
          'sys-boot/coreboot-private-files-nami',
          'sys-boot/coreboot-private-files-nautilus',
          'sys-boot/coreboot-private-files-nocturne',
          'sys-boot/coreboot-private-files-panther',
          'sys-boot/coreboot-private-files-peppy',
          'sys-boot/coreboot-private-files-poppy',
          'sys-boot/coreboot-private-files-pyro',
          'sys-boot/coreboot-private-files-rambi',
          'sys-boot/coreboot-private-files-rammus',
          'sys-boot/coreboot-private-files-reef',
          'sys-boot/coreboot-private-files-samus',
          'sys-boot/coreboot-private-files-sand',
          'sys-boot/coreboot-private-files-sarien',
          'sys-boot/coreboot-private-files-sklrvp',
          'sys-boot/coreboot-private-files-snappy',
          'sys-boot/coreboot-private-files-soraka',
          'sys-boot/exynos-pre-boot',
          'sys-boot/intel-cflfsp',
          'sys-boot/intel-glkfsp',
          'sys-boot/intel-iclfsp',
          'sys-boot/mma-blobs',
          'sys-boot/nhlt-blobs',
          'sys-boot/rk3399-hdcp-fw',
          'sys-firmware/displaylink-firmware',
          'sys-firmware/huddly-firmware',
          'sys-firmware/iq-firmware',
          'sys-firmware/sis-firmware',
          'www-servers/spacecast',
      }
      # TODO(b/148306737), only allow this package prior to R84. To be removed
      # once the proper license is in place.
      version = manifest_version.VersionInfo.from_repo(constants.SOURCE_ROOT)
      if int(version.chrome_branch) <= 83:
        LEGACY_PKGS.add('sys-firmware/parade-ps8815a0-firmware')
      if not any(fullnamerev.startswith(x) for x in LEGACY_PKGS):
        raise AssertionError('Google-TOS is not a valid license.')

    pkg = PackageInfo(self.board, fullnamerev)
    pkg.homepages = homepages
    pkg.license_names = license_names
    pkg.license_text_scanned = license_texts
    self.packages[fullnamerev] = pkg

  # Called directly by src/repohooks/pre-upload.py
  @staticmethod
  def FindLicenseType(license_name, board=None, overlay_path=None,
                      buildroot=constants.SOURCE_ROOT):
    """Says if a license is stock Gentoo, custom, or doesn't exist.

    Will check the old, static locations by default, but supplying either the
    overlay directory or board name allows searching in the overlay hierarchy.
    Ignores the overlay_path if board is provided.

    Args:
      license_name: The license name
      board: Which board to use as the search base
      overlay_path: Which overlay directory to use as the search base
      buildroot: source root

    Returns:
      str - license type

    Raises:
      AssertionError when the license couldn't be found
    """
    # Check the stock licenses first since those may appear in the generated
    # list of overlay directories for a board
    stock = _GetLicenseDirectories(dir_set=_STOCK_DIRS, buildroot=buildroot)
    for directory in stock:
      path = os.path.join(directory, license_name)
      if os.path.exists(path):
        return 'Gentoo Package Stock'

    # Not stock, find and check relevant custom directories
    if board is None and overlay_path is not None:
      board = portage_util.GetOverlayName(overlay_path)

    # Check the custom licenses
    custom = _GetLicenseDirectories(board, _CUSTOM_DIRS, buildroot)
    for directory in custom:
      path = os.path.join(directory, license_name)
      if os.path.exists(path):
        return 'Custom'

    if license_name in SKIPPED_LICENSES:
      return 'Custom'

    raise AssertionError("""
license %s could not be found in %s
If the license in the ebuild is correct,
a) a stock license should be added to portage-stable/licenses :
running `cros_portage_upgrade` inside of the chroot should clone this repo
to /tmp/portage/:
https://chromium.googlesource.com/chromiumos/overlays/portage/+/gentoo
find the new licenses under licenses, and add them to portage-stable/licenses

b) if it's a non gentoo package with a custom license, you can copy that license
to third_party/chromiumos-overlay/licenses/

Try re-running the script with -p cat/package-ver --generate
after fixing the license.""" % (license_name, '\n'.join(set(stock + custom))))

  @staticmethod
  def ReadSharedLicense(license_name, board=None,
                        buildroot=constants.SOURCE_ROOT):
    """Read and return stock or cust license file specified in an ebuild."""
    directories = _GetLicenseDirectories(board=board, dir_set=_BOTH_DIRS,
                                         buildroot=buildroot)
    license_path = None
    for directory in directories:
      path = os.path.join(directory, license_name)
      if os.path.exists(path):
        license_path = path
        break

    if license_path:
      return ReadUnknownEncodedFile(license_path, 'read license')
    else:
      raise AssertionError('license %s could not be found in %s'
                           % (license_name, '\n'.join(directories)))

  @staticmethod
  def EvaluateTemplate(template, env):
    """Expand a template with vars like {{foo}} using a dict of expansions."""
    # TODO switch to stock python templates.
    for key, val in env.items():
      template = template.replace('{{%s}}' % key, val)
    return template

  def _GeneratePackageLicenseText(self, pkg):
    """Concatenate all licenses related to a pkg.

    This means a combination of ebuild shared licenses and licenses read from
    the pkg source tree, if any.

    Args:
      pkg: PackageInfo object

    Raises:
      AssertionError: on runtime errors
    """
    license_text = []
    for license_text_scanned in pkg.license_text_scanned:
      license_text.append(license_text_scanned)
      license_text.append('%s\n' % ('-=' * 40))

    license_pointers = []
    # sln: shared license name.
    for sln in pkg.license_names:
      # Says whether it's a stock gentoo or custom license.
      license_type = self.FindLicenseType(sln, board=self.board)
      license_pointers.append(
          "<li><a href='#%s'>%s License %s</a></li>" % (
              sln, license_type, sln))

    # This should get caught earlier, but one extra check.
    if not license_text + license_pointers:
      raise AssertionError('Ended up with no license_text for %s' %
                           pkg.fullnamerev)

    env = {
        'name': pkg.name,
        'namerev': '%s-%s' % (pkg.name, pkg.version),
        'url': cgi.escape(pkg.homepages[0]) if pkg.homepages else '',
        'licenses_txt': cgi.escape('\n'.join(license_text)) or '',
        'licenses_ptr': '\n'.join(license_pointers) or '',
    }
    self.package_text[pkg] = self.EvaluateTemplate(self.entry_template, env)

  def GenerateHTMLLicenseOutput(self, output_file,
                                output_template=TMPL,
                                entry_template=ENTRY_TMPL,
                                license_template=SHARED_LICENSE_TMPL):
    """Generate the combined html license file used in ChromeOS.

    Args:
      output_file: resulting HTML license output.
      output_template: template for the entire HTML file.
      entry_template: template for per package entries.
      license_template: template for shared license entries.
    """
    self.entry_template = ReadUnknownEncodedFile(entry_template)
    sorted_license_txt = []

    # Keep track of which licenses are used by which packages.
    for pkg in self.packages.values():
      if pkg.skip:
        continue
      for sln in pkg.license_names:
        self.licenses.setdefault(sln, []).append(pkg.fullnamerev)

    # Find licenses only used once, and roll them in the package that uses them.
    # We use list() because licenses is modified in the loop, so we can't use
    # an iterator.
    for sln in list(self.licenses):
      if len(self.licenses[sln]) == 1:
        pkg_fullnamerev = self.licenses[sln][0]
        logging.info('Collapsing shared license %s into single use license '
                     '(only used by %s)', sln, pkg_fullnamerev)
        license_type = self.FindLicenseType(sln, board=self.board)
        license_txt = self.ReadSharedLicense(sln, board=self.board)
        single_license = '%s License %s:\n\n%s' % (license_type, sln,
                                                   license_txt)
        pkg = self.packages[pkg_fullnamerev]
        pkg.license_text_scanned.append(single_license)
        pkg.license_names.remove(sln)
        del self.licenses[sln]

    for pkg in sorted(self.packages.values(),
                      key=lambda x: (x.name.lower(), x.version, x.revision)):
      if pkg.skip:
        logging.debug('Skipping package %s', pkg.fullnamerev)
        continue
      self._GeneratePackageLicenseText(pkg)
      sorted_license_txt += [self.package_text[pkg]]

    # Now generate the bottom of the page that will contain all the shared
    # licenses and a list of who is pointing to them.
    license_template = ReadUnknownEncodedFile(license_template)

    licenses_txt = []
    for license_name in self.sorted_licenses:
      env = {
          'license_name': license_name,
          'license': cgi.escape(self.ReadSharedLicense(license_name,
                                                       board=self.board)),
          'license_type': self.FindLicenseType(license_name, board=self.board),
          'license_packages': ' '.join(self.LicensedPackages(license_name)),
      }
      licenses_txt += [self.EvaluateTemplate(license_template, env)]

    file_template = ReadUnknownEncodedFile(output_template)
    env = {
        'entries': '\n'.join(sorted_license_txt),
        'licenses': '\n'.join(licenses_txt),
    }
    osutils.WriteFile(output_file,
                      self.EvaluateTemplate(file_template, env).encode('utf-8'),
                      mode='wb')


def ListInstalledPackages(board, all_packages=False):
  """Return a list of all packages installed for a particular board."""

  # If all_packages is set to True, all packages visible in the build
  # chroot are used to generate the licensing file. This is not what you want
  # for a release license file, but it's a way to run licensing checks against
  # all packages.
  # If it's set to False, it will only generate a licensing file that contains
  # packages used for a release build (as determined by the dependencies for
  # virtual/target-os).

  if all_packages:
    # The following returns all packages that were part of the build tree
    # (many get built or used during the build, but do not get shipped).
    # Note that it also contains packages that are in the build as
    # defined by build_packages but not part of the image we ship.
    equery_cmd = cros_build_lib.GetSysrootToolPath(
        cros_build_lib.GetSysroot(board), 'equery')
    args = [equery_cmd, 'list', '*']
    packages = cros_build_lib.run(args, encoding='utf-8',
                                  stdout=True).output.splitlines()
  else:
    # The following returns all packages that were part of the build tree
    # (many get built or used during the build, but do not get shipped).
    # Note that it also contains packages that are in the build as
    # defined by build_packages but not part of the image we ship.
    emerge_cmd = cros_build_lib.GetSysrootToolPath(
        cros_build_lib.GetSysroot(board), 'emerge')
    args = [emerge_cmd, '--with-bdeps=y', '--usepkgonly',
            '--emptytree', '--pretend', '--color=n', 'virtual/target-os']
    emerge = cros_build_lib.run(args, encoding='utf-8',
                                stdout=True).output.splitlines()
    # Another option which we've decided not to use, is bdeps=n.  This outputs
    # just the packages we ship, but does not packages that were used to build
    # them, including a package like flex which generates a .a that is included
    # and shipped in ChromeOS.
    # We've decided to credit build packages, even if we're not legally required
    # to (it's always nice to do), and that way we get corner case packages like
    # flex. This is why we use bdep=y and not bdep=n.

    packages = []
    # [binary   R    ] x11-libs/libva-1.1.1 to /build/x86-alex/
    pkg_rgx = re.compile(r'\[[^]]+R[^]]+\] (.+) to /build/.*')
    # If we match something else without the 'R' like
    # [binary     U  ] chromeos-base/pepper-flash-13.0.0.133-r1 [12.0.0.77-r1]
    # this is bad and we should die on this.
    pkg_rgx2 = re.compile(r'(\[[^]]+\] .+) to /build/.*')
    for line in emerge:
      match = pkg_rgx.search(line)
      match2 = pkg_rgx2.search(line)
      if match:
        packages.append(match.group(1))
      elif match2:
        raise AssertionError('Package incorrectly installed, try eclean-%s' %
                             board, '\n%s' % match2.group(1))

  return packages


def ReadUnknownEncodedFile(file_path, logging_text=None):
  """Read a file of unknown encoding (UTF-8 or latin) by trying in sequence.

  Args:
    file_path: what to read.
    logging_text: what to display for logging depending on file read.

  Returns:
    File content, possibly converted from latin1 to UTF-8.

  Raises:
    Assertion error: if non-whitelisted illegal XML characters
      are found in the file.
    ValueError: returned if we get invalid XML.
  """
  try:
    with codecs.open(file_path, encoding='utf-8') as c:
      file_txt = c.read()
      if logging_text:
        logging.info('%s %s (UTF-8)', logging_text, file_path)
  except UnicodeDecodeError:
    with codecs.open(file_path, encoding='latin1') as c:
      file_txt = c.read()
      if logging_text:
        logging.info('%s %s (latin1)', logging_text, file_path)

  # Remove characters that are not XML 1.0 legal.
  # XML 1.0 acceptable character range:
  # Char ::= #x9 | #xA | #xD | [#x20-#xD7FF] | [#xE000-#xFFFD] | \
  #          [#x10000-#x10FFFF]

  # Strip out common/OK values silently.
  silent_chars_re = re.compile(u'[\x0c]')
  file_txt = silent_chars_re.sub('', file_txt)

  illegal_chars_re = re.compile(
      u'[\x00-\x08\x0b\x0c\x0e-\x1F\uD800-\uDFFF\uFFFE\uFFFF]')

  if illegal_chars_re.findall(file_txt):
    logging.warning('Found illegal XML characters, stripping them out.')
    file_txt = illegal_chars_re.sub('', file_txt)

  return file_txt


def _BuildInfo(build_info_path, filename):
  """Fetch contents of a file from portage build_info directory.

  Portage maintains a build_info directory that exists both during the process
  of emerging an ebuild, and (in a different location) after the ebuild has been
  emerged.

  Various useful data files exist there like:
   'CATEGORY', 'PF', 'SIZE', 'HOMEPAGE', 'LICENSE'

  Args:
    build_info_path: Path to the build_info directory to read from.
    filename: Name of the file to read.

  Returns:
    Contents of the file as a string, or "".
  """
  filename = os.path.join(build_info_path, filename)

  # Buildinfo properties we read are in US-ASCII, not Unicode.
  try:
    bi = osutils.ReadFile(filename).rstrip()
  # Some properties like HOMEPAGE may be absent.
  except IOError:
    bi = ''
  return bi


def HookPackageProcess(pkg_build_path):
  """Different entry point to populate a packageinfo.

  This is called instead of LoadPackageInfo when called by a package build.

  Args:
    pkg_build_path: unpacked being built by emerge.
  """
  build_info_dir = os.path.join(pkg_build_path, 'build-info')

  fullnamerev = '%s/%s' % (_BuildInfo(build_info_dir, 'CATEGORY'),
                           _BuildInfo(build_info_dir, 'PF'))
  logging.debug('Computed package name %s from %s',
                fullnamerev, pkg_build_path)

  pkg = PackageInfo(None, fullnamerev)

  src_dir = os.path.join(pkg_build_path, 'work')
  pkg.GetLicenses(build_info_dir, src_dir)

  pkg.SaveLicenseDump(os.path.join(build_info_dir, 'license.yaml'))
