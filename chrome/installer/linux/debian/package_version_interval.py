#!/usr/bin/env python
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import re
import sys

import deb_version

class PackageVersionIntervalEndpoint:
  def __init__(self, is_open, is_inclusive, version):
    self._is_open = is_open;
    self._is_inclusive = is_inclusive
    self._version = version

  def _intersect(self, other, is_start):
    if self._is_open and other._is_open:
      return self
    if self._is_open:
      return other
    if other._is_open:
      return self
    cmp_code = self._version.__cmp__(other._version)
    if not is_start:
      cmp_code *= -1
    if cmp_code > 0:
      return self
    if cmp_code < 0:
      return other
    if not self._is_inclusive:
      return self
    return other

  def __str__(self):
    return 'PackageVersionIntervalEndpoint(%s, %s, %s)' % (
        self._is_open, self._is_inclusive, self._version)

  def __eq__(self, other):
    if self._is_open and other._is_open:
      return True
    return (self._is_open == other._is_open and
            self._is_inclusive == other._is_inclusive and
            self._version == other._version)


class PackageVersionInterval:
  def __init__(self, start, end):
    self.start = start
    self.end = end

  def contains(self, version):
    if not self.start._is_open:
      if self.start._is_inclusive:
        if version < self.start._version:
          return False
      elif version <= self.start._version:
        return False
    if not self.end._is_open:
      if self.end._is_inclusive:
        if version > self.end._version:
          return False
      elif version >= self.end._version:
        return False
    return True

  def intersect(self, other):
    return PackageVersionInterval(self.start._intersect(other.start, True),
                                  self.end._intersect(other.end, False))

  def __str__(self):
    return 'PackageVersionInterval(%s, %s)' % (str(self.start), str(self.end))

  def __eq__(self, other):
    return self.start == other.start and self.end == other.end


def version_interval_from_exp(op, version):
  open_endpoint = PackageVersionIntervalEndpoint(True, None, None)
  inclusive_endpoint = PackageVersionIntervalEndpoint(False, True, version)
  exclusive_endpoint = PackageVersionIntervalEndpoint(False, False, version)
  if op == '>=':
    return PackageVersionInterval(inclusive_endpoint, open_endpoint)
  if op == '<=':
    return PackageVersionInterval(open_endpoint, inclusive_endpoint)
  if op == '>>' or op == '>':
    return PackageVersionInterval(exclusive_endpoint, open_endpoint)
  if op == '<<' or op == '<':
    return PackageVersionInterval(open_endpoing, exclusive_endpoint)
  assert op == '='
  return PackageVersionInterval(inclusive_endpoint, inclusive_endpoint)


def parse_dep(dep):
  """Parses a package and version requirement formatted by dpkg-shlibdeps.

  Args:
      dep: A string of the format "package (op version)"

  Returns:
      A tuple of the form (package_string, PackageVersionInterval)
  """
  package_name_regex = '[a-z][a-z0-9\+\-\.]+'
  match = re.match('^(%s)$' % package_name_regex, dep)
  if match:
    return (match.group(1), PackageVersionInterval(
        PackageVersionIntervalEndpoint(True, None, None),
        PackageVersionIntervalEndpoint(True, None, None)))
  match = re.match('^(%s) \(([\>\=\<]+) ([\~0-9A-Za-z\+\-\.\:]+)\)$' %
                   package_name_regex, dep)
  if match:
    return (match.group(1), version_interval_from_exp(
        match.group(2), deb_version.DebVersion(match.group(3))))
  # At the time of writing this script, Chrome does not have any
  # complex version requirements like 'version >> 3 | version << 2'.
  print ('Conjunctions and disjunctions in package version requirements are ' +
         'not implemented at this time.')
  sys.exit(1)


def format_package_intervals(m):
  """Formats package versions suitable for use by dpkg-deb.

  Args:
      m: A map from package names to PackageVersionInterval's

  Returns:
      A formatted string of package-versions for use by dpkg-deb.  Ex:
          package1 (< left_endpoint1)
          package1 (>= right_endpoint1)
          package2 (< left_endpoint2)
          package2 (>= right_endpoint2)
  """
  lines = []
  for package in m:
    interval = m[package]
    if interval.start._is_open and interval.end._is_open:
      lines.append(package + '\n')
    elif (not interval.start._is_open and not interval.end._is_open and
          interval.start._version == interval.end._version):
      assert interval.start._is_inclusive and interval.end._is_inclusive
      lines.append(package + ' (= ' + str(interval.start._version) + ')\n')
    else:
      if not interval.start._is_open:
        op = '>=' if interval.start._is_inclusive else '>>'
        lines.append(package + ' (' + op + ' ' +
                     str(interval.start._version) +')\n')
      if not interval.end._is_open:
        op = '<=' if interval.end._is_inclusive else '<<'
        lines.append(package + ' (' + op + ' ' +
                     str(interval.end._version) + ')\n')
  return ''.join(sorted(lines))
