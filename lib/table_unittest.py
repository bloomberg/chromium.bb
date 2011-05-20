#!/usr/bin/python2.6
# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for the table module."""

import unittest

import table

class TableTest(unittest.TestCase):
  """Unit tests for the Table class."""

  COL0 = 'Column1'
  COL1 = 'Column2'
  COL2 = 'Column3'
  COL3 = 'Column4'
  COLUMNS=[COL0, COL1, COL2, COL3]

  ROW0 = {COL0: 'Xyz', COL1: 'Bcd', COL2: 'Cde'}
  ROW1 = {COL0: 'Abc', COL1: 'Bcd', COL2: 'Opq', COL3: 'Foo'}
  ROW2 = {COL0: 'Abc', COL1: 'Nop', COL2: 'Wxy', COL3: 'Bar'}

  EXTRAROW = {COL1: 'Walk', COL2: 'The', COL3: 'Line'}

  def setUp(self):
    self._table = table.Table(self.COLUMNS)
    self._table.AppendRow(self.ROW0)
    self._table.AppendRow(self.ROW1)
    self._table.AppendRow(self.ROW2)

  def testLen(self):
    self.assertEquals(3, len(self._table))

  def testGetNumRows(self):
    self.assertEquals(3, self._table.GetNumRows())

  def testGetNumColumns(self):
    self.assertEquals(4, self._table.GetNumColumns())

  def testGetByIndex(self):
    self.assertEquals(self.ROW0, self._table.GetRowByIndex(0))
    self.assertEquals(self.ROW0, self._table[0])

    self.assertEquals(self.ROW2, self._table.GetRowByIndex(2))
    self.assertEquals(self.ROW2, self._table[2])

  def testSlice(self):
    self.assertEquals([self.ROW0, self.ROW1], self._table[0:2])
    self.assertEquals([self.ROW2], self._table[-1:])

  def testGetByValue(self):
    rows = self._table.GetRowsByValue({self.COL0: 'Abc'})
    self.assertEquals([self.ROW1, self.ROW2], rows)
    rows = self._table.GetRowsByValue({self.COL2: 'Opq'})
    self.assertEquals([self.ROW1], rows)
    rows = self._table.GetRowsByValue({self.COL3: 'Foo'})
    self.assertEquals([self.ROW1], rows)

  def testAppendRow(self):
    self._table.AppendRow(self.EXTRAROW)
    self.assertEquals(4, self._table.GetNumRows())
    self.assertEquals(self.EXTRAROW, self._table[len(self._table) - 1])

  def testSetRowByIndex(self):
    self._table.SetRowByIndex(1, self.EXTRAROW)
    self.assertEquals(3, self._table.GetNumRows())
    self.assertEquals(self.EXTRAROW, self._table[1])

  def testRemoveRowByIndex(self):
    self._table.RemoveRowByIndex(1)
    self.assertEquals(2, self._table.GetNumRows())
    self.assertEquals(self.ROW2, self._table[1])

  def testRemoveRowBySlice(self):
    del self._table[0:2]
    self.assertEquals(1, self._table.GetNumRows())
    self.assertEquals(self.ROW2, self._table[0])

  def testIteration(self):
    ix = 0
    for row in self._table:
      self.assertEquals(row, self._table[ix])
      ix += 1

  def testClear(self):
    self._table.Clear()
    self.assertEquals(0, len(self._table))

  def testSort1(self):
    self.assertEquals(self.ROW0, self._table[0])
    self.assertEquals(self.ROW1, self._table[1])
    self.assertEquals(self.ROW2, self._table[2])

    # Sort by COL3
    self._table.Sort(lambda row : row[self.COL3])

    self.assertEquals(3, len(self._table))
    self.assertEquals(self.ROW0, self._table[0])
    self.assertEquals(self.ROW2, self._table[1])
    self.assertEquals(self.ROW1, self._table[2])

    # Reverse sort by COL3
    self._table.Sort(lambda row : row[self.COL3], reverse=True)

    self.assertEquals(3, len(self._table))
    self.assertEquals(self.ROW1, self._table[0])
    self.assertEquals(self.ROW2, self._table[1])
    self.assertEquals(self.ROW0, self._table[2])

  def testSort2(self):
    """Test multiple key sort."""
    self.assertEquals(self.ROW0, self._table[0])
    self.assertEquals(self.ROW1, self._table[1])
    self.assertEquals(self.ROW2, self._table[2])

    # Sort by COL0 then COL1
    def sorter(row):
      return (row[self.COL0], row[self.COL1])
    self._table.Sort(sorter)

    self.assertEquals(3, len(self._table))
    self.assertEquals(self.ROW1, self._table[0])
    self.assertEquals(self.ROW2, self._table[1])
    self.assertEquals(self.ROW0, self._table[2])

    # Reverse the sort
    self._table.Sort(sorter, reverse=True)

    self.assertEquals(3, len(self._table))
    self.assertEquals(self.ROW0, self._table[0])
    self.assertEquals(self.ROW2, self._table[1])
    self.assertEquals(self.ROW1, self._table[2])

if __name__ == "__main__":
  unittest.main()
