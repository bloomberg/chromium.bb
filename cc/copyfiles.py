# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import re
import shutil

prefixes = ["../third_party/WebKit/Source/WebCore/platform/graphics/chromium",
            "../third_party/WebKit/Source/WebCore/platform/graphics/chromium/cc",
            "../third_party/WebKit/Source/WebKit/chromium/tests"]

def Copy(name):
  src = name
  dst = name
  if name.startswith("test/"):
    src = src[5:]
  fullsrc = ""
  for prefix in prefixes:
    candidate = "%s/%s" % (prefix, src)
    if os.path.exists(candidate):
      fullsrc = candidate
      break
  assert fullsrc != ""
  shutil.copyfile(fullsrc, dst)
  print "copying from %s to %s" % (fullsrc, dst)
  return dst

def FixCopyrightHeaderText(text, year):
  header_template = """// Copyright %s The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
"""

  while text[0].find(" */") == -1:
    text = text[1:]
  text = text[1:]

  return (header_template % year) + "".join(text)

def FixCopyrightHeader(filepath):
  with open(filepath, "r") as f:
    text = f.readlines()

  if filepath.endswith("CCPageScaleAnimation.cpp"):
    year = 2012
  else:
    pattern = ".*Copyright \(C\) (20[01][0-9])"
    m = re.match(pattern, text[0])
    if m == None:
      m = re.match(pattern, text[1])
    assert m
    year = m.group(1)

  fixed_text = FixCopyrightHeaderText(text, year)
  with open(filepath, "w") as f:
    f.write(fixed_text)

def Readfile(gypfile):
  with open(gypfile, "r") as cc_gyp:
    obj = eval(cc_gyp.read())
    return obj

def Main():
  files = Readfile("cc.gyp")['variables']['cc_source_files']
  for f in files:
    dst = Copy(f)
    FixCopyrightHeader(dst)

  files = Readfile("cc_tests.gyp")['variables']['cc_tests_source_files']
  for f in files:
    dst = Copy(f)
    FixCopyrightHeader(dst)

if __name__ == '__main__':
  import sys
  os.chdir(os.path.dirname(__file__))
  sys.exit(Main())
