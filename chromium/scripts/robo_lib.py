#!/usr/bin/python
#
# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Various utility fns for robosushi.

import os
from subprocess import call
import subprocess

def log(msg):
  print "[ %s ]" % msg

class UserInstructions(Exception):
  """Handy exception subclass that just prints very verbose instructions to the
  user.  Normal exceptions tend to lose the message in the stack trace, which we
  probably don't care about."""
  def __init__ (self, msg):
    self._msg = msg

  def __str__(self):
    sep = "=" * 78
    return "\n\n%s\n%s\n%s\n\n" % (sep, self._msg, sep)

class RoboConfiguration:
  def __init__(self):
    """Ensure that our config has basic fields fill in, and passes some sanity
    checks too.

    Important: We might be doing --setup, so these sanity checks should only be
    for things that we don't plan for fix as part of that.
    """
    self.set_prompt_on_call(False)
    # This is the prefix that our branches start with.
    self._sushi_branch_prefix = "sushi-"
    # This is the title that we use for the commit with GN configs.
    self._gn_commit_title = "GN Configuration"
    # Title of the commit with chromium/patches/README.
    self._patches_commit_title = "Chromium patches file"
    # Title of the commit with README.chromium
    self._readme_chromium_commit_title = "README.chromium file"
    self.EnsureHostInfo()
    self.EnsureChromeSrc()

    # Directory where llvm lives.
    self._llvm_path = os.path.join(self.chrome_src(), "third_party",
            "llvm-build", "Release+Asserts", "bin")

    self.EnsurePathContainsLLVM()
    log("Using chrome src: %s" % self.chrome_src())
    self.EnsureFFmpegHome()
    log("Using ffmpeg home: %s" % self.ffmpeg_home())
    self.EnsureASANConfig()
    self.ComputeBranchName()
    log("On branch: %s" % self.branch_name())
    if self.sushi_branch_name():
      log("On sushi branch: %s" % self.sushi_branch_name())

    # Filename that we'll ask generate_gn.py to write git commands to.
    self._autorename_git_file = os.path.join(
                                  self.ffmpeg_home(),
                                  "chromium",
                                  "scripts",
                                  ".git_commands.sh")


  def chrome_src(self):
    """Return /path/to/chromium/src"""
    return self._chrome_src

  def host_operating_system(self):
    """Return host type, e.g. "linux" """
    return self._host_operating_system

  def host_architecture(self):
    """Return host architecture, e.g. "x64" """
    return self._host_architecture

  def ffmpeg_home(self):
    """Return /path/to/third_party/ffmpeg"""
    return self._ffmpeg_home

  def relative_asan_directory(self):
    return self._relative_asan_directory

  def absolute_asan_directory(self):
    return os.path.join(self.chrome_src(), self.relative_asan_directory())

  def chdir_to_chrome_src(self):
    os.chdir(self.chrome_src())

  def chdir_to_ffmpeg_home(self):
    os.chdir(self.ffmpeg_home())

  def branch_name(self):
    """Return the current workspace's branch name, or None.  This might be any
    branch (e.g., "master"), not just one that we've created."""
    return self._branch_name

  def sushi_branch_name(self):
    """If the workspace is currently on a branch that we created (a "sushi
    branch"), return it.  Else return None."""
    return self._sushi_branch_name

  def sushi_branch_prefix(self):
    """Return the branch name that indicates that this is a "sushi branch"."""
    return self._sushi_branch_prefix

  def gn_commit_title(self):
    return self._gn_commit_title

  def patches_commit_title(self):
    return self._patches_commit_title

  def readme_chromium_commit_title(self):
    return self._readme_chromium_commit_title

  def nasm_path(self):
    return self._nasm_path

  def EnsureHostInfo(self):
    """Ensure that the host architecture and platform are set."""
    # TODO(liberato): autodetect
    self._host_operating_system = "linux"
    self._host_architecture = "x64"

  def EnsureChromeSrc(self):
    """Find the /absolute/path/to/my_chrome_dir/src"""
    wd = os.getcwd()
    # Walk up the tree until we find src/AUTHORS
    while wd != "/":
      if os.path.isfile(os.path.join(wd, "src", "AUTHORS")):
        self._chrome_src = os.path.join(wd, "src")
        return
      wd = os.path.dirname(wd)
    raise Exception("could not find src/AUTHORS in any parent of the wd")

  def EnsureFFmpegHome(self):
    """Ensure that |self| has "ffmpeg_home" set."""
    self._ffmpeg_home = os.path.join(self.chrome_src(), "third_party", "ffmpeg")

  def EnsureASANConfig(self):
    """Find the asan directories.  Note that we don't create them."""
    # Compute gn ASAN out dirnames.
    self.chdir_to_chrome_src();
    local_directory = os.path.join("out", "sushi_asan")
    # ASAN dir, suitable for 'ninja -C'
    self._relative_asan_directory = local_directory

  def EnsurePathContainsLLVM(self):
    """Make sure that we have chromium's LLVM in $PATH.

    We don't want folks to accidentally use the wrong clang.
    """

    llvm_path = os.path.join(self.chrome_src(), "third_party",
            "llvm-build", "Release+Asserts", "bin")
    if self.llvm_path() not in os.environ["PATH"]:
      raise UserInstructions(
                          "Please add:\n%s\nto the beginning of $PATH" %
                          self.llvm_path())
  def llvm_path(self):
    return self._llvm_path

  def ComputeBranchName(self):
    """Get the current branch name and set it."""
    self.chdir_to_ffmpeg_home()
    branch_name = subprocess.Popen(["git", "rev-parse", "--abbrev-ref", "HEAD"],
          stdout=subprocess.PIPE).communicate()[0].strip()
    self.SetBranchName(branch_name)

  def SetBranchName(self, name):
    """Set our branch name, which may be a sushi branch or not."""
    self._branch_name = name
    # If this is one of our branches, then record that too.
    if name and not name.startswith(self.sushi_branch_prefix()):
      name = None
    self._sushi_branch_name = name

  def autorename_git_file(self):
    return self._autorename_git_file

  def prompt_on_call(self):
    """ Return True if and only if we're supposed to ask the user before running
    any command that might have a side-effect."""
    return self._prompt_on_call

  def set_prompt_on_call(self, value):
    self._prompt_on_call = value

  def Call(self, args, shell=False):
    """Run the command specified by |args| (see subprocess.call), optionally
    prompting the user."""
    if self.prompt_on_call():
      print("[%s] About to run: %s " % (os.getcwd(), args))
      raw_input("Press ENTER to continue, or interrupt the script: ")
    return call(args, shell=shell)

