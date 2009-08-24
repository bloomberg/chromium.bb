#!/usr/bin/env python
# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# TODO: remove this script when GYP has for loops

import sys

def main(argv):
  if len(argv) < 3:
    print 'ERROR: need string and list of locales'
    return 1

  str_template = argv[1]
  locales = argv[2:]

  results = []
  for locale in locales:
    results.append(str_template.replace('ZZLOCALE', locale))

  # Quote each element so filename spaces don't mess up GYP's attempt to parse
  # it into a list.
  print " ".join(['"%s"' % x for x in results])

if __name__ == '__main__':
  sys.exit(main(sys.argv))
