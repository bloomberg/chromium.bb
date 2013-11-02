#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""NaCl gcc wrapper that presents glibc and newlib as a single
toolchain.

This wraps the newlib and glibc compilers and allows users
to choose which one they really want by passed in --glibc
or --newlib on the command line.

We need this when using gyp to generator build files since
gyp only support one target toolchain and one host toolchain
(for now).
"""
import sys
import os

def main():
  args = sys.argv[1:]
  if '--glibc' in args:
    variant = 'glibc'
    args.remove('--glibc')
  elif '--newlib' in args:
    variant = 'newlib'
    args.remove('--newlib')
  else:
    sys.exit("Expected --glibc or --newlib in arg list")
  compiler = os.path.abspath(sys.argv[0])
  compiler = compiler.replace("linux_x86", "linux_x86_%s" % variant)
  args = [compiler] + args
  os.execv(compiler, args)

if __name__ == '__main__':
  main()
