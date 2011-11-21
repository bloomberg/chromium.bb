#!/usr/bin/python
# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Fake implementation of gsutils, which is the utility used to upload files
to commondatastorage'''

import optparse
import sys


def HandleLS(args):
  if len(args) == 0:
    print 'gs://nativeclient-upload/'
    return 0
  print ('InvalidUriError: Attempt to get key for "%s" failed. '
         'This probably indicates the URI is invalid.' % args[0])
  return 1


def UnknownCommand(args):
  return 0


def HandleCP(args):
  return 0


def main(args):
  if len(args) == 0:
    return 0
  COMMANDS = {
      'ls': HandleLS,
      'cp': HandleCP,
      }
  return COMMANDS.get(args[0], UnknownCommand)(args[1:])

if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
