#!/usr/bin/python
#
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Original Author: Brian Harring <ferringb@chromium.org>
# Maintainer: Marc MERLIN <merlin@chromium.org>

"""Cheesy script that attempts to generate an HTML file containing license
information and homepage links for all installed packages.

This script is NOT ready for production, not style compliant, and is only
checked in as a placeholder until new logic from licenses.py can be moved
into it.
"""

import cgi
import os
import subprocess
import sys
import shutil

PKG_DATA_TARGET_DIR = '/usr/share/licenses'

STOCK_LICENSE_DIRS = [
  os.path.expanduser('~/trunk/src/third_party/portage/licenses'),
  'licenses',
]

SKIPPED_CATEGORIES = [
  'chromeos-base',
  'virtual',
]

SKIPPED_PACKAGES = [
  # These are Chrome-OS-specific packages.
  'media-libs/libresample',
  'sys-apps/rootdev',
  'sys-kernel/chromeos-kernel',  # already manually credit Linux

  'www-plugins/adobe-flash',

  # These have been split across several packages, so we skip listing the
  # individual components (and just list the main package instead).
  'app-editors/vim-core',
  'x11-apps/mesa-progs',

  # Portage metapackage.
  'x11-base/xorg-drivers',

  # These are covered by app-i18n/ibus-mozc (BSD, copyright Google).
  'app-i18n/ibus-mozc-chewing',
  'app-i18n/ibus-mozc-hangul',

  # These are all X.org sub-packages; shouldn't be any need to list them
  # individually.
  'media-fonts/encodings',
  'x11-apps/iceauth',
  'x11-apps/intel-gpu-tools',
  'x11-apps/mkfontdir',
  'x11-apps/rgb',
  'x11-apps/setxkbmap',
  'x11-apps/xauth',
  'x11-apps/xcursorgen',
  'x11-apps/xdpyinfo',
  'x11-apps/xdriinfo',
  'x11-apps/xev',
  'x11-apps/xgamma',
  'x11-apps/xhost',
  'x11-apps/xinit',
  'x11-apps/xinput',
  'x11-apps/xkbcomp',
  'x11-apps/xlsatoms',
  'x11-apps/xlsclients',
  'x11-apps/xmodmap',
  'x11-apps/xprop',
  'x11-apps/xrandr',
  'x11-apps/xrdb',
  'x11-apps/xset',
  'x11-apps/xwininfo',
  'x11-base/xorg-server',
  'x11-drivers/xf86-input-evdev',
  'x11-drivers/xf86-input-keyboard',
  'x11-drivers/xf86-input-mouse',
  'x11-drivers/xf86-input-synaptics',
  'x11-drivers/xf86-video-intel',
  'x11-drivers/xf86-video-vesa',
  'x11-drivers/xf86-video-vmware',
  'x11-libs/libICE',
  'x11-libs/libSM',
  'x11-libs/libX11',
  'x11-libs/libXScrnSaver',
  'x11-libs/libXau',
  'x11-libs/libXcomposite',
  'x11-libs/libXcursor',
  'x11-libs/libXdamage',
  'x11-libs/libXdmcp',
  'x11-libs/libXext',
  'x11-libs/libXfixes',
  'x11-libs/libXfont',
  'x11-libs/libXfontcache',
  'x11-libs/libXft',
  'x11-libs/libXi',
  'x11-libs/libXinerama',
  'x11-libs/libXmu',
  'x11-libs/libXp',
  'x11-libs/libXrandr',
  'x11-libs/libXrender',
  'x11-libs/libXres',
  'x11-libs/libXt',
  'x11-libs/libXtst',
  'x11-libs/libXv',
  'x11-libs/libXvMC',
  'x11-libs/libXxf86vm',
  'x11-libs/libdrm',
  'x11-libs/libfontenc',
  'x11-libs/libpciaccess',
  'x11-libs/libxkbfile',
  'x11-libs/libxkbui',
  'x11-libs/pixman',
  'x11-libs/xtrans',
  'x11-misc/util-macros',
  'x11-misc/xbitmaps',
  'x11-proto/bigreqsproto',
  'x11-proto/compositeproto',
  'x11-proto/damageproto',
  'x11-proto/dri2proto',
  'x11-proto/fixesproto',
  'x11-proto/fontcacheproto',
  'x11-proto/fontsproto',
  'x11-proto/inputproto',
  'x11-proto/kbproto',
  'x11-proto/printproto',
  'x11-proto/randrproto',
  'x11-proto/recordproto',
  'x11-proto/renderproto',
  'x11-proto/resourceproto',
  'x11-proto/scrnsaverproto',
  'x11-proto/trapproto',
  'x11-proto/videoproto',
  'x11-proto/xcmiscproto',
  'x11-proto/xextproto',
  'x11-proto/xf86bigfontproto',
  'x11-proto/xf86dgaproto',
  'x11-proto/xf86driproto',
  'x11-proto/xf86rushproto',
  'x11-proto/xf86vidmodeproto',
  'x11-proto/xineramaproto',
  'x11-proto/xproto',
]

LICENSE_FILENAMES = [
  'COPYING',
  'COPYRIGHT',  # used by strace
  'LICENCE',    # used by openssh
  'LICENSE',
  'LICENSE.txt',  # used by NumPy, glew
  'LICENSE.TXT',  # used by hdparm
  'License',    # used by libcap
  'IPA_Font_License_Agreement_v1.0.txt',  # used by ja-ipafonts
]
LICENSE_FILENAMES = frozenset(x.lower() for x in LICENSE_FILENAMES)

SKIPPED_LICENSE_DIRECTORIES = frozenset([
  'third_party'
])

PACKAGE_LICENSES = {
  'app-crypt/nss': ['MPL-1.1'],
  'app-editors/gentoo-editor': ['MIT-gentoo-editor'],
  'app-editors/vim': ['vim'],
  'app-i18n/ibus-mozc': ['BSD-Google'],
  'dev-db/sqlite': ['sqlite'],
  'dev-libs/libevent': ['BSD-libevent'],
  'dev-libs/nspr': ['GPL-2'],
  'dev-libs/nss': ['GPL-2'],
  'dev-libs/protobuf': ['BSD-Google'],
  'dev-util/bsdiff': ['BSD-bsdiff'],
  'media-fonts/font-util': ['font-util'],  # COPYING file from git repo
  'media-libs/freeimage': ['GPL-2'],
  'media-libs/jpeg': ['jpeg'],
  'media-plugins/o3d': ['BSD-Google'],
  'net-dialup/ppp': ['ppp-2.4.4'],
  'net-dns/c-ares': ['MIT-MIT'],
  'net-misc/dhcpcd': ['BSD-dhcpcd'],
  'net-misc/iputils': ['BSD-iputils'],
  'net-wireless/wpa_supplicant': ['GPL-2'],
  'net-wireless/iwl1000-ucode': ['Intel-iwl1000'],
  'net-wireless/marvell_sd8787': ['Marvell'],
  'sys-apps/less': ['BSD-less'],
  'sys-libs/ncurses': ['ncurses'],
  'sys-libs/talloc': ['LGPL-3'],  # ebuild incorrectly says GPL-3
  'sys-libs/timezone-data': ['public-domain'],
  'sys-process/vixie-cron': ['vixie-cron'],
  'x11-drivers/xf86-input-cmt': ['BSD-Google'],
}

INVALID_STOCK_LICENSES = frozenset([
  'BSD',   # requires distribution of copyright notice
  'BSD-2', # same
  'BSD-4', # same
  'MIT',   # same
])

TEMPLATE_FILE = 'about_credits.tmpl'
ENTRY_TEMPLATE_FILE = os.path.dirname(os.path.abspath(__file__))
ENTRY_TEMPLATE_FILE = os.path.join(ENTRY_TEMPLATE_FILE,
                                      'about_credits_entry.tmpl')

def FilterUsableLicenses(raw_license_names):
  for license_name in raw_license_names:
    if license_name in INVALID_STOCK_LICENSES:
      print 'skipping invalid stock license %s' % license_name
      continue
    yield license_name

def _FindLicenseInSource(workdir):
  names = []
  for name in LICENSE_FILENAMES:
    if names:
      names.append('-o')
    names.extend(('-iname', name))
  if names:
    names = ['('] + names + [')']

  # ensure the directory has a trailing slash, else if it's a symlink
  # find will just stop at the sym.
  args = ['find', workdir +'/', '-maxdepth', '3', '-mindepth', '1',
    '-type', 'f']
  args += names
  names = []
  for dirname in SKIPPED_LICENSE_DIRECTORIES:
    if names:
      names.append('-o')
    names.extend(('-ipath', '*/%s/*' % dirname))
  if names:
    args += ['-a', '!', '('] + names + [')']
  p = subprocess.Popen(args, stdin=None, stderr=None,
    stdout=subprocess.PIPE, shell=False)
  stdout, _ = p.communicate()
  files = filter(None, stdout.split("\n"))
  if files:
    return open(files[0], 'r').read()
  return None

def _GetStockLicense(licenses, stock_licenses):
  if not licenses:
    return None

  license_texts = []
  for license_name in licenses:
    license_path = stock_licenses.get(license_name)
    if license_path is None:
      # TODO: We should probably report failure if we're unable to
      # find one of the licenses from a dual-licensed package.
      continue
    license_texts.append(open(license_path).read())

  return '\n'.join(license_texts)

def identifyLicenseText(workdir, metadata_licenses, stock_licenses):
  license_text = _FindLicenseInSource(workdir)
  if not license_text:
    license_text = _GetStockLicense(metadata_licenses, stock_licenses)
  if not license_text:
    raise Exception("failed finding a license in both the source, and a "
      "usable stock license")
  return license_text

def EvaluateTemplate(template, env, escape=True):
  """Expand a template with variables like {{foo}} using a
  dictionary of expansions."""
  for key, val in env.items():
    if escape:
      val = cgi.escape(val)
    template = template.replace('{{%s}}' % key, val)
  return template

def WritePkgData(image_dir, pkgname, license_text, homepage):
  data = EvaluateTemplate(open(ENTRY_TEMPLATE_FILE).read(),
    dict(name=pkgname, license=license_text, url=homepage))
  base = os.path.join(image_dir, PKG_DATA_TARGET_DIR.lstrip('/'), pkgname)
  shutil.rmtree(base, ignore_errors=True)
  os.makedirs(base, 0755)
  with open(os.path.join(base, 'license_text'), 'w') as f:
    f.write(data)

def main(fullname, license_data, homepage_data, image_dir, workdir,
  stock_license_dir, forced_license_files=None):
  metadata_licenses = list(FilterUsableLicenses(license_data.split()))

  stock_licenses = {}
  # ordering matters; via this, our license texts override the gentoo
  # provided ones where desired.
  our_license_dir = os.path.dirname(os.path.abspath(__file__))
  our_license_dir = os.path.join(our_license_dir, 'licenses')
  for license_dir in [stock_license_dir, our_license_dir]:
    stock_licenses.update((x, os.path.join(license_dir, x))
      for x in os.listdir(license_dir)
        if x not in INVALID_STOCK_LICENSES)

  if forced_license_files:
    licenses_text = "\n".join(open(os.path.join(workdir, x)).read()
      for x in forced_license_files.split())
  else:
    licenses_text = identifyLicenseText(workdir, metadata_licenses,
      stock_licenses)

  homepages = homepage_data.split()
  if not homepages:
    homepage = ''
  else:
    homepage = homepages[0]

  if not licenses_text:
    raise Exception("failed identifying license text for %s" % fullname)

  WritePkgData(image_dir, fullname, licenses_text, homepage)

if __name__ == '__main__':
  # ./process-pkg.py "${CATEGORY}/${PN}-${PVR}" "${LICENSE}" "${HOMEPAGE}"
  # "${D}" "${WORKDIR}"
  # "${LICENSES_DIR}"
  if len(sys.argv) < 7:
    sys.stderr.write("not enough args; expected ${CATEGORY}/${PN}-${PVR} "
      "${LICENSE} ${HOMEPAGE} ${D} ${WORKDIR} ${PORTDIR}/licenses"
      "[FORCED_LICENSE_FILE1 .. FORCED_LICENSE_FILE9]\n"
      "got %i args: %r\n"
      % (len(sys.argv) -1, sys.argv))
    sys.exit(1)
  args = sys.argv[1:7]
  forced_license_files = sys.argv[7:]
  main(forced_license_files=forced_license_files, *sys.argv[1:])
  sys.exit(0)
