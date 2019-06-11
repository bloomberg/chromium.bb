# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Toolchain and related functionality."""

from __future__ import print_function

import os

from chromite.lib import cros_build_lib


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
  COMPRESSION_SUFFIX = '.tar.xz'

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

    return result

  def _CreateTarball(self, targets):
    """This command runs outside of chroot."""
    ret = []
    for t in targets:
      tar_name = os.path.basename(t) + self.COMPRESSION_SUFFIX
      # Put output tarball in the out_dir
      target = os.path.join(self.output_dir, tar_name)
      # CreateTarball runs within tempdir, so need to use the relative
      # path of the output tarball
      cros_build_lib.CreateTarball(
          os.path.relpath(target, self.working_dir),
          cwd=self.working_dir,
          inputs=[os.path.basename(t)])
      ret.append(tar_name)
    return ret

  def Perform(self):
    """Generate post-processed Chrome orderfile and create tarball."""
    self._CheckArguments()
    chrome_nm = self._GenerateChromeNM()
    orderfile = self._PostProcessOrderfile(chrome_nm)
    self._CreateTarball([chrome_nm, orderfile])
