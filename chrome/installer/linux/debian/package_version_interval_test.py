#!/usr/bin/env python
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import deb_version
import package_version_interval

def make_interval(start_open, start_inclusive, start_cmp,
                  end_open, end_inclusive, end_cmp):
  start = package_version_interval.PackageVersionIntervalEndpoint(
      start_open, start_inclusive, start_cmp)
  end = package_version_interval.PackageVersionIntervalEndpoint(
      end_open, end_inclusive, end_cmp)
  return package_version_interval.PackageVersionInterval(start, end)


# intersect() test.
assert (make_interval(True, None, None, False, True, 10).intersect(
    make_interval(False, True, 5, True, None, None)) ==
      make_interval(False, True, 5, False, True, 10))
assert (make_interval(False, True, 3, False, True, 7).intersect(
    make_interval(False, True, 4, False, True, 6)) ==
      make_interval(False, True, 4, False, True, 6))
assert (make_interval(False, False, 3, False, False, 7).intersect(
    make_interval(False, True, 3, False, True, 7)) ==
      make_interval(False, False, 3, False, False, 7))

# contains() test.
assert make_interval(False, False, 3, False, False, 7).contains(5)
assert not make_interval(False, False, 3, False, False, 7).contains(3)
assert not make_interval(False, False, 3, False, False, 7).contains(7)
assert make_interval(False, True, 3, False, True, 7).contains(3)
assert make_interval(False, True, 3, False, True, 7).contains(7)
assert make_interval(True, None, None, False, True, 7).contains(5)
assert make_interval(False, True, 3, True, None, None).contains(5)
assert not make_interval(True, None, None, False, True, 7).contains(8)
assert not make_interval(False, True, 3, True, None, None).contains(2)

# parse_dep() test.
assert (package_version_interval.parse_dep('libfoo (> 1.0)') ==
        ('libfoo', make_interval(False, False, deb_version.DebVersion('1.0'),
                                 True, None, None)))
assert (package_version_interval.parse_dep('libbar (>> a.b.c)') ==
        ('libbar', make_interval(False, False, deb_version.DebVersion('a.b.c'),
                                 True, None, None)))
assert (package_version_interval.parse_dep('libbaz (= 2:1.2.3-1)') ==
        ('libbaz', make_interval(
            False, True, deb_version.DebVersion('2:1.2.3-1'),
            False, True, deb_version.DebVersion('2:1.2.3-1'))))

# format_package_intervals() test.
actual = package_version_interval.format_package_intervals({
    'a': make_interval(True, None, None, True, None, None),
    'b': make_interval(False, False, 1, True, None, None),
    'c': make_interval(True, None, None, False, False, 2),
    'd': make_interval(False, True, 3, True, None, None),
    'e': make_interval(True, None, None, False, True, 4),
    'f': make_interval(False, True, 5, False, True, 5),
    'g': make_interval(False, False, 6, False, False, 7),
})
expected = """a
b (>> 1)
c (<< 2)
d (>= 3)
e (<= 4)
f (= 5)
g (<< 7)
g (>> 6)
"""
assert expected == actual
