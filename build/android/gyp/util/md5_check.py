# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import hashlib
import os


def UpdateMd5ForFile(md5, path, block_size=2**16):
  with open(path, 'rb') as infile:
    while True:
      data = infile.read(block_size)
      if not data:
        break
      md5.update(data)


def UpdateMd5ForDirectory(md5, dir_path):
  for root, _, files in os.walk(dir_path):
    for f in files:
      UpdateMd5ForFile(md5, os.path.join(root, f))


def UpdateMd5ForPath(md5, path):
  if os.path.isdir(path):
    UpdateMd5ForDirectory(md5, path)
  else:
    UpdateMd5ForFile(md5, path)


class Md5Checker(object):
  def __init__(self, stamp=None, inputs=[], command=[]):
    self.stamp = stamp

    md5 = hashlib.md5()
    for i in inputs:
      UpdateMd5ForPath(md5, i)
    for s in command:
      md5.update(s)
    self.new_digest = md5.hexdigest()

    self.old_digest = ''
    if os.path.exists(stamp):
      with open(stamp, 'r') as old_stamp:
        self.old_digest = old_stamp.read()

  def IsStale(self):
    return self.old_digest != self.new_digest

  def Write(self):
    with open(self.stamp, 'w') as new_stamp:
      new_stamp.write(self.new_digest)
