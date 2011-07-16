#!/usr/bin/python2.6
# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Perform various tasks related to updating Portage packages."""

import logging
import optparse
import os
import parallel_emerge
import portage
import re
import shutil
import sys
import tempfile

import chromite.lib.table as table

class UpgradeTable(table.Table):
  """Class to represent upgrade data in memory, can be written to csv/html."""
  # TODO(mtennant): Remove html output - it isn't used.

  # Column names.  Note that 'ARCH' is replaced with a real arch name when
  # these are accessed as attributes off an UpgradeTable object.
  COL_PACKAGE = 'Package'
  COL_SLOT = 'Slot'
  COL_OVERLAY = 'Overlay'
  COL_CURRENT_VER = 'Current ARCH Version'
  COL_STABLE_UPSTREAM_VER = 'Stable Upstream ARCH Version'
  COL_LATEST_UPSTREAM_VER = 'Latest Upstream ARCH Version'
  COL_STATE = 'State On ARCH'
  COL_DEPENDS_ON = 'Dependencies On ARCH'
  COL_TARGET = 'Root Target'
  COL_UPGRADED = 'Upgraded ARCH Version'

  # COL_STATE values should be one of the following:
  STATE_UNKNOWN = 'unknown'
  STATE_NEEDS_UPGRADE = 'needs upgrade'
  STATE_PATCHED = 'patched locally'
  STATE_DUPLICATED = 'duplicated locally'
  STATE_NEEDS_UPGRADE_AND_PATCHED = 'needs upgrade and patched locally'
  STATE_NEEDS_UPGRADE_AND_DUPLICATED = 'needs upgrade and duplicated locally'
  STATE_CURRENT = 'current'

  @staticmethod
  def GetColumnName(col, arch=None):
    """Translate from generic column name to specific given |arch|."""
    if arch:
      return col.replace('ARCH', arch)
    return col

  def __init__(self, arch, upgrade=False, name=None):
    self._arch = arch

    # These constants serve two roles, for both csv and html table output:
    # 1) Restrict which column names are valid.
    # 2) Specify the order of those columns.
    columns = [self.COL_PACKAGE,
               self.COL_SLOT,
               self.COL_OVERLAY,
               self.COL_CURRENT_VER,
               self.COL_STABLE_UPSTREAM_VER,
               self.COL_LATEST_UPSTREAM_VER,
               self.COL_STATE,
               self.COL_DEPENDS_ON,
               self.COL_TARGET,
               ]

    if upgrade:
      columns.append(self.COL_UPGRADED)

    table.Table.__init__(self, columns, name=name)

  def __getattribute__(self, name):
    """When accessing self.COL_*, substitute ARCH name."""
    if name.startswith('COL_'):
      text = getattr(UpgradeTable, name)
      return UpgradeTable.GetColumnName(text, arch=self._arch)
    else:
      return object.__getattribute__(self, name)

  def GetArch(self):
    """Get the architecture associated with this UpgradeTable."""
    return self._arch

  def WriteHTML(self, filehandle):
    """Write table out as a custom html table to |filehandle|."""
    # Basic HTML, up to and including start of table and table headers.
    filehandle.write('<html>\n')
    filehandle.write('  <table border="1" cellspacing="0" cellpadding="3">\n')
    filehandle.write('    <caption>Portage Package Status</caption>\n')
    filehandle.write('    <thead>\n')
    filehandle.write('      <tr>\n')
    filehandle.write('        <th>%s</th>\n' %
             '</th>\n        <th>'.join(self._columns))
    filehandle.write('      </tr>\n')
    filehandle.write('    </thead>\n')
    filehandle.write('    <tbody>\n')

    # Now write out the rows.
    for row in self._rows:
      filehandle.write('      <tr>\n')
      for col in self._columns:
        val = row.get(col, "")

        # Add color to the text in specific cases.
        if val and col == self.COL_STATE:
          # Add colors for state column.
          if val == self.STATE_NEEDS_UPGRADE or val == self.STATE_UNKNOWN:
            val = '<span style="color:red">%s</span>' % val
          elif (val == self.STATE_NEEDS_UPGRADE_AND_DUPLICATED or
                val == self.STATE_NEEDS_UPGRADE_AND_PATCHED):
            val = '<span style="color:red">%s</span>' % val
          elif val == self.STATE_CURRENT:
            val = '<span style="color:green">%s</span>' % val
        if val and col == self.COL_DEPENDS_ON:
          # Add colors for dependencies column.  If a dependency is itself
          # out of date, then make it red.
          vallist = []
          for cpv in val.split(' '):
            # Get category/packagename from cpv, in order to look up row for
            # the dependency.  Then see if that pkg is red in its own row.
            catpkg = Upgrader._GetCatPkgFromCpv(cpv)
            deprow = self.GetRowsByValue({self.COL_PACKAGE: catpkg})[0]
            if (deprow[self.COL_STATE] == self.STATE_NEEDS_UPGRADE or
                deprow[self.COL_STATE] == self.STATE_UNKNOWN):
              vallist.append('<span style="color:red">%s</span>' % cpv)
            else:
              vallist.append(cpv)
          val = ' '.join(vallist)

        filehandle.write('        <td>%s</td>\n' % val)

      filehandle.write('      </tr>\n')

    # Finish the table and html
    filehandle.write('    </tbody>\n')
    filehandle.write('  </table>\n')
    filehandle.write('</html>\n')
