#!/usr/bin/env python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Usage:
#    gclient-new-workdir.py <repository> <new_workdir> [<branch>]
#

import os
import shutil
import subprocess
import sys


def parse_options(argv):
  assert not sys.platform.startswith("win")

  if len(argv) != 3:
    print("usage: gclient-new-workdir.py <repository> <new_workdir>")
    sys.exit(1)

  repository = argv[1]
  new_workdir = argv[2]

  if not os.path.exists(repository):
    print("Repository does not exist: " + repository)
    sys.exit(1)

  if os.path.exists(new_workdir):
    print("New workdir already exists: " + new_workdir)
    sys.exit(1)

  return repository, new_workdir


def main(argv):
  repository, new_workdir = parse_options(argv)

  gclient = os.path.join(repository, ".gclient")
  if not os.path.exists(gclient):
    print("No .gclient file: " + gclient)

  gclient_entries = os.path.join(repository, ".gclient_entries")
  if not os.path.exists(gclient_entries):
    print("No .gclient_entries file: " + gclient_entries)

  os.mkdir(new_workdir)
  os.symlink(gclient, os.path.join(new_workdir, ".gclient"))
  os.symlink(gclient_entries, os.path.join(new_workdir, ".gclient_entries"))

  for root, dirs, _ in os.walk(repository):
    if ".git" in dirs:
      workdir = root.replace(repository, new_workdir, 1)
      make_workdir(os.path.join(root, ".git"),
                   os.path.join(workdir, ".git"))


def make_workdir(repository, new_workdir):
  print("Creating: " + new_workdir)
  os.makedirs(new_workdir)

  GIT_DIRECTORY_WHITELIST = [
    "config",
    "info",
    "hooks",
    "logs/refs",
    "objects",
    "packed-refs",
    "refs",
    "remotes",
    "rr-cache",
    "svn"
  ]

  for entry in GIT_DIRECTORY_WHITELIST:
    make_symlink(repository, new_workdir, entry)

  shutil.copy2(os.path.join(repository, "HEAD"),
               os.path.join(new_workdir, "HEAD"))
  subprocess.check_call(["git", "checkout", "-f"],
                        cwd=new_workdir.rstrip(".git"))


def make_symlink(repository, new_workdir, link):
  if not os.path.exists(os.path.join(repository, link)):
    return
  link_dir = os.path.dirname(os.path.join(new_workdir, link))
  if not os.path.exists(link_dir):
    os.makedirs(link_dir)
  os.symlink(os.path.join(repository, link), os.path.join(new_workdir, link))


if __name__ == '__main__':
  sys.exit(main(sys.argv))
