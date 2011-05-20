#!/usr/bin/python2.6
# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Support generic spreadsheet-like table information."""

class Table(object):
  """Class to represent column headers and rows of data."""

  __slots__ = ['_columns',     # List of column headers in order
               '_column_set',  # Set of column headers (for faster lookup)
               '_rows',        # List of row dicts
               ]

  def __init__(self, columns):
    self._columns = columns
    self._column_set = set(columns)
    self._rows = []

  def __str__(self):
    """Return a table-like string representation of this table."""
    cols = ['%10s' % col for col in self._columns]
    text = 'Columns: %s\n' % ', '.join(cols)

    ix = 0
    for row in self._rows:
      vals = ['%10s' % row[col] for col in self._columns]
      text += 'Row %3d: %s\n' % (ix, ', '.join(vals))
      ix += 1
    return text

  def __len__(self):
    """Length of table equals the number of rows."""
    return self.GetNumRows()

  def __getitem__(self, index):
    """Access one or more rows by index or slice."""
    return self.GetRowByIndex(index)

  def __setitem__(self, key, item):
    """Set one or more rows by index or slice."""
    raise NotImplementedError("Implementation not done, yet.")

  def __delitem__(self, index):
    """Delete one or more rows by index or slice."""
    self.RemoveRowByIndex(index)

  def __iter__(self):
    """Declare that this class supports iteration (over rows)."""
    return self._rows.__iter__()

  def Clear(self):
    """Remove all row data."""
    self._rows = []

  def GetNumRows(self):
    """Return the number of rows in the table."""
    return len(self._rows)

  def GetNumColumns(self):
    """Return the number of columns in the table."""
    return len(self._columns)

  def GetRowByIndex(self, index):
    """Access one or more rows by index or slice.

    If more than one row is returned they will be contained in a list."""
    return self._rows[index]

  def GetRowsByValue(self, values):
    """Return list of rows that match key/value pairs in |values|."""
    def grep(row):
      for key in values:
        if values[key] != row.get(key, None):
          return False
      return True

    return [r for r in self._rows if grep(r)]

  def _PrepareValuesForAdd(self, values):
    """Prepare a |values| dict to be added as a row.

    Verify that only supported column values are included.
    Add empty string values for columns not seen in the row.
    """
    for col in values:
      if not col in self._column_set:
        raise LookupError("Tried adding data to unknown column '%s'" % col)

    for col in self._columns:
      if not col in values:
        values[col] = ""

  def AppendRow(self, values):
    """Add a single row of data to the table, according to |values| dict."""
    self._PrepareValuesForAdd(values)
    self._rows.append(values)

  def SetRowByIndex(self, index, values):
    """Replace the row at |index| with values from |values| dict."""
    self._PrepareValuesForAdd(values)
    self._rows[index] = values;

  def SetRowByValue(self, id_column, values):
    """Set the |values| of the row identified by the value in |id_column|.

    The column specified by |id_column| should be a unique identifier column,
    where the value in that column is different for every row.  If it is not
    unique, the behavior of this method is undefined.
    """
    self._PrepareValuesForAdd(values)
    raise NotImplementedError("Implementation not done, yet.")

  def RemoveRowByIndex(self, index):
    """Remove the row at |index|."""
    del self._rows[index]

  def RemoveRowsByValue(self, column, value):
    """Remove any rows with |value| in the |column| column."""
    raise NotImplementedError("Implementation not done, yet.")

  def Sort(self, key, reverse=False):
    """Sort the rows using the given |key| function."""
    self._rows.sort(key=key, reverse=reverse)

  def WriteCSV(self, filehandle, hiddencols=None):
    """Write this table out as comma-separated values to |filehandle|.

    To skip certain columns during the write, use the |hiddencols| set.
    """
    def colfilter(col):
      return not hiddencols or col not in hiddencols

    cols = [col for col in self._columns if colfilter(col)]
    filehandle.write(','.join(cols) + '\n')
    for row in self._rows:
      vals = [row.get(col, "") for col in self._columns if colfilter(col)]
      filehandle.write(','.join(vals) + '\n')

# Support having this module test itself if run as __main__, by leveraging
# the table_unittest module.
if __name__ == "__main__":
  import table_unittest
  table_unittest.unittest.main(table_unittest)
