# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Toolchain and related functionality."""

from __future__ import print_function

import os
import re

from chromite.lib import constants
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import gs
from chromite.lib import path_util
from chromite.lib import portage_util

ORDERFILE_GS_URL_UNVETTED = \
    'gs://chromeos-prebuilt/afdo-job/orderfiles/unvetted'
ORDERFILE_GS_URL_VETTED = \
    'gs://chromeos-prebuilt/afdo-job/orderfiles/vetted'
ORDERFILE_COMPRESSION_SUFFIX = '.xz'


class Error(Exception):
  """Base module error class."""


class GenerateChromeOrderfileError(Error):
  """Error for GenerateChromeOrderfile class."""


class GenerateChromeOrderfile(object):
  """Class to handle generation of orderfile for Chrome.

  This class takes orderfile containing symbols ordered by Call-Chain
  Clustering (C3), produced when linking Chrome, and uses a toolchain
  script to perform post processing to generate an orderfile that can
  be used for linking Chrome and creates tarball. The output of this
  script is a tarball of the orderfile and a tarball of the NM output
  of the built Chrome binary.
  """

  PROCESS_SCRIPT = ('/mnt/host/source/src/third_party/toolchain-utils/'
                    'orderfile/post_process_orderfile.py')
  CHROME_BINARY_PATH = ('/var/cache/chromeos-chrome/chrome-src-internal/'
                        'src/out_${BOARD}/Release/chrome')
  INPUT_ORDERFILE_PATH = ('/build/${BOARD}/opt/google/chrome/'
                          'chrome.orderfile.txt')

  def __init__(self, board, output_dir, chrome_version, chroot_path,
               chroot_args):
    self.output_dir = output_dir
    self.chrome_version = chrome_version
    self.chrome_binary = self.CHROME_BINARY_PATH.replace('${BOARD}', board)
    self.input_orderfile = self.INPUT_ORDERFILE_PATH.replace('${BOARD}', board)
    self.working_dir = os.path.join(chroot_path, 'tmp')
    self.working_dir_inchroot = '/tmp'
    self.chroot_path = chroot_path
    self.chroot_args = chroot_args

  def _CheckArguments(self):
    if not os.path.isdir(self.output_dir):
      raise GenerateChromeOrderfileError(
          'Non-existent directory %s specified for --out-dir', self.output_dir)

    chrome_binary_path_outside = os.path.join(self.chroot_path,
                                              self.chrome_binary[1:])
    if not os.path.exists(chrome_binary_path_outside):
      raise GenerateChromeOrderfileError(
          'Chrome binary does not exist at %s in chroot',
          chrome_binary_path_outside)

    chrome_orderfile_path_outside = os.path.join(self.chroot_path,
                                                 self.input_orderfile[1:])
    if not os.path.exists(chrome_orderfile_path_outside):
      raise GenerateChromeOrderfileError(
          'No orderfile generated in the builder.')

  def _GenerateChromeNM(self):
    """This command runs inside chroot."""
    cmd = ['llvm-nm', '-n', self.chrome_binary]
    result_inchroot = os.path.join(self.working_dir_inchroot,
                                   self.chrome_version + '.nm')
    result_out_chroot = os.path.join(self.working_dir,
                                     self.chrome_version + '.nm')

    try:
      cros_build_lib.RunCommand(
          cmd,
          log_stdout_to_file=result_out_chroot,
          enter_chroot=True,
          chroot_args=self.chroot_args)
    except cros_build_lib.RunCommandError:
      raise GenerateChromeOrderfileError(
          'Unable to run %s to get nm on Chrome binary' % (cmd))

    # Return path inside chroot
    return result_inchroot

  def _PostProcessOrderfile(self, chrome_nm):
    """This command runs inside chroot."""
    result = os.path.join(self.working_dir_inchroot,
                          self.chrome_version + '.orderfile')
    cmd = [
        self.PROCESS_SCRIPT, '--chrome', chrome_nm, '--input',
        self.input_orderfile, '--output', result
    ]

    try:
      cros_build_lib.RunCommand(
          cmd, enter_chroot=True, chroot_args=self.chroot_args)
    except cros_build_lib.RunCommandError:
      raise GenerateChromeOrderfileError(
          'Unable to run %s to process orderfile.' % (cmd))

    # Return path inside chroot
    return result

  def _CreateTarball(self, targets):
    """This command runs outside of chroot."""
    ret = []
    for t in targets:
      # The input t is the path inside chroot
      input_path = os.path.join(self.working_dir, os.path.basename(t))
      # Put output tarball in the out_dir
      compressed = os.path.basename(t) + ORDERFILE_COMPRESSION_SUFFIX
      output_path = os.path.join(self.output_dir, compressed)
      cros_build_lib.CompressFile(input_path, output_path)
      # Only return the basename
      ret.append(compressed)
    return ret

  def Perform(self):
    """Generate post-processed Chrome orderfile and create tarball."""
    self._CheckArguments()
    chrome_nm = self._GenerateChromeNM()
    orderfile = self._PostProcessOrderfile(chrome_nm)
    self._CreateTarball([chrome_nm, orderfile])


class UpdateChromeEbuildWithOrderfileError(Error):
  """Error for UpdateChromeEbuildWithOrderfile class."""

class UpdateChromeEbuildWithOrderfile(object):
  """Class to update Chrome ebuild with unvetted orderfile."""

  # regex to find orderfile declaration within the ebuild file.
  CHROME_EBUILD_ORDERFILE_REGEX = (
      r'^(?P<bef>UNVETTED_ORDERFILE=")(?P<name>.*)(?P<aft>")')
  # and corresponding replacement string
  CHROME_EBUILD_ORDERFILE_REPL = r'\g<bef>%s\g<aft>'

  def __init__(self, board, orderfile):
    self.board = board
    if ORDERFILE_COMPRESSION_SUFFIX in orderfile:
      self.orderfile = orderfile.replace(ORDERFILE_COMPRESSION_SUFFIX, '')
    else:
      self.orderfile = orderfile

  def _FindChromeEbuild(self):
    """Find the Chrome ebuild in the build root."""
    equery_prog = 'equery-%s' % self.board
    equery_cmd = [equery_prog, 'w', 'chromeos-chrome']
    ebuild_file = cros_build_lib.RunCommand(
        equery_cmd, enter_chroot=True, redirect_stdout=True).output.rstrip()
    return ebuild_file

  def _PatchChromeEbuild(self, ebuild_file):
    """Patch the Chrome ebuild to use the orderfile.

    Args:
      ebuild_file: path to the ebuild file.
    """
    original_ebuild = path_util.FromChrootPath(ebuild_file)
    modified_ebuild = '%s.new' % original_ebuild

    pattern = re.compile(self.CHROME_EBUILD_ORDERFILE_REGEX)
    orderfile = self.CHROME_EBUILD_ORDERFILE_REPL % self.orderfile
    found = False
    with open(original_ebuild) as original, \
         open(modified_ebuild, 'w') as modified:
      for line in original:
        matched = pattern.match(line)
        if matched:
          found = True
          modified.write(pattern.sub(orderfile, line))
        else:
          modified.write(line)
    if not found:
      logging.info("Unable to find markers for setting orderfile.")
      raise UpdateChromeEbuildWithOrderfileError(
          'Chrome ebuild file does not have appropriate orderfile marker')
    os.rename(modified_ebuild, original_ebuild)
    logging.info("Patched %s with %s", original_ebuild, self.orderfile)

  def _UpdateManifest(self, ebuild_file):
    """Regenerate the Manifest file. (To update orderfile)

    Args:
      ebuild_file: path to the ebuild file
    """
    ebuild_prog = 'ebuild-%s' % self.board
    cmd = [ebuild_prog, ebuild_file, 'manifest', '--force']
    cros_build_lib.RunCommand(cmd, enter_chroot=True)

  def Perform(self):
    """Main function to update Chrome ebuild with orderfile"""
    ebuild = self._FindChromeEbuild()
    self._PatchChromeEbuild(ebuild)
    # Patch the chrome 9999 ebuild too, as the manifest will use
    # 9999 ebuild.
    ebuild_9999 = os.path.join(
        os.path.dirname(ebuild), 'chromeos-chrome-9999.ebuild')
    self._PatchChromeEbuild(ebuild_9999)
    # Use 9999 ebuild to update manifest.
    self._UpdateManifest(ebuild_9999)


def FindLatestChromeOrderfile(gs_url):
  """Find the latest unvetted Chrome orderfile.

  Args:
    gs_url: The full path to GS bucket URL.

  Returns:
    The orderfile name.
  """
  gs_context = gs.GSContext()
  orderfiles = sorted(
      gs_context.List(gs_url, details=True), key=lambda x: x.creation_time)
  orderfile_url = orderfiles[-1].url
  orderfile_name = os.path.basename(orderfile_url)
  logging.info("Latest orderfile in %s is %s", gs_url, orderfile_name)
  return orderfile_name


def CheckOrderfileExists(buildroot, orderfile_verify):
  """Checks if the orderfile to be generated/verified already exists.

  Args:
    buildroot: The path of build root
    orderfile_verify: Whether it's for orderfile_verify or orderfile_generate.
    For orderfile verify builder, it verifies the most recent orderfile from
    the unvetted bucket. So we checks if it's in the vetted bucket or not.
    For orderfile generate builder, it generates an orderfile based on the
    Chrome version number. So we need to find out the Chrome is upreved than
    the most recent unvetted orderfile or not.

  Returns:
    True if the orderfile is in the GS bucket already.
  """
  gs_context = gs.GSContext()
  if orderfile_verify:
    orderfile_name = FindLatestChromeOrderfile(ORDERFILE_GS_URL_UNVETTED)
    # Check if the latest unvetted orderfile is already verified
    return gs_context.Exists(ORDERFILE_GS_URL_VETTED + '/' + orderfile_name)

  # For orderfile_generate builder, get the chrome version
  cpv = portage_util.PortageqBestVisible(constants.CHROME_CP, cwd=buildroot)
  orderfile_name = '{0}-orderfile-{1}'.format(cpv.package, cpv.version)
  return gs_context.Exists(ORDERFILE_GS_URL_UNVETTED + '/' + orderfile_name +
                           ORDERFILE_COMPRESSION_SUFFIX)


def OrderfileUpdateChromeEbuild(board):
  """Update Chrome ebuild with latest unvetted orderfile.

  Args:
    board: Board type that was built on this machine.

  Returns:
    True, if the Chrome ebuild is successfully updated.
    False, if the latest unvetted orderfile is already verified.
  """

  orderfile_name = FindLatestChromeOrderfile(ORDERFILE_GS_URL_UNVETTED)
  updater = UpdateChromeEbuildWithOrderfile(board, orderfile_name)
  updater.Perform()
  return True
