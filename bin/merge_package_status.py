#!/usr/bin/python2.6
# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Merge multiple csv files representing Portage package data into
one csv file, in preparation for uploading to a Google Docs spreadsheet.
"""

import optparse
import os
import sys

import cros_portage_upgrade

sys.path.append(os.path.join(os.path.dirname(__file__), '..', '..'))
import chromite.lib.table as table
import chromite.lib.cros_build_lib as cros_lib

COL_PACKAGE = cros_portage_upgrade.UpgradeTable.COL_PACKAGE
COL_SLOT = cros_portage_upgrade.UpgradeTable.COL_SLOT
COL_TARGET = cros_portage_upgrade.UpgradeTable.COL_TARGET
COL_OVERLAY = cros_portage_upgrade.UpgradeTable.COL_OVERLAY
ID_COLS = [COL_PACKAGE, COL_SLOT]

# A bit of hard-coding with knowledge of how cros targets work.
CHROMEOS_TARGET_ORDER = ['chromeos', 'chromeos-dev', 'chromeos-test']
def _GetCrosTargetRank(target):
  """Hard-coded ranking of known/expected chromeos root targets for sorting.

  The lower the ranking, the earlier in the target list it falls by
  convention.  In other words, in the typical target combination
  "chromeos chromeos-dev", "chromeos" has a lower ranking than "chromeos-dev".

  All valid rankings are greater than zero.

  Return valid ranking for target or a false value if target is unrecognized."""
  for ix, targ in enumerate(CHROMEOS_TARGET_ORDER):
    if target == targ:
      return ix + 1 # Avoid a 0 (non-true) result
  return None

def _ProcessTargets(targets, reverse_cros=False):
  """Process a list of |targets| to smaller, sorted list.

  For example:
  chromeos chromeos-dev -> chromeos-dev
  chromeos chromeos-dev world -> chromeos-dev world
  world hard-host-depends -> hard-host-depends world

  The one chromeos target always comes back first, with targets
  otherwise sorted alphabetically.  The chromeos target that is
  kept will be the one with the highest 'ranking', as decided
  by _GetCrosTargetRank.  To reverse the ranking sense, specify
  |reverse_cros| as True.

  These rules are specific to how we want the information to appear
  in the final spreadsheet.
  """
  if targets:
    # Sort cros targets according to "rank".
    cros_targets = [t for t in targets if _GetCrosTargetRank(t)]
    cros_targets.sort(key=_GetCrosTargetRank, reverse=reverse_cros)

    # Don't condense non-cros targets.
    other_targets = [t for t in targets if not _GetCrosTargetRank(t)]
    other_targets.sort()

    # Assemble final target list, with single cros target first.
    final_targets = []
    if cros_targets:
      final_targets.append(cros_targets[-1])
    if other_targets:
      final_targets.extend(other_targets)

    return final_targets

def _ProcessRowTargetValue(row):
  """Condense targets like 'chromeos chromeos-dev' to just 'chromeos-dev'."""
  targets = row[COL_TARGET].split()
  if targets:
    processed_targets = _ProcessTargets(targets)
    row[COL_TARGET] = ' '.join(processed_targets)

def LoadTable(filepath):
  """Load the csv file at |filepath| into a table.Table object."""
  csv_table = table.Table.LoadFromCSV(filepath)

  # Process the Target column now.
  csv_table.ProcessRows(_ProcessRowTargetValue)

  return csv_table

def LoadTables(args):
  """Load all csv files in |args| into one merged table.  Return table."""
  def TargetMerger(col, val, other_val):
    """Function to merge two values in Root Target column from two tables."""
    targets = []
    if val:
      targets.extend(val.split())
    if other_val:
      targets.extend(other_val.split())

    processed_targets = _ProcessTargets(targets, reverse_cros=True)
    return ' '.join(processed_targets)

  def DefaultMerger(col, val, other_val):
    """Merge |val| and |other_val| in column |col| for some row."""
    # This function is registered as the default merge function,
    # so verify that the column is a supported one.
    prfx = cros_portage_upgrade.UpgradeTable.COL_DEPENDS_ON.replace('ARCH', '')
    if col.startswith(prfx):
      # Merge dependencies by taking the superset.
      deps = set(val.split())
      other_deps = set(other_val.split())
      all_deps = deps.union(other_deps)
      return ' '.join(sorted(dep for dep in all_deps))

    # Raise a generic ValueError, which MergeTable function will clarify.
    # The effect should be the same as having no merge_rule for this column.
    raise ValueError

  # This is only needed because the automake-wrapper package is coming from
  # different overlays for different boards right now!
  def MergeWithAND(col, val, other_val):
    """For merging columns that might have differences but should not!."""
    if not val:
      return '"" AND ' + other_val
    if not other_val + ' AND ""':
      return val
    return val + " AND " + other_val

  # Prepare merge_rules with the defined functions.
  merge_rules = {COL_TARGET: TargetMerger,
                 COL_OVERLAY: MergeWithAND,
                 '__DEFAULT__': DefaultMerger,
                 }

  # Load and merge the files.
  print "Loading csv table from '%s'." % (args[0])
  csv_table = LoadTable(args[0])
  if len(args) > 1:
    for arg in args[1:]:
      print "Loading csv table from '%s'." % (arg)
      tmp_table = LoadTable(arg)

      print "Merging tables into one."
      csv_table.MergeTable(tmp_table, ID_COLS,
                           merge_rules=merge_rules, allow_new_columns=True)

  # Sort the table by package name, then slot.
  def IdSort(row):
    return tuple(row[col] for col in ID_COLS)
  csv_table.Sort(IdSort)

  return csv_table

def FinalizeTable(csv_table):
  """Process the table to prepare it for upload to online spreadsheet."""
  print "Processing final table to prepare it for upload."

  col_ver = cros_portage_upgrade.UpgradeTable.COL_CURRENT_VER
  col_arm_ver = cros_portage_upgrade.UpgradeTable.GetColumnName(col_ver, 'arm')
  col_x86_ver = cros_portage_upgrade.UpgradeTable.GetColumnName(col_ver, 'x86')

  # Insert new columns
  col_cros_target = "ChromeOS Root Target"
  col_host_target = "Host Root Target"
  col_cmp_arch = "Comparing arm vs x86 Versions"
  csv_table.AppendColumn(col_cros_target)
  csv_table.AppendColumn(col_host_target)
  csv_table.AppendColumn(col_cmp_arch)

  # Row by row processing
  for row in csv_table:
    # If the row is not unique when just the package
    # name is considered, then add a ':<slot>' suffix to the package name.
    id_values = { COL_PACKAGE: row[COL_PACKAGE] }
    matching_rows = csv_table.GetRowsByValue(id_values)
    if len(matching_rows) > 1:
      for mr in matching_rows:
        mr[COL_PACKAGE] = mr[COL_PACKAGE] + ":" + mr[COL_SLOT]

    # Split target column into cros_target and host_target columns
    target_str = row.get(COL_TARGET, None)
    if target_str:
      targets = target_str.split()
      cros_targets = []
      host_targets = []
      for target in targets:
        if _GetCrosTargetRank(target):
          cros_targets.append(target)
        else:
          host_targets.append(target)

      row[col_cros_target] = ' '.join(cros_targets)
      row[col_host_target] = ' '.join(host_targets)

    # Compare x86 vs. arm version, add result to col_cmp_arch.
    x86_ver = row.get(col_x86_ver, None)
    arm_ver = row.get(col_arm_ver, None)
    if x86_ver and arm_ver:
      if x86_ver != arm_ver:
        row[col_cmp_arch] = "different"
      elif x86_ver:
        row[col_cmp_arch] = "same"

def WriteTable(csv_table, outpath):
  """Write |csv_table| out to |outpath| as csv."""
  try:
    fh = open(outpath, 'w')
    csv_table.WriteCSV(fh)
    print "Wrote merged table to '%s'" % outpath
  except IOError as ex:
    print "Unable to open %s for write: %s" % (outpath, str(ex))
    raise

def main():
  """Main function."""
  usage = 'Usage: %prog --out=merged_csv_file input_csv_files...'
  parser = optparse.OptionParser(usage=usage)
  parser.add_option('--finalize-for-upload', dest='finalize',
                    action='store_true', default=False,
                    help="Some processing of output for upload purposes.")
  parser.add_option('--out', dest='outpath', type='string',
                    action='store', default=None,
                    help="File to write merged results to")

  (options, args) = parser.parse_args()

  # Check required options
  if not options.outpath:
    parser.print_help()
    cros_lib.Die("The --out option is required.")
  if len(args) < 1:
    parser.print_help()
    cros_lib.Die("At least one input_csv_file is required.")

  csv_table = LoadTables(args)

  if options.finalize:
    FinalizeTable(csv_table)

  WriteTable(csv_table, options.outpath)

if __name__ == '__main__':
  main()
